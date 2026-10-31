#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vt;

static constexpr int maxSampleSeconds = 30;
static constexpr int chunk = 512;

VocalTuneProcessor::VocalTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      state (*this, nullptr, "PARAMS", createLayout())
{
    formats.registerBasicFormats();
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            raw[r->getParameterID().toStdString()] = state.getRawParameterValue (r->getParameterID());
    startTimer (2000);
}

VocalTuneProcessor::~VocalTuneProcessor() { stopTimer(); }

bool VocalTuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    const auto in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void VocalTuneProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    fx.prepare (sampleRate);
    latency = engine.getLatency();
    setLatencySamples (latency);
    for (auto* b : { &monoIn, &monoSide, &monoOut }) b->setSize (1, chunk);
    wet.setSize (2, chunk);
    dryDelay.setSize (2, latency + 1);
    dryDelay.clear();
    dryPos = 0;
    outGain = juce::Decibels::decibelsToGain (p (id::output));
    mixSm = p (id::mix);
}

void VocalTuneProcessor::readSettings (EngineSettings& e, FxSettings& f) const
{
    e.pitchSemis = p (id::pitch) + p (id::fine) * 0.01f;
    e.formantSemis = p (id::formant);
    e.whisper = p (id::whisper);
    e.wobble = p (id::wobble);
    e.wobbleRate = p (id::wobbleRate);
    e.tune = p (id::tune);
    e.tuneSpeedMs = p (id::tuneSpeed);
    e.key = juce::roundToInt (p (id::key));
    e.scaleMask = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, juce::roundToInt (p (id::scale)))].mask;
    e.robot = p (id::robot);
    e.robotNote = p (id::robotNote);
    e.talk = p (id::talk);
    e.talkOctave = juce::roundToInt (p (id::talkOctave));
    e.bands = juce::roundToInt (p (id::bands));
    e.character = p (id::character);
    e.sibilance = p (id::sibilance);
    e.carrier.type = juce::roundToInt (p (id::carrier));
    e.carrier.chord = juce::roundToInt (p (id::chord));
    e.carrier.follow = p (id::follow) > 0.5f;
    e.carrier.sampleTune = p (id::sampleTune);
    e.carrier.sampleMode = juce::roundToInt (p (id::sampleMode));
    e.carrier.sampleStart = p (id::sampleStart);

    f.buzz = p (id::buzz); f.buzzHz = p (id::buzzHz);
    f.crush = p (id::crush); f.decimate = p (id::decimate);
    f.grit = p (id::grit); f.tone = p (id::tone);
    f.metal = p (id::metal); f.metalHz = p (id::metalHz);
    f.radio = p (id::radio); f.choir = p (id::choir);
    f.echo = p (id::echo); f.echoTime = p (id::echoTime); f.echoFeedback = p (id::echoFeedback);
    f.space = p (id::space); f.spaceSize = p (id::spaceSize);
}

void VocalTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // pick up a newly loaded sample without ever waiting on the GUI
    if (sampleChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked()) { audioSample = pendingSample; sampleChanged = false; }
    }

    EngineSettings es; FxSettings fs;
    readSettings (es, fs);

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const bool hasSide = getBusCount (true) > 1 && getBus (true, 1)->isEnabled() && getChannelCountOfBus (true, 1) > 0;
    juce::AudioBuffer<float> side;
    if (hasSide) side = getBusBuffer (buffer, true, 1);

    const int inCh = mainIn.getNumChannels();
    const int outCh = mainOut.getNumChannels();
    const int total = buffer.getNumSamples();
    const float targetGain = juce::Decibels::decibelsToGain (p (id::output));
    const float targetMix = p (id::mix);
    const float sk = 1.0f - std::exp (-1.0f / (0.02f * (float) getSampleRate()));

    for (int start = 0; start < total; start += chunk)
    {
        const int num = std::min (chunk, total - start);
        auto* mi = monoIn.getWritePointer (0);
        for (int i = 0; i < num; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < inCh; ++c) s += mainIn.getReadPointer (c)[start + i];
            mi[i] = inCh > 0 ? s / (float) inCh : 0.0f;
        }
        const float* sidePtr = nullptr;
        if (hasSide && side.getNumChannels() > 0)
        {
            auto* ms = monoSide.getWritePointer (0);
            for (int i = 0; i < num; ++i)
            {
                float s = 0.0f;
                for (int c = 0; c < side.getNumChannels(); ++c) s += side.getReadPointer (c)[start + i];
                ms[i] = s / (float) side.getNumChannels();
            }
            sidePtr = ms;
        }

        engine.process (mi, sidePtr, monoOut.getWritePointer (0), num, es, audioSample.get());
        fx.process (monoOut.getReadPointer (0), wet.getWritePointer (0), wet.getWritePointer (1), num, fs);

        const int dlen = dryDelay.getNumSamples();
        for (int i = 0; i < num; ++i)
        {
            mixSm += (targetMix - mixSm) * sk;
            outGain += (targetGain - outGain) * sk;
            const float inL = inCh > 0 ? mainIn.getReadPointer (0)[start + i] : 0.0f;
            const float inR = inCh > 1 ? mainIn.getReadPointer (1)[start + i] : inL;
            // dry signal delayed by the engine's latency so the two line up
            const float dL = dryDelay.getSample (0, dryPos), dR = dryDelay.getSample (1, dryPos);
            dryDelay.setSample (0, dryPos, inL);
            dryDelay.setSample (1, dryPos, inR);
            dryPos = (dryPos + 1) % dlen;

            float L = (dL * (1.0f - mixSm) + wet.getSample (0, i) * mixSm) * outGain;
            float R = (dR * (1.0f - mixSm) + wet.getSample (1, i) * mixSm) * outGain;
            if (! std::isfinite (L) || ! std::isfinite (R)) { L = R = 0.0f; engine.reset(); fx.reset(); }
            if (outCh == 1) mainOut.getWritePointer (0)[start + i] = 0.5f * (L + R);
            else
            {
                mainOut.getWritePointer (0)[start + i] = L;
                mainOut.getWritePointer (1)[start + i] = R;
            }
        }
    }
    for (int c = 2; c < outCh; ++c) mainOut.clear (c, 0, total);
}

// ====================================================================== samples
float VocalTuneProcessor::detectRoot (const juce::AudioBuffer<float>& mono, double rate)
{
    const int order = rate > 60000.0 ? 12 : 11;
    const int n = 1 << order;
    if (mono.getNumSamples() < n) return 0.0f;
    PitchDetector det;
    det.prepare (rate, order);
    det.setRange (40.0f, 1500.0f);
    std::vector<float> found;
    int frames = 0;
    for (int pos = 0; pos + n <= mono.getNumSamples() && frames < 400; pos += n / 2, ++frames)
    {
        const auto r = det.detect (mono.getReadPointer (0) + pos);
        if (r.hz > 0.0f && r.aperiodicity < 0.2f) found.push_back (r.hz);
    }
    if (frames == 0 || (float) found.size() < 0.25f * (float) frames) return 0.0f;
    std::nth_element (found.begin(), found.begin() + (long) found.size() / 2, found.end());
    return found[found.size() / 2];
}

void VocalTuneProcessor::loadSampleBuffer (const juce::AudioBuffer<float>& audio, double rate, const juce::String& name)
{
    SampleData::Ptr s = new SampleData();
    const int len = std::min (audio.getNumSamples(), (int) (rate * maxSampleSeconds));
    s->audio.setSize (1, len);
    s->audio.clear();
    for (int c = 0; c < audio.getNumChannels(); ++c)
        s->audio.addFrom (0, 0, audio, c, 0, len, 1.0f / (float) audio.getNumChannels());
    // normalise to a healthy level
    const float mag = s->audio.getMagnitude (0, 0, len);
    if (mag > 1.0e-6f) s->audio.applyGain (0.9f / mag);
    s->sampleRate = rate;
    s->name = name;
    s->rootHz = detectRoot (s->audio, rate);
    const int cols = 300;
    s->peaks.assign (cols, 0.0f);
    for (int c = 0; c < cols && len > 0; ++c)
    {
        const int a = (int) ((int64_t) c * len / cols), b = std::max (a + 1, (int) ((int64_t) (c + 1) * len / cols));
        s->peaks[(size_t) c] = s->audio.getMagnitude (0, a, std::min (b, len) - a);
    }
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        if (pendingSample != nullptr) retired.add (pendingSample);
        pendingSample = s;
        sampleChanged = true;
    }
    if (onPresetChanged) onPresetChanged();
}

juce::String VocalTuneProcessor::loadSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return "Couldn't read " + file.getFileName() + " (try WAV, AIFF, FLAC, MP3 or OGG)";
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * maxSampleSeconds));
    if (len < 64) return file.getFileName() + " is empty";
    juce::AudioBuffer<float> buf ((int) std::max (1u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, true);
    loadSampleBuffer (buf, reader->sampleRate, file.getFileNameWithoutExtension());
    return {};
}

void VocalTuneProcessor::clearSample()
{
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        if (pendingSample != nullptr) retired.add (pendingSample);
        pendingSample = nullptr;
        sampleChanged = true;
    }
    if (onPresetChanged) onPresetChanged();
}

vt::SampleData::Ptr VocalTuneProcessor::getSample() const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return pendingSample;
}

void VocalTuneProcessor::timerCallback()
{
    // free old samples here, on the message thread, once the audio thread has let go of them
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    if (sampleChanged.load()) return;
    for (int i = retired.size(); --i >= 0;)
        if (retired[i]->getReferenceCount() == 1)
            retired.remove (i);
}

// ====================================================================== state
std::unique_ptr<juce::XmlElement> VocalTuneProcessor::makeStateXml (bool includeSample)
{
    auto xml = std::make_unique<juce::XmlElement> ("VocalTune");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("preset", presetName);
    xml->setAttribute ("presetIndex", presetIndex);
    xml->setAttribute ("editorScale", editorScale);
    xml->addChildElement (state.copyState().createXml().release());

    if (auto s = getSample(); includeSample && s != nullptr)
    {
        juce::MemoryBlock flac;
        {
            juce::FlacAudioFormat fmt;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::MemoryOutputStream> (flac, false);
            auto writer = fmt.createWriterFor (os, juce::AudioFormatWriterOptions{}
                                                     .withSampleRate (s->sampleRate)
                                                     .withNumChannels (1)
                                                     .withBitsPerSample (16));
            if (writer != nullptr) writer->writeFromAudioSampleBuffer (s->audio, 0, s->length());
        }
        auto* e = xml->createNewChildElement ("Sample");
        e->setAttribute ("name", s->name);
        e->setAttribute ("rate", s->sampleRate);
        e->setAttribute ("root", s->rootHz);
        e->addTextElement (flac.toBase64Encoding());
    }
    return xml;
}

void VocalTuneProcessor::restoreFromXml (const juce::XmlElement& xml, bool keepSampleIfMissing)
{
    if (auto* params = xml.getChildByName (state.state.getType()))
        state.replaceState (juce::ValueTree::fromXml (*params));
    presetName = xml.getStringAttribute ("preset", presetName);
    presetIndex = xml.getIntAttribute ("presetIndex", -1);
    editorScale = (float) xml.getDoubleAttribute ("editorScale", editorScale);

    if (auto* e = xml.getChildByName ("Sample"))
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (e->getAllSubText().trim()))
        {
            juce::FlacAudioFormat fmt;
            std::unique_ptr<juce::AudioFormatReader> r (fmt.createReaderFor (new juce::MemoryInputStream (mb, false), true));
            if (r != nullptr && r->lengthInSamples > 0)
            {
                juce::AudioBuffer<float> buf (1, (int) r->lengthInSamples);
                r->read (&buf, 0, buf.getNumSamples(), 0, true, false);
                loadSampleBuffer (buf, r->sampleRate, e->getStringAttribute ("name", "Sample"));
            }
        }
    }
    else if (! keepSampleIfMissing)
    {
        clearSample();
    }
    if (onPresetChanged) onPresetChanged();
}

void VocalTuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    copyXmlToBinary (*makeStateXml (true), dest);
}

void VocalTuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName ("VocalTune"))
            restoreFromXml (*xml, false);
}

// ====================================================================== presets
juce::Array<VocalTuneProcessor::PresetEntry> VocalTuneProcessor::presetList() const
{
    juce::Array<PresetEntry> list;
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i) list.add ({ f[(size_t) i].name, f[(size_t) i].category, i, {} });
    for (auto& file : userPresetFiles()) list.add ({ file.getFileNameWithoutExtension(), "User", -1, file });
    return list;
}

void VocalTuneProcessor::setPresetName (const juce::String& n, int idx)
{
    presetName = n;
    presetIndex = idx;
    if (onPresetChanged) onPresetChanged();
}

void VocalTuneProcessor::applyPreset (const PresetEntry& e)
{
    const auto list = presetList();
    int idx = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].name == e.name && list[i].category == e.category) idx = i;

    if (e.factoryIndex >= 0)
    {
        applyFactory (state, factoryPresets()[(size_t) e.factoryIndex]);
        setPresetName (e.name, idx);
    }
    else if (auto xml = juce::XmlDocument::parse (e.file); xml != nullptr && xml->hasTagName ("VocalTune"))
    {
        restoreFromXml (*xml, true);
        setPresetName (e.name, idx);
    }
}

void VocalTuneProcessor::stepPreset (int delta)
{
    const auto list = presetList();
    if (list.isEmpty()) return;
    int idx = presetIndex < 0 ? (delta > 0 ? -1 : 0) : presetIndex;
    idx = ((idx + delta) % list.size() + list.size()) % list.size();
    applyPreset (list[idx]);
}

bool VocalTuneProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (name + ".vtpreset");
    auto xml = makeStateXml (true);
    xml->setAttribute ("preset", name);
    if (! xml->writeTo (file)) return false;
    const auto list = presetList();
    for (int i = 0; i < list.size(); ++i)
        if (list[i].file == file) { setPresetName (name, i); return true; }
    setPresetName (name, -1);
    return true;
}

void VocalTuneProcessor::dice()
{
    randomise (state, rng);
    if (getSample() != nullptr && rng.nextFloat() < 0.5f && p (id::talk) > 0.5f)
        if (auto* c = state.getParameter (id::carrier)) c->setValueNotifyingHost (c->convertTo0to1 ((float) carSample));
    static const char* adj[] { "Wild", "Tiny", "Fuzzy", "Cosmic", "Sneaky", "Bubbly", "Grumpy", "Sparkly", "Wobbly", "Spooky", "Rusty", "Jelly" };
    static const char* noun[] { "Gremlin", "Robot", "Goblin", "Unicorn", "Toaster", "Wizard", "Duck", "Alien", "Puppet", "Dragon", "Bean", "Kazoo" };
    setPresetName (juce::String (adj[rng.nextInt (12)]) + " " + noun[rng.nextInt (12)], -1);
}

void VocalTuneProcessor::mutateCurrent()
{
    mutate (state, rng);
    if (! presetName.endsWith ("*")) setPresetName (presetName + "*", -1);
    else setPresetName (presetName, -1);
}

juce::AudioProcessorEditor* VocalTuneProcessor::createEditor() { return new VocalTuneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalTuneProcessor(); }

VOCALTUNE 1.0 for Mac
Toy box voice lab: voice changer, talk box and tune, inside Ableton Live

VST3 and Audio Unit. Universal: runs natively on Apple Silicon and Intel Macs.
Needs macOS 11 or newer.

------------------------------------------------------------
INSTALL
------------------------------------------------------------
The easy way is "VocalTune.pkg" (a normal Mac installer). This folder is
for installing by hand:
  VocalTune.vst3       goes in  /Library/Audio/Plug-Ins/VST3
  VocalTune.component  goes in  /Library/Audio/Plug-Ins/Components
Files moved by hand keep macOS's "downloaded from the internet" mark, which
makes Live skip them; clear it in Terminal with:
  xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/VocalTune.vst3" "/Library/Audio/Plug-Ins/Components/VocalTune.component"

Then in Live: Settings (Cmd + ,) > Plug-Ins. Turn on "Use Audio Units" and
"Use VST3 Plug-in System Folders", hold Option and click Rescan. VocalTune
is under Plug-Ins > Bass Bible in Live's browser.

------------------------------------------------------------
THE TOY BOX
------------------------------------------------------------
Put VocalTune on a track with a voice: a vocal clip, a sample, or a live mic
(arm the track and set Monitor to In).

PRESETS   Click the screen for 67 presets in Classic, Talk, Sample, Tune and
          Weird. The arrows step through them.
DICE      A brand new random voice every click.
MUTATE    Nudges the current sound a little. Keep clicking to drift.
SAVE      Saves your sound (and its sample) as your own preset, in
          ~/Music/VocalTune/Presets. Share the .vtpreset files with friends.

VOICE     PITCH and FINE move the notes. THROAT moves the formants: down =
          bigger body (monster, giant), up = smaller (baby, chipmunk).
          Pitch and throat are separate, so you can make a tiny-voiced giant
          or a huge baby. WHISPER takes the pitch out. WOBBLE + SPEED add
          vibrato.

TALK BOX  Makes another sound talk with your mouth. Pick the talk sound:
            SAMPLE     drop ANY audio file on the drop zone (dog bark, guitar,
                       synth pad, car horn) or drag one in from Live's browser.
            SAW, SQUARE, SUPER, PULSE   built-in synths that sing your notes.
            NOISE      a whispery wind voice.
            SIDECHAIN  any other track in your set (see below).
          TALK        how much of the talk sound replaces your voice.
          FLATTEN     high = your mouth shapes it fully (talkbox),
                      low = the sound keeps more of its own tone.
          S-SOUNDS    adds the "s" and "t" sounds so words stay clear.
          CHORD       plays the talk sound as a chord (choirs, power chords).
          VOCODER BANDS  Smooth = talkbox; 8 to 32 bands = old-school vocoder.
          OCTAVE      moves the talk sound up or down.
          FOLLOW      on: the sample sings your notes. off: it keeps its own
                      pitch (great for textures and drums).
          SMP TUNE, START, LOOP/RETRIGGER  tune the sample, pick where it
                      starts, loop it or restart it on every new word.

TUNE & ROBOT  The screen shows the note you sing and the note that comes out.
          TUNE snaps you to KEY + SCALE; SPEED 0 ms = hard robotic snap,
          higher = natural. ROBOT flattens everything to one NOTE.
          Tune also steers the talk box, so talk + tune = instant talkbox
          hooks in key.

EFFECTS   BUZZ (ring mod), CRUSH (bits + rate), GRIT (distortion) + TONE,
          METAL (tin can), RADIO (phone/megaphone), CHOIR (stereo doubles),
          ECHO, SPACE (reverb). MIX blends in your dry voice; OUT is volume.

Every knob is a Live parameter: automate it, or MIDI-map it in MIDI Map mode.
Double-click any knob to reset it. Drag the window's corner to resize.
The loaded sample is saved inside your Live set.

SIDECHAIN (VST3): on VocalTune's device, open the sidechain section and pick
the track to use as the talk sound, then press SIDECHAIN in the talk box.

Latency: VocalTune looks ahead about 43 ms to hear your pitch. Live hides
this on playback automatically. When singing live through it, that delay
is what you hear.

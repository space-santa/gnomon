# Gnomon

A small Euclidean drum sequencer plugin. It generates MIDI notes in time with your DAW's transport, so you can drive a Drum Rack (or any instrument) with it. (Curious about the name? See [About the name](#about-the-name).)

It has four lanes (kick, snare, closed hat and open hat by default). Each lane spreads a number of hits as evenly as possible across a number of steps. For example, 3 hits over 8 steps gives `x..x..x.`.

## Building (macOS)

### Requirements

- Apple's Command Line Tools: `xcode-select --install`. Full Xcode is optional.
- CMake 3.22 or newer. Without Homebrew, download the macOS `.dmg` from [cmake.org/download](https://cmake.org/download/), drag CMake to Applications, then add this line to `~/.zshrc` and open a new terminal:

  ```sh
  export PATH="/Applications/CMake.app/Contents/bin:$PATH"
  ```

- An internet connection the first time you build. CMake downloads JUCE (about 1–2 minutes).

### Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The plugin is installed automatically after every build:

| Format | Installed to |
| --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/Gnomon.vst3` |
| AU | `~/Library/Audio/Plug-Ins/Components/Gnomon.component` |

To build without installing, add `-DGNOMON_COPY_PLUGIN=OFF` to the first command.

**After rebuilding, restart Live.** Live keeps the old version of a plugin in memory until it quits.

## Using it in Ableton Live

### Why it needs two tracks

Live only accepts its own devices and Max for Live devices in the MIDI-effect slot. Third-party plugins can only load as instruments or audio effects, and a plugin's MIDI output never reaches the next device on the same track. So Gnomon sits on one track and generates notes, and a second track listens to it and plays the sounds.

```
Track 1 "Gnomon"                     Track 2 "Drums"
┌──────────────────┐   MIDI notes    ┌──────────────────────────┐
│  Gnomon (VST3)   │ ──────────────► │  MIDI From: 1-Gnomon     │
│  (silent audio)  │                 │             Gnomon       │
└──────────────────┘                 │  Monitor:   In           │
                                     │  Drum Rack               │
                                     └──────────────────────────┘
```

**Use the VST3 version in Live.** Live can't receive MIDI from AU plugins, so the AU version won't drive another track.

### One-time setup

1. Open **Live → Settings → Plug-Ins** (called Preferences in older Live versions).
2. Turn on **Use VST3 Plug-In System Folders**.
3. Click **Rescan**.

### Setting up a set

1. **Show the routing controls.** Turn on **View → In/Out** (or press `⌘ ⌥ I`). Each track now shows "MIDI From" and "Monitor" controls.
2. **Track 1: add Gnomon.** Create a MIDI track and rename it "Gnomon". In the browser, go to **Plug-Ins → VST3 → Gnomon** and drag **Gnomon** onto the track.
3. **Track 2: add the drums.** Create a second MIDI track and drag a Drum Rack kit onto it.
4. **Route track 1 into track 2.** On track 2, set:
   - **MIDI From**, top dropdown: **1-Gnomon** (or whatever track 1 is called).
   - **MIDI From**, bottom dropdown: **Gnomon**. Pick the plugin by name, not "Pre FX" or "Post FX".
   - **Monitor**: **In**.
5. **Press play.** Gnomon only produces notes while Live's transport is running.

To change the pattern, open Gnomon's window on track 1 (the wrench icon on the device, or double-click its title bar).

### Recording a pattern as a clip

To keep a pattern you like as an ordinary MIDI clip:

1. Arm track 2 for recording.
2. Record a few bars.
3. Set track 2's **MIDI From** back to **No Input** (or turn Gnomon off). Otherwise the clip and Gnomon play at the same time and every note doubles.

## Controls

| Control | Range | What it does |
| --- | --- | --- |
| Rate | 1/4, 1/8, 1/16, 1/32 | Length of one step, shared by all lanes. Default 1/16. |
| *Lane* steps | 1–32 | Length of the lane's pattern. |
| *Lane* pulses | 0–32 | How many hits are spread across those steps. Capped at the step count. |
| *Lane* rotate | 0–31 | Moves the pattern later by this many steps. Wraps around. |
| *Lane* note | C-2–G8 | The MIDI note this lane plays. |
| *Lane* velocity | 1–127 | How hard each hit is. |

Notes are half a step long. Drum Rack one-shots ignore note length, but pitched instruments won't.

### Default lanes

| Lane | Note | Pattern |
| --- | --- | --- |
| Kick | C1 (36) | 16 steps, 4 pulses: four on the floor |
| Snare | D1 (38) | 16 steps, 2 pulses, rotate 4: beats 2 and 4 |
| Closed hat | F#1 (42) | 16 steps, 8 pulses, rotate 1: the "e" and "a" sixteenths between the eighth notes |
| Open hat | A#1 (46) | 16 steps, 4 pulses, rotate 2: offbeat eighths |

Note names use Live's convention, where middle C (MIDI 60) is C3, so they match the names on Drum Rack pads. Most Ableton kits put kick, snare and hats on these notes, but not all. If a lane triggers the wrong sound, click the pad you want in the Drum Rack, read its note name, and set the lane's **note** to match.

### Pattern timing

Steps are counted from the start of the song, not from when you pressed play. This keeps patterns locked to the bar grid, so starting playback from bar 9 gives the same pattern as playing through from bar 1.

Lanes with step counts that don't divide evenly into a bar (5, 7, 13 and so on) drift against the beat and against each other. That's how you get polymetric grooves.

## Troubleshooting

**Gnomon isn't in the browser**
- Check that `~/Library/Audio/Plug-Ins/VST3/Gnomon.vst3` exists. If not, rebuild without `-DGNOMON_COPY_PLUGIN=OFF`.
- Check that **Use VST3 Plug-In System Folders** is on. Then rescan; hold `⌥` while clicking **Rescan** to force a full rescan.

**"Gnomon" isn't in track 2's bottom MIDI From dropdown**
- Check that track 1 has the **VST3** version loaded, not the AU.
- Check that the top dropdown is set to track 1.

**Nothing plays**
- Is Live's transport running?
- Is track 2's Monitor set to **In**?
- Does the Drum Rack have samples on the pads Gnomon is playing? See [Default lanes](#default-lanes).

**Notes are doubled or flamming**
- Track 2 is probably also playing a clip, or two tracks are routed from Gnomon.

**The plugin didn't change after rebuilding**
- Restart Live.

## Windows

Every push to GitHub builds a Windows VST3 (see `.github/workflows/build.yml`).

1. Download the **Gnomon-Windows** artifact from the workflow run.
2. Copy `Gnomon.vst3` into `C:\Program Files\Common Files\VST3`.
3. In Live, open **Options → Preferences → Plug-Ins**, then follow the same steps as on macOS.

## Project layout

| Path | Contents |
| --- | --- |
| `CMakeLists.txt` | Build setup: plugin formats, IDs, JUCE version |
| `Source/PluginProcessor.cpp` | Sequencer: parameters, transport sync, note timing |
| `Source/Euclid.h` | The Euclidean pattern function |
| `.github/workflows/build.yml` | macOS (universal) and Windows builds on GitHub |

## About the name

A **gnomon** (Greek for "one who knows" or "indicator", pronounced NO-mon) has two lives, and both fit this plugin.

**It tells time.** On a sundial, the gnomon is the upright blade that casts the shadow. As the sun moves, the shadow sweeps around the dial and marks the hours. According to the historian Herodotus, the Greeks learned to use the gnomon from the Babylonians. A step sequencer works the same way: a playhead sweeps around a loop of steps, and the steps it lands on make the rhythm.

**It's a shape in Euclid's geometry.** In Book II of the *Elements*, Euclid defines the gnomon as the L-shaped figure that's left when you cut a smaller copy of a parallelogram out of one corner of a larger one. Later Greek mathematicians described the gnomon as whatever you add to a shape to make a bigger version of that same shape. The Pythagoreans used it with numbers. Wrap a square of dots in an L of the next odd number of dots, and you get the next square:

```
● ● ● ○
● ● ● ○      9 dots (3 × 3)
● ● ● ○    + 7 dots (the gnomon, ○)
○ ○ ○ ○    = 16 dots (4 × 4)
```

**Euclid also supplies the rhythms.** The patterns Gnomon plays are called Euclidean rhythms because they come from Euclid's algorithm. It's his method for finding the greatest common divisor of two numbers, from Book VII of the same book. In 2005, the computer scientist Godfried Toussaint showed that running this algorithm on the number of hits and the number of steps spreads the hits as evenly as possible. Those even spreads turn out to be rhythms that musical traditions around the world arrived at independently. For example, 3 hits over 8 steps (`x..x..x.`) is the Cuban *tresillo*, a figure heard throughout Latin American and West African music.

So the name ties together a word from the book that gave us the algorithm and an ancient tool for keeping time.

## License

Gnomon is free software: you can redistribute it and/or modify it under the terms of the [GNU Affero General Public License](LICENSE), version 3 or (at your option) any later version.

In short, you can use, study, change and share Gnomon. If you share a version (modified or not), you must make its full source code available under the same license.

Gnomon includes third-party code under these licenses:

| Component | License |
| --- | --- |
| [JUCE](https://juce.com) | [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.html) (JUCE is also available under a commercial license, which Gnomon doesn't use) |
| [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) (bundled with JUCE) | [MIT](https://opensource.org/license/mit) |

VST is a registered trademark of Steinberg Media Technologies GmbH.

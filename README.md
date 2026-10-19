# GDPredictiveInputModGeode

A Geode mod for Geometry Dash 2.2091 that saves you from clicking a hair too early.

**Built on experimental Geode v6 branch because current geode release doesn't support 2.2091**

Every time you click, the mod quickly plays out what would happen. If clicking right now would kill you but clicking a tick or two later wouldn't, it holds your click for those few ticks and lets it through at the right moment. If no small delay would save you, it doesn't touch anything and you just die like normal (skill issue).

This is basically just baby aim assist for GD. Haven't done any extensive testing. If you find something wonky with it, please create an **issue** and please attach a **clip** to your issue.

## Modes

Press **F6** in a level to cycle through them (you can rebind it in the mod settings):

| Mode | Max delay |
|---|---|
| Disabled | nothing, your inputs go straight through |
| 1 Tick | 1 tick |
| 2 Ticks | 2 ticks |
| 4 Ticks | 4 ticks |
| 8 Ticks | 8 ticks |

GD runs 240 ticks a second no matter your FPS, so 4 ticks is one frame at 60fps and 8 ticks is two.

The current mode shows up in the top left while you're playing, along with the last click it saved.

## Gamemodes

- **Cube, robot, ball, spider** - checks your jumps and orb clicks
- **UFO** - checks your clicks
- **Wave** - checks both presses and releases
- **Ship, swing** - left alone, your inputs always go straight through

## Settings

- **Buffer Mode** - same as the F6 modes above
- **Buffer Releases (Wave)** - turn this off if you only want presses touched
- **Show Status Label** - hides the top-left text
- **Log Saved Inputs** - prints a line to the Geode console every time a click gets saved
- **Lookahead settings** - how far ahead it's allowed to check. The defaults are fine

## Installing

You need GD **2.2091** with the Geode **v6** loader, since that's the only Geode that runs on 2.2091 right now. There's no prebuilt download yet, so build it (below) and drop the `.geode` file in `Geometry Dash/geode/mods`.

### Building

Needs: MSVC (Visual Studio Build Tools with the C++ x64 tools), CMake 3.21+, Ninja, Git, Geode CLI, Geode SDK (v6 branch), Geode bindings (with 2.2091)

The VS Build Tools "C++ CMake tools for Windows" component gets you CMake and Ninja. Point the paths at the top of `build.bat` at your stuff and run it. It builds and installs the mod for you.

## Heads up

- Turn off Click Between Frames if you have it, both mess with input timing.
- Stuff that moves around (move triggers and such) isn't predicted perfectly, so the odd weird call can happen there.
- Inputs near gamemode, speed, mirror or teleport portals are never delayed.
- This is basically an input assist, so runs with it on don't count for leaderboards or list submissions.

## License

GPL-3.0, see [LICENSE](LICENSE). Do whatever you want with it, just keep it open source.

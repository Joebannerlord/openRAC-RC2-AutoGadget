# Auto Gadgets for Going Commando

Press Circle at a compatible target to use an owned gadget from your current loadout. The mod briefly equips the gadget, uses the game's own targeting and activation, and restores your previous equipment.

Supports **OpenRAC 1.1.0 on Windows and Linux x86-64** with **Ratchet & Clank: Going Commando USA v1.01** (`SCUS-97268`). The plugin uses OpenRAC mod API 1.14.

## Install

1. Select **Going Commando** in the OpenRAC launcher.
2. Open **Mods → Add from a link** and paste this repository address.

   ```text
   https://github.com/Joebannerlord/openRAC-RC2-AutoGadget
   ```

3. Select **Install**, enable Auto Gadgets, and place it first among enabled Going Commando mods. This lets it read physical Circle before Contextual Wrench and Fire maps R2 to Circle.
4. Start the game. To change how long the Thermanator stays equipped, open **Auto Gadgets** in the **Mods** tab and set **Thermanator return delay** to 2–5 seconds. It defaults to 3 seconds. Changes apply on the next game launch.

To install from a ZIP, download the archive for your platform from **Releases** and select it in **Mods → Add a mod**. Disable Auto Gadgets in the Mods tab to return to stock controls. [Releases](https://github.com/Joebannerlord/openRAC-RC2-AutoGadget/releases) · [OpenRAC installation guide](https://openrac.dev/mods/docs/install-from-link)

## Controls

| Gadget | How to use it |
| --- | --- |
| **Dynamo** | Tap **Circle** at an eligible activator to fire once. |
| **Tractor Beam** | Hold **Circle** until it connects. Release Circle to move the camera while keeping the link. Press **Circle** again, jump, press **Square**, or press **R2** to drop the object. With Contextual Wrench and Fire enabled, hold R2 through the switch to fire the restored weapon. |
| **Thermanator** | Aim at a freezable pool or ice and hold **Circle**. Release to stop firing. The gadget stays equipped for the configured delay so you can read the meter. Press Circle again at a ready target to resume during the delay. |
| **Hypnomatic** | Aim at a Receiver Bot and hold **Circle** until capture starts. The receiver must stay in the aim cone with a valid sightline until capture. Possession uses stock controls. Your previous item returns when Ratchet regains control. |

Circle works normally away from eligible targets. With a weapon drawn, R2 takes priority when pressed with Circle. Quick Select overrides a pending switch or restore. Swingshot uses manual equip, and Electrolyzer and Infiltrator puzzles still use **Triangle**.

### Level coverage

| Gadget | Levels with auto-use |
| --- | --- |
| Dynamo | Oozla, Endako, Notak, Dobbo, Joba, Snivelak, Smolg, Yeedil |
| Tractor Beam | Oozla, Maktar Nebula, Siberius, Todano, Yeedil |
| Thermanator | Barlow, Notak, Tabora, Damosel, Yeedil |
| Hypnomatic | Damosel, Grelbin, Yeedil |

## Build and test

Use CMake 3.24 or newer and Python 3. On Windows, build from an x64 Visual Studio developer prompt with MSVC. On Linux, use a C++17 compiler and Ninja. The fetcher checks the SDK download against [sdk.lock](sdk.lock).

```powershell
python tools/fetch_sdk.py
$wrenchPlugin = python tools/fetch_wrench_test.py windows-x86_64
cmake -S . -B build "-DWRENCH_TEST_PLUGIN:FILEPATH=$wrenchPlugin"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target release_zip
```

```sh
python3 tools/fetch_sdk.py
wrench_plugin=$(python3 tools/fetch_wrench_test.py linux-x86_64)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DWRENCH_TEST_PLUGIN:FILEPATH=$wrench_plugin"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target release_zip
```

The Wrench plugin is an optional dependency for the offline tests. You can omit its fetch and CMake setting to build Auto Gadgets by itself. The tests use simulated game state, and the SDK writes the platform ZIP to `build/dist/`.

## Credits and license

- **Joebannerlord** maintains the mod and verified the gadget behavior in play. **Codex** assisted with development and verification.
- **glawks** created Auto Swingshot, which the temporary equip and Circle replay flow was adapted from. [Auto Swingshot](https://github.com/glawks/auto-swingshot) · [MIT notice](LICENSE-AUTO-SWINGSHOT)
- **OpenRAC contributors** provide the SDK, build template, and JSON reader used here. [OpenRAC](https://github.com/OpenRAC/OpenRAC) · [SDK MIT notice](LICENSE-OPENRAC-SDK)
- Native behavior was cross-checked against the disc and the ReRAC2 project. The pad helpers were adapted from Contextual Wrench and Fire. [ReRAC2](https://github.com/re-rac/rerac2) · [Contextual Wrench and Fire](https://github.com/Joebannerlord/openRAC-RC2-contextual-wrench-throw)

Auto Gadgets is licensed under [GPL-3.0-or-later](LICENSE).

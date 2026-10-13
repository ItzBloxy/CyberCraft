# AGENTS.md

Guidance for AI coding agents (Claude Code, Codex, Cursor and the like) that help someone **install**, **troubleshoot**
or **develop** CyberCraft. Humans can follow it too: it's the README's install guide with a check after each step.

CyberCraft plays Minecraft inside Cyberpunk 2077. Two halves talk over shared memory:
- **The game side:** BlockRender, a RED4ext plugin, with its redscript file and a Cyber Engine Tweaks (CET) overlay.
- **The Minecraft side:** SkyCraft's unchanged Fabric mod (0.1.2), plus CyberCraft Tweaks, a small Fabric mod.

See [README.md](README.md) for what it does and the user-facing guide.

## Ground rules

- **The user runs the games.** Don't start, stop or kill Cyberpunk 2077 or Minecraft yourself. Ask the user, and wait
  for them to say it's done. A killed Minecraft loses everything since its last autosave (every 5 minutes).
- **Check whether the games are running before copying files.** The game locks `BlockRender.dll` while it runs. Check
  with `Get-Process Cyberpunk2077, javaw -ErrorAction SilentlyContinue`.
- **Never touch the user's saves:** Cyberpunk's (`%USERPROFILE%\Saved Games\CD Projekt Red\Cyberpunk 2077`) or
  Minecraft's (`<instance>\saves`).
- **Ask before deleting anything.** That includes older mod versions, the instance's other mods and resource packs.
  Rename a jar to `.jar.disabled` instead of deleting it.
- **Don't run the `mclink` scripts** (`spikes/block-render/tools/mclink/*.txt`) against the user's Minecraft unless
  they ask. Those scripts summon mobs, set off TNT and change the user's world, game mode and inventory.
- **Download only from the sources listed here:** this repo's releases, SkyCraft's GitHub releases, Nexus Mods pages
  for the game's frameworks, and Modrinth for Fabric API. Tell the user what you'll download, and from where, before
  you do it.
- **If something doesn't match** (another game version, a different SkyCraft version), stop and tell the user. Don't
  patch around it.

## Install checklist

Work through the steps in order. Each has a **check**: run it and confirm the result before moving on. The commands
are PowerShell. `$game` is the Cyberpunk 2077 folder, the one holding `bin` and `r6`.

### 0. Find the game folder and check its version

Typical places:

| Store | Folder |
|---|---|
| Steam | `C:\Program Files (x86)\Steam\steamapps\common\Cyberpunk 2077` (other libraries: `steamapps\libraryfolders.vdf`) |
| GOG | `C:\Program Files (x86)\GOG Galaxy\Games\Cyberpunk 2077` |
| Epic | `C:\Program Files\Epic Games\Cyberpunk2077` |

Ask the user if it's in none of these.

```powershell
$game = "E:\Cyberpunk2077"   # the user's folder
(Get-Item "$game\bin\x64\Cyberpunk2077.exe").VersionInfo | Select-Object ProductVersion, FileVersion
```

**Check:** `ProductVersion` is `2.31`. CyberCraft was tested on the Epic build, `FileVersion` `3.0.5294808`.
BlockRender is tied to the game's code, so with another version, stop: tell the user it's unsupported, and look for a
matching CyberCraft release.

### 1. The game's frameworks

The user installs these with their mod manager (Vortex or MO2) or by hand, from Nexus Mods:
[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380),
[redscript](https://www.nexusmods.com/cyberpunk2077/mods/1511),
[Cyber Engine Tweaks](https://www.nexusmods.com/cyberpunk2077/mods/107) and
[Codeware](https://www.nexusmods.com/cyberpunk2077/mods/7780). Nexus needs the user's account, so let them download.

```powershell
"red4ext\RED4ext.dll", "bin\x64\winmm.dll", "engine\tools\scc.exe",
"bin\x64\plugins\cyber_engine_tweaks.asi", "red4ext\plugins\Codeware\Codeware.dll" |
  ForEach-Object { "{0,-45} {1}" -f $_, (Test-Path "$game\$_") }
```

**Check:** all five are `True`. Each framework writes a log once the game has started with it. Ask the user to start
the game to the main menu once and quit, then:

```powershell
Get-ChildItem "$game\red4ext\logs\red4ext-*.log" | Sort-Object LastWriteTime | Select-Object -Last 1 |
  Get-Content | Select-String "has been loaded"
```

**Check:** Codeware is among the loaded plugins. When CET first starts, it asks for an overlay key: suggest
**Shift + F6**, which BlockRender handles in build mode.

### 2. CyberCraft's game files

Download `CyberCraft-<version>.zip` from <https://github.com/ItzBloxy/CyberCraft/releases>. If the user has Vortex,
let them install it there. Otherwise, with the game closed, extract it into `$game`:

```powershell
Expand-Archive "$env:USERPROFILE\Downloads\CyberCraft-<version>.zip" -DestinationPath $game -Force
```

**Check:**

```powershell
"red4ext\plugins\BlockRender\BlockRender.dll", "red4ext\plugins\BlockRender\Scripts\BlockRender.reds",
"red4ext\plugins\BlockRender\shaders\blocks.hlsl", "red4ext\plugins\BlockRender\make_atlas.bat",
"bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\init.lua" |
  ForEach-Object { "{0,-70} {1}" -f $_, (Test-Path "$game\$_") }
```

### 3. The block atlas

Minecraft 26.3 must have been started once in a launcher, so its client jar exists. `make_atlas.bat` looks in
`%APPDATA%\.minecraft\versions\26.3\26.3.jar`, then in `%APPDATA%\ModrinthApp\meta\versions\26.3-*\`. For any other
launcher, find the 26.3 client jar (a jar of about 40 MB with `assets/minecraft/textures/block/` inside) and pass its
path. It needs `uv` or Python 3 on PATH.

```powershell
cmd /c "`"$game\red4ext\plugins\BlockRender\make_atlas.bat`" `"<path to 26.3.jar>`""
```

With no argument it searches by itself, but then it waits for a key press at the end, so pass the path when running it
yourself. **Check:** it prints `atlas: 256 x 64, 62 sprites, 50 blocks` and `$game\red4ext\plugins\BlockRender\atlas.bin`
exists (91,520 bytes from 26.3's jar).

### 4. The Minecraft instance

The user creates a **Minecraft 26.3, Fabric** instance in their launcher (the Modrinth App is the tested one), and adds
**Fabric API** for 26.3. Find its `mods` folder:

| Launcher | Instance folder |
|---|---|
| Modrinth App | `%APPDATA%\ModrinthApp\profiles\<instance name>` |
| Prism Launcher | `%APPDATA%\PrismLauncher\instances\<instance name>\minecraft` (or `.minecraft`) |
| CurseForge | `%USERPROFILE%\curseforge\minecraft\Instances\<instance name>` |

Into `mods`, add:
- `skycraft-fabric-0.1.2.jar` from
  <https://github.com/chasmlol/SkyCraft/releases/download/v0.1.2/skycraft-fabric-0.1.2.jar>. It has to be exactly
  this version; sha256 `ea429c73dcec49d39b6a0bb9b17ad08a6b0290a39a96ccf6f6c07d581cf992d9`;
- `cybercraft-tweaks-<version>.jar` from this repo's release.

The user sets the instance's Java arguments to
`--enable-native-access=ALL-UNNAMED -Dskycraft.quitWithSkyrim=false` (Modrinth App: the instance's *Settings → Java and
memory*).

**Check:**

```powershell
$mods = "$env:APPDATA\ModrinthApp\profiles\<instance name>\mods"
Get-ChildItem $mods -Filter *.jar | Select-Object Name
(Get-FileHash "$mods\skycraft-fabric-0.1.2.jar").Hash   # EA429C73DCEC49D3...
```

Then the user starts the instance and leaves it on the title screen:

```powershell
Select-String -Path "$env:APPDATA\ModrinthApp\profiles\<instance name>\logs\latest.log" `
  -Pattern "Loading \d+ mods", "- (skycraft|fabric-api|cybercraft_tweaks) ", "SkyCraft:" | Select-Object -First 12
```

**Check:** `skycraft 0.1.2`, `fabric-api` and `cybercraft_tweaks` are listed. A line like
`SkyCraft: can't open Skyrim's shared memory yet` is expected: it's waiting for the game. (SkyCraft's messages name
Skyrim; BlockRender plays that part.)

### 5. First run

The user starts Cyberpunk 2077, loads a save, and presses **F8**. Afterwards:

```powershell
Get-ChildItem "$game\red4ext\logs\blockrender-*.log" | Sort-Object LastWriteTime | Select-Object -Last 1 | Get-Content -Tail 40
Get-Content "$game\bin\x64\plugins\cyber_engine_tweaks\mods\blockrender_hud\status.txt"
```

**Check:** Minecraft's window hid itself, and Minecraft's log says `Skyrim link up`. `status.txt`, which BlockRender
rewrites once a second, shows the link and build mode as on.

## Troubleshooting map

| Symptom | Where to look | Usual cause |
|---|---|---|
| BlockRender not in RED4ext's log | `$game\red4ext\logs\red4ext-*.log` | Game version not 2.31; RED4ext missing or outdated |
| Script errors at startup | `$game\r6\logs\redscript_rCURRENT.log` | redscript missing; a conflicting script mod |
| "there's no atlas.bin" | the overlay's request line | Step 3 not done |
| Never links | Minecraft's `logs\latest.log`, `SkyCraft:` lines | Minecraft not running; a Minecraft menu open; wrong SkyCraft version |
| Fabric refuses to start | Minecraft's `logs\latest.log` | CyberCraft Tweaks needs SkyCraft **0.1.2**, Minecraft **26.3**, Fabric API |
| No city collision; "V's controller isn't found" | `status.txt` | A save loaded in an already-running game: restart the game |
| Entities' textures wrong | the instance's enabled resource packs | A pack that retextures entities |
| Game froze or crashed | `%LOCALAPPDATA%\REDEngine\ReportQueue\`, `blockrender-*.log` | Report it with both logs |

`REDEngineErrorReporter.exe` starts with every game launch. Its running process doesn't mean a crash happened; a
new folder in `ReportQueue` does.

## Developing

### Building and testing

- **BlockRender** (`spikes/block-render/`): Visual Studio 2022 with C++, CMake 3.25+. `cmake --preset default`,
  `cmake --build --preset release`, `ctest --preset release` (556 tests, no game needed). `-DCP2077_DIR=<game>` copies
  each build into the game. Build with the game closed.
- **CyberCraft Tweaks** (`minecraft/cybercraft-tweaks/`): `bash build.sh`, `bash test.sh`, `bash mixins.sh`. Plain
  `javac` against the jars of a Modrinth App instance named `CyberCraft` (see `build.sh`), with no Gradle.
- **Release:** `python release/package.py <build folder> <tweaks jar> <version>` writes to `release/out/`.
- **Clone with submodules:** `git clone --recursive`, or `git submodule update --init` afterwards.

### How the project works

- Work goes in **spikes**: a question, a design (`docs/superpowers/specs/`), a plan of small tasks
  (`docs/superpowers/plans/`), an in-game run, and the results with the answer at the top (`docs/spikes/`). Read the
  latest spec and results before changing a subsystem: they record measurements and dead ends the code doesn't.
- **Only the running game can test the runtime parts** (hooks, rendering, physics, input). Unit-test the logic, and
  have the user run the in-game steps.
- `mclink` (`spikes/block-render/tools/mclink/`) stands in for the game against a real Minecraft, so Minecraft's side
  can be tested without Cyberpunk.

### Hard-won facts about the game

- **Mod code must never fault.** The game's crash handler sees first-chance access violations before any
  `__try/__except`: even a caught fault makes it dump and freeze the game. Validate pointers before reading them.
- **The game's PhysX 3.4 isn't ABI-identical to NVIDIA's public headers.** `PxShape` has an extra virtual at slot 6, so
  every `PxShape` slot from 6 on is one later. The audit is in `docs/spikes/2026-10-01-physx-abi-audit.md`.
- **PhysX coordinates are the world minus a moving origin:** the 1,024 m grid point nearest V, updated one frame
  behind V. Read it from V's character controller, not from V's position.
- **Script callbacks run on many threads** (`NPCPuppet.OnGameAttached` on 16 of them). Don't mutate script arrays from
  them; pass data to BlockRender's natives, which take a lock.
- **The city is in PhysX scene 0 only after a fresh game start.** Loading another save in the same session moves it.
- **Raw Input:** the game registers keyboard and mouse without `RIDEV_INPUTSINK` and reads them in one
  `GetRawInputData` call site. BlockRender patches that import to route input in build mode.

### Conventions

- C++20, tabs, the existing naming (`g_` globals, `a` parameters, trailing `_` members). Comments say *why*, in full
  sentences, as the surrounding code does.
- Bump the version in `spikes/block-render/src/Plugin.cpp` (`kVersion`), or in
  `minecraft/cybercraft-tweaks/src/main/resources/fabric.mod.json`, with each change you ship.
- CyberCraft Tweaks targets SkyCraft's classes by name, so it pins SkyCraft 0.1.2. A new SkyCraft version means
  re-checking every mixin (`mixins.sh` checks Minecraft's; SkyCraft's need reading its jar).

### License

MIT with attribution ([LICENSE](LICENSE)): anything published from this code must credit SkyCraft and CyberCraft where
its users will see it.

# AIity foundation

AIity is an Unreal Engine C++ Stone Age settlement simulation. This foundation contains a deterministic portable core, ten named adult founders, finite food and water, visible movement, a small procedural river valley, a selectable observer camera, a prototype needs/Event display, and SQLite commit-before-publication saves.

**Main first.** The owner directed a reviewed, buildable source baseline onto `main`
as soon as possible. That is not a playable release and not parallel-issue ready.
Remaining runtime foundation proof and bugs
[#5](https://github.com/hazanid/AIity/issues/5),
[#7](https://github.com/hazanid/AIity/issues/7),
[#8](https://github.com/hazanid/AIity/issues/8), and
[#11](https://github.com/hazanid/AIity/issues/11) follow on `main` before parallel
features. Graphical and clustered-restore proof remain required before parallel work
and before any playable claim, not as a pre-`main` bar. A pull request is being
created now. See [project status](docs/status.md).

Latest native evidence: corrected Mac Development arm64 build passed in 8.17 seconds;
targeted report `run.JPpLs9` passed all 13 `AIity.*` tests (11 success, 2 success with
expected corrupt-fixture SQLite warnings; failed 0, notRun 0, inProcess 0). Graphical
and foundation follow-up remain after `main`.

## Project knowledge

- [Approved product and architecture plan](docs/plan.md)
- [Current evidence, blockers, and next gates](docs/status.md)
- [Development, setup, checks, and save handling](docs/development.md)
- [Foundation world rules](docs/world-rules.md)
- [Asset provenance](docs/asset-provenance.md)
- [M1 portable contracts](docs/m1-contracts.md)
- [Agent guidance](AGENTS.md)

Owner-approved exception: M1 contract prototypes may be prepared under
`prototypes/m1/` while Unreal installs. They are not runtime integration.

## Required setup on macOS

1. Xcode 26.3 build 17C529, first launch, and Apple Metal 32023.864 are verified from an
   owner-managed installation through per-build `DEVELOPER_DIR`. Do not assume a
   system-wide path.
2. Unreal Engine 5.8.2 compiled `AIityEditor` Mac Development arm64 with Xcode 26.3
   and SDK 26.2. Latest targeted Automation `run.JPpLs9` passed all 13 `AIity.*` tests
   after the corrected native Mac Development arm64 build.
3. `AIity.uproject` associates engine `5.8`. Do not call the world playable until
   later runtime proof; that proof is not a pre-`main` requirement.
4. Open `AIity.uproject`. When asked, select the installed engine and build the
   `AIityEditor` target.
5. Run the project. `Config/DefaultEngine.ini` deliberately starts `/Engine/Maps/Entry`
   with `AAIityGameMode`; that game mode builds the starter valley from retained engine
   mesh references on every run. It shows a startup failure if required geometry is
   missing.
6. After the first successful build, record the exact working Unreal and Xcode versions
   before release.

To make a saved binary map later, open the running starter world in the editor, choose **File → Save Current Level As**, and save it under `Content/Maps/AIityRiverValley.umap`. Then set `GameDefaultMap` and `EditorStartupMap` to `/Game/Maps/AIityRiverValley`. Keep the generated `.umap` in Git LFS. Do not do this until the installed engine can open and validate the map.

## Controls

- `Space`: pause or resume logical and founder movement clocks
- `R`: probe storage, persist a new movement epoch, and stay paused until `Space`
- `1`, `2`, `4`: scale logical ticks, founder movement, and movement timeout together
- Left click: select a founder
- `[` / `]`: select previous / next founder
- `F`: follow the selected founder
- `WASD`, `Q`, `E`, mouse: move the observer camera

The world opens paused. Saves live under `Saved/Worlds/` and are not committed.
Selection, follow, pause, 4× speed, and one isolated Tick 0/reopen check were
historical observations. Clustered restore, small text, rendering, accessibility, and
package gates remain required before parallel work and release.

## Narrow checks

Portable core check, which does not replace Unreal tests:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror scripts/core-check.cpp \
  Source/AIity/Simulation/AIityRules.cpp \
  Source/AIity/Simulation/AIitySerialization.cpp \
  -o /tmp/aiity-core-check && /tmp/aiity-core-check
```

Portable presentation-helper check:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror scripts/presentation-check.cpp \
  -o /tmp/aiity-presentation-check && /tmp/aiity-presentation-check
```

Portable M1 contract check (not JSON, Ollama, or Unreal):

```sh
clang++ -std=c++17 -Wall -Wextra -Werror \
  scripts/m1-contract-check.cpp \
  prototypes/m1/AIityM1Contracts.cpp \
  -o /tmp/aiity-m1-contract-check && /tmp/aiity-m1-contract-check
```

Required Unreal Automation gate after engine setup:

```sh
UE_ROOT="/path/to/UE_5.x" scripts/check-world.sh
```

The wrapper runs only `AIity.*` tests with `-NullRHI`. It fails on missing engine, timeout, process failure, missing JSON report, zero tests, incomplete `inProcess`, or failed/unrun tests. It creates a unique report directory and does not clear a caller-supplied parent. Complete save/reopen, rendering, accessibility, and packaged 1080p performance remain pre-parallel and pre-release gates.

Commands and setup notes here are a quick start. The
[development guide](docs/development.md) is authoritative.
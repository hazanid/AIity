# AIity — M1 technical foundation

AIity is an Unreal Engine C++ Stone Age settlement simulation. This foundation contains a deterministic portable core, ten named adult founders, finite food and water, visible movement, a small procedural river valley, a selectable observer camera, a prototype needs/Event display, and SQLite commit-before-publication saves.

**Development foundation verified; not a playable release.** PR #12 is merged.
The lead's foundation pass verified all 13 native tests, clustered restoration of all
ten founders, actual-arrival gathering rules, signed SQLite range rejection, ordinary
close, and paused reopen without offline progression. The historical shutdown assertion
matches an unresolved upstream Unreal viewport race; bounded standalone close/F9-close
checks passed. See [exact evidence and limits](docs/status.md).

Portable pull-request checks (#9), readable HUD (#4), and procedural colors/lighting (#6)
are integrated and verified. Real-pointer acceptance (#10) remains open. Camera startup is stabilized and
keyboard selection/follow are verified at 720p and 1080p; physical mouse acceptance is
pending. M2 prototypes are not a local-model/dialogue runtime.

## Project knowledge

- [Approved product and architecture plan](docs/plan.md)
- [Current evidence, blockers, and next gates](docs/status.md)
- [Milestones, issues and dependency order](docs/roadmap.md)
- [Development, setup, checks, and save handling](docs/development.md)
- [Foundation world rules](docs/world-rules.md)
- [Asset provenance](docs/asset-provenance.md)
- [M2 portable contracts](docs/m1-contracts.md)
- [Agent guidance](AGENTS.md)

Milestone numbering was updated with owner approval: M1 foundation, M2 playable lives,
M3 society/production, M4 generations. Legacy `m1` file and code names are retained.

Owner-approved exception: M2 contract prototypes may be prepared under
`prototypes/m1/` while Unreal installs. They are not runtime integration.

## Required setup on macOS

1. Xcode 26.3 build 17C529, first launch, and Apple Metal 32023.864 are verified from an
   owner-managed installation through per-build `DEVELOPER_DIR`. Do not assume a
   system-wide path.
2. Unreal Engine 5.8.2 compiled `AIityEditor` Mac Development arm64 with Xcode 26.3
   and SDK 26.2. Foundation Automation `run.ATEqpo` passed all 13 `AIity.*` tests
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
- `WASD`: move the observer; `Q` / `E`: descend / ascend
- Hold right mouse button and move the mouse: look around

The world opens paused. Saves live under `Saved/Worlds/` and are not committed.
Keyboard selection, follow, pause, 4× speed, fresh Tick 0, clustered restore and exact
paused reopen were observed in isolated graphical sessions. HUD text and procedural lighting/colors are verified at 720p and 1080p.
Physical mouse acceptance, broader accessibility, packaging and M2 behavior remain unfinished.

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

Portable M2 contract check (not JSON, Ollama, or Unreal):

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

The wrapper runs only `AIity.*` tests with `-NullRHI`. It fails on missing engine, timeout, process failure, missing JSON report, zero tests, incomplete `inProcess`, or failed/unrun tests. It creates a unique report directory and does not clear a caller-supplied parent. Save/reopen evidence is recorded in status. Rendering, accessibility, and packaged
1080p performance remain release gates.

Commands and setup notes here are a quick start. The
[development guide](docs/development.md) is authoritative.

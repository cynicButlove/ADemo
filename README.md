# ADemo

ADemo is an Unreal Engine 5.5 tactical FPS / extraction-mode demo focused on server-authoritative combat, weapon feel, tactical AI, looting, extraction, and runtime debugging tools.

## Highlights

- Server-authoritative combat path based on UE Dedicated Server, RPC, Actor Replication, CharacterMovement, and GAS AttributeSet replication.
- Hitscan weapon logic runs in the real UE server world, so walls, cover, characters, and map collision participate in authoritative hit validation.
- Data-driven weapon system with configurable damage, fire rate, magazine size, reserve ammo, reload time, ADS spread, recoil, spread recovery, and body-part damage multipliers.
- Deterministic sustained-fire spray pattern replacing temporary random spread, with a runtime weapon workbench for comparing baseline and attachment-modified shot distribution.
- Simplified runtime attachment system for muzzle, grip, magazine, and optic modifiers while keeping weapon visuals unchanged.
- Tactical AI built with Behavior Tree, Blackboard, AI Perception, squad alert sharing, suppression/flank roles, cover scoring, search states, and debug visualization.
- PvPvE extraction loop with inventory, loot containers, pickup/drop flow, extraction zones, match timer, HUD feedback, and extraction result summary.

## Tech Stack

- Unreal Engine 5.5
- C++ and Blueprint
- Gameplay Ability System
- UE Dedicated Server
- Behavior Tree / Blackboard / AI Perception
- Git LFS for Unreal binary assets

## Repository Layout

- `DemoClient/` - UE project, gameplay code, content, packaging and local run scripts.
- `DemoServer/` - external server prototype used for early custom TCP/snapshot synchronization experiments.

## Main Code Areas

- `DemoClient/Source/DemoClient/Character/` - player character, inputs, combat RPCs, weapon switching, death and respawn.
- `DemoClient/Source/DemoClient/Weapon/` - weapon base logic, hitscan, recoil, spray pattern, attachments, decals, damage application.
- `DemoClient/Source/DemoClient/Combat/` - GAS attribute set and damage execution.
- `DemoClient/Source/DemoClient/AI/` - AI character, controller, tactical state, behavior tree tasks.
- `DemoClient/Source/DemoClient/Inventory/` - inventory, loot items, loot containers.
- `DemoClient/Source/DemoClient/Online/` - lobby flow, direct UE Dedicated Server travel, external server subsystem.
- `DemoClient/Source/DemoClient/UI/` - HUD, hit feedback, extraction UI, inventory panel, weapon workbench.

## Running Locally

1. Open `DemoClient/DemoClient.uproject` with Unreal Engine 5.5.
2. Build the `DemoClientEditor` target.
3. Use the editor for local PIE testing, or use the scripts under `DemoClient/` for dedicated-server local runs.

Useful scripts include:

- `DemoClient/Run_UEDedicatedServer_Local.bat`
- `DemoClient/Run_DemoClient_UEDirect_Local.bat`
- `DemoClient/Run_DemoClient_UEDirect_Player1.bat`
- `DemoClient/Run_DemoClient_UEDirect_Player2.bat`
- `DemoClient/Package_UEDedicatedServer_Win64.bat`

## Notes

Generated UE folders such as `Binaries`, `Intermediate`, `Saved`, packaged builds, debug symbols, and local project notes are intentionally excluded from source control.

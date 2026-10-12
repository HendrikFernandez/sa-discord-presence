# GTA San Andreas Discord Rich Presence

A 32-bit ASI plugin for the **classic PC GTA San Andreas 1.0 US executable**. Discord shows:

- **Details:** `Free roam`, `Mission: Tagging Up Turf`, `Paused`, `Wasted`, or `Busted`.
- **State:** the current neighborhood, game completion, and money, for example `Ganton | 12.8% complete | $4,500`.

The plugin samples game state every five seconds and only sends changes. It reads game state on the game thread and uses Discord's legacy `discord-rpc` library for the background IPC connection. No account token or bot is needed.

Vehicle side missions are included: Paramedic (ambulance), Vigilante (police vehicles), Firefighter (fire truck), Taxi Driver, Burglary, and Freight Train. The mission detector checks active mission scripts, so these can appear even when the storyline mission flag is clear.

## Downloads

Download the ZIP from the [latest release](https://github.com/HendrikFernandez/sa-discord-presence/releases/latest). The public ZIP includes a blank `SaDiscordPresence.ini`. Before installing, enter a Discord Application ID in its `ApplicationId` field. You can create an application in the [Discord Developer Portal](https://discord.com/developers/applications).

## Build

Install Visual Studio with the **Desktop development with C++** workload, CMake, and a 32-bit build of [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk). The SDK's [setup guide](https://github.com/DK22Pac/plugin-sdk/wiki/Set-up-plugin-sdk) explains how to generate and build its libraries. CMake downloads the tagged [Discord RPC v3.4.0](https://github.com/discord/discord-rpc/releases/tag/v3.4.0) and [RapidJSON v1.1.0](https://github.com/Tencent/rapidjson/releases/tag/v1.1.0) sources at configure time.

From this directory in PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DPSDK_DIR="C:/path/to/plugin-sdk" -DPSDK_LIB="C:/path/to/plugin-sdk/output/lib/Plugin.lib"
cmake --build build --config Release
```

Set `PSDK_LIB` to the actual **SA Release x86** library produced by your Plugin-SDK build; its exact filename can vary by SDK version. `SaDiscordPresence.asi` and both INI files are copied to `build/Release`.

## Install

1. Extract the release ZIP and copy `SaDiscordPresence.asi`, `SaDiscordPresence.ini`, and `SaDiscordPresence.missions.ini` into the game's `gta_sa.exe` directory (or an ASI-loader plugin directory, keeping all three files together).
2. Install an ASI loader if the game does not already have one. Run the desktop Discord client, enable **Share my activity** under **User Settings > Activity Sharing**, then start a saved game.

You can edit the mission INI to name scripts added by other mods. Unknown missions show their internal script name.

## Compatibility and limits

- Supports classic GTA SA **1.0 US** via Plugin-SDK, including its recognized Compact and Hoodlum variants. It does not target the Definitive Edition, 1.01, or Steam binaries.
- Vanilla mission names come from this [script-name list](https://gist.github.com/ViperJohnGR/8e4655a791b259a127e9299b4f096824). `BCESAR4` and some activities share script names, so those entries may show a combined or generic title.
- Indoor areas show `Interior` because interior coordinates do not reliably identify the outside neighborhood.
- Discord's Rich Presence layout is controlled by Discord; long text is trimmed to its 128-byte field limit.
- The x86 ASI build and its DLL imports were verified in this workspace. It has not been run in-game here.

Game data and events use [Plugin-SDK](https://github.com/DK22Pac/plugin-sdk). Presence delivery uses [Discord's legacy RPC library](https://github.com/discord/discord-rpc); its API still exposes `Discord_Initialize` and `Discord_UpdatePresence` for native games.

Third-party license texts are in [`third-party-licenses`](third-party-licenses).
The mod's original source is available under the [MIT License](LICENSE).

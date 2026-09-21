# Launcher Steam initialization fix

Launching v12 from Explorer could show **Unable to initialize SteamAPI** even
while Steam was running. The direct headset launch did not supply Steam's app
identity. The launcher now sets `SteamAppId=287700` and `SteamGameId=287700` for
the MGSV child process, then restores the calling environment on success or error.
An inherited identity from another game is replaced only for this launch.

The game still uses the existing Steam client and its ownership checks. The
launcher does not create or edit `steam_appid.txt`, change the global OpenXR
runtime, or restart Steam. The selected physical runtime stays local to MGSV.
Valve documents app identity as one cause of initialization failure:
[SteamAPI_Init](https://partner.steamgames.com/doc/api/steam_api#SteamAPI_Init).

Observed locally on September 20: supplying the identity launched PID 37472;
Steam tracked it as app 287700, the game rendered frames, and OpenXR reported
Oculus / Meta Quest 3. The game was left running for the user's headset test.
This establishes successful startup, not headset gameplay acceptance.

The headset-launcher and launcher-display tests passed on Windows PowerShell 5.1.
They cover absent or wrong inherited Steam identities, restoration after failure,
runtime isolation, and the launcher button's path into the headset launch script.
The VR DLL is unchanged from v12. The user subsequently reported the candidate
good to go and approved packaging. See [the release note](RELEASE_2026-09-20.md)
for scope and remaining issues; this is not an exhaustive headset coverage claim.

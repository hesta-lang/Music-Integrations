# Music Integrations — Android ARM64 port kit

This kit is a **porting/build overlay**, not a verified `.geode` binary. It targets Android ARM64 and deliberately keeps the original non-Windows Spotify Web API path instead of trying to compile Windows Media Controls / WinRT on Android.

## Important discovery

The uploaded `.geode` is a compiled package and does not contain the original C++ source. The upstream source repository is:
https://github.com/Vinsterplays/Music-Integrations

The upstream C++ already uses Spotify's Web API on non-Windows platforms. The Windows-only media-session code is guarded by `GEODE_IS_WINDOWS`; therefore the first Android port should preserve the Spotify path and remove Windows-specific build assumptions, rather than introduce Discord Social SDK or an Android-only media-session service.

## Use from an Android browser

1. Open the upstream repository above and create a fork under your GitHub account.
2. Extract this ZIP on your phone.
3. Upload `android-port.patch`, `mod.json.android.json`, and `.github/workflows/android-port.yml` to the root / matching path of your fork. GitHub's mobile site may require **Add file → Upload files**; create `.github/workflows/` if it does not exist.
4. Open **Actions → Android ARM64 build → Run workflow**.
5. If the patch applies and the build succeeds, download the `music-integrations-android64` artifact and install the `.geode` in Geode.

## Honest status / expected limitations

- The patch only changes the project build metadata to include Android and updates the CMake platform link branch; it does not claim that the whole mod is verified on-device.
- On Android the patch adds a touch button near the top-left of each game scene to open/close the overlay. This is a first-pass entry point and should be checked for overlap with other mods/UI layouts on-device.
- The original Spotify OAuth helper uses a loopback HTTP callback and a third-party token relay. Verify that authentication still works on Android and review the relay/privacy behavior before using it.
- The original code's automatic pause/resume behavior relies on game-layer hooks and Spotify API calls. It can only control Spotify, not arbitrary Android media apps.
- A green build proves compilation only. Test: launch game without crashing, open overlay, link Spotify, verify current song/artist, play/pause/next/previous, then test auto pause/resume in a level and editor playtest.

This is a staged port because Android cannot use Windows' `GlobalSystemMediaTransportControlsSessionManager`. It does not add an Android notification-listener service; that would require app/launcher-side manifest and permission work beyond a Geode native mod.

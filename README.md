<p align="center">
  <img src="ps5/app/sce_sys/icon0.png" width="160" alt="Régie5 icon">
</p>

<h1 align="center">Régie5</h1>

<p align="center">
  <b>Your self-hosted media stack, on the TV, from your PS5.</b><br>
  A native PS5 homebrew app to keep an eye on Sonarr, Radarr, Seerr and Jellyfin
  from the couch, with a controller.
</p>

<p align="center">
  <img src="docs/images/home.jpg" width="820" alt="Régie5 home screen: upcoming episodes and films, recently added, server health">
</p>

Régie5 (French for a TV control room) is for people who run their own Jellyfin
server with the *arr apps and have a jailbroken PS5 under the TV. It shows what is
coming, what is downloading, who is watching and how well, and lets you act on the
few things worth doing from the couch: approve a request, stop a stream, scan the
libraries, free some space.

Everything stays on your local network: Régie5 talks only to your own servers,
never to the internet.

## Features

| | |
|---|---|
| **Home** | Upcoming episodes and films (14 days, Sonarr + Radarr calendars), recently added to Jellyfin, disk space and health alerts of every instance. |
| **Library** | Your series and films (read-only), a week-by-week **TV guide**, what is **missing**, and an optional **Cleanup** view. |
| **Activity** | The download queue of Sonarr and Radarr merged (title, progress, time left, status; refreshed every 5 s), and a 14-day **log**: imports, Seerr requests, Jellyfin playback and sign-ins, health alerts. |
| **Requests** | Seerr requests (pending / approved / all), with **Approve** and **Decline**. |
| **Jellyfin** | Who is watching what, Direct Play or transcoding and **why**, a **playback diagnosis** (source vs. sent to the client, hardware acceleration, "is the transcode keeping up?", advice), **Stop playback**, **Scan libraries**, scheduled tasks, and server load through [Glances](https://nicolargo.github.io/glances/) if you use it. |
| **Settings** | Connection state of every service and instance, version, Cleanup on/off, and **set-up from a phone** (QR code). |

Also:

- **Several instances** of a service (for example Radarr and Radarr 4K) are shown together, each named.
- **HTTPS** and reverse-proxy sub-paths (`https://example.lan/sonarr`), self-signed certificates on request.
- **Liquid-glass interface** made for a TV: large type readable from 3 m, a springy focus drop, 4K at 60 fps.
- **English and French**, following the console's language.

<p align="center">
  <img src="docs/images/movies.jpg" width="400" alt="Library: films">
  <img src="docs/images/series-detail.jpg" width="400" alt="Library: a series' page">
  <img src="docs/images/guide.jpg" width="400" alt="Library: the TV guide">
  <img src="docs/images/queue.jpg" width="400" alt="Activity: the download queue">
  <img src="docs/images/requests.jpg" width="400" alt="Requests from Seerr">
  <img src="docs/images/jellyfin.jpg" width="400" alt="Jellyfin: sessions and transcoding">
  <img src="docs/images/diagnosis.jpg" width="400" alt="Jellyfin: playback diagnosis">
  <img src="docs/images/cleanup.jpg" width="400" alt="Library: Cleanup candidates">
</p>

<sub>Screenshots: the desktop build in demo mode (sample data, generated covers).</sub>

### What Régie5 changes on your servers

Almost nothing, and never without asking. It is read-only, except for these
actions, each behind a confirmation whose focus starts on **Cancel**:

- approve or decline a Seerr request;
- stop a Jellyfin playback session; scan the Jellyfin libraries;
- **Cleanup** (off by default): delete a film or a series **with its files** in
  Radarr/Sonarr, always added to the import-list exclusions so no list brings it
  back. It is turned on in Settings after a warning, offers only titles watched
  and finished (by every active account, or at least one, as you choose) and not
  watched for months, never favourites; every deletion asks twice, one at a time.

### What Régie5 will never do

No download client, no indexer, no release search, no release or tracker names
anywhere: Régie5 shows titles, progress and quality, nothing else. Its activity
log never reads Jellyfin's free-text fields (which hold IP addresses and device
names).

## Requirements

- A **jailbroken PS5** with [ShadowMount+](https://github.com/drakmor/ShadowMountPlus),
  which turns folders in `/data/homebrew/` into home-screen tiles. Tested on
  firmware **13.60**.
- A way to copy a folder to the console: **ps5upload** or an **FTP** payload.
- At least one of the services below, reachable from the console on your local
  network, with its API key.

### Services and versions

| Service | Tested with | Used for |
|---|---|---|
| [Sonarr](https://sonarr.tv) | 4.0 | calendar, queue, series, health, disk space, history; Cleanup |
| [Radarr](https://radarr.video) | 6.4 | calendar, queue, films, health, disk space, history; Cleanup |
| [Seerr](https://github.com/seerr-team/seerr) | 3.5 | requests, approve / decline, posters through its image proxy |
| [Jellyfin](https://jellyfin.org) | 12.2 | sessions, playback info, recently added, users, activity log, scheduled tasks |
| [Glances](https://nicolargo.github.io/glances/) (optional) | 4.x API | server load |

Older versions are not tested. Each service is optional: a service turned off simply
leaves its sections out. Settings says plainly when a service does not answer as
expected.

## Install

1. Download `PPSA99520.zip` from the [latest release](https://github.com/viviandsx/regie5/releases/latest).
2. Unzip it on your computer: it holds a single folder, `PPSA99520`.
3. Copy that whole folder into `/data/homebrew/` on the console (ps5upload or FTP),
   so that `/data/homebrew/PPSA99520/eboot.bin` exists.
4. Wait about ten seconds: ShadowMount+ adds the **Régie5** tile to the home screen.
5. Open Régie5.

**Updating:** copy the new `PPSA99520` folder over the old one. A release never
contains a `config.json`, so yours is kept.
**Uninstalling:** delete `/data/homebrew/PPSA99520`.

The same steps, in English and French, are in `INSTALL.txt` inside the zip.

## Set up

### From your phone (recommended)

1. In Régie5, press **R1** until the gear (**Settings**) is selected.
2. Choose **From a phone**: a QR code and a 6-digit code appear on the TV.
3. Scan the QR code with your phone, on the same network as the console (or type
   the address shown on the TV into the phone's browser).
4. Fill in each service: its address with the port (`http://192.168.1.10:8989`,
   `https://example.lan/radarr`…) and its API key. Tick **Self-signed
   certificate** for an `https://` address whose certificate is self-signed.
5. Tap **Save on the console**: Régie5 tests every service, and each should get a
   green dot.

The page is served by the console on your local network only, while that window
is open, and needs the code. Keys already saved are never sent back to the phone:
a key field left empty keeps the saved key.

### By file

Copy `config.example.json` (in the app folder) to `config.json` in
`/data/homebrew/PPSA99520/`, fill it in on a computer, copy it to the console, then
choose **Test all** in Settings. Several instances of a service are a list of named
instances; the fields are described in `config.example.json`.

Where the API keys are: Sonarr and Radarr, *Settings › General › Security*; Seerr,
*Settings › General*; Jellyfin, *Dashboard › API Keys*.

## Controls

| Button | Action |
|---|---|
| D-pad / left stick | move |
| ✕ | select |
| ○ | back |
| L1 / R1 | previous / next tab |
| L2 / R2 | previous / next letter (lists by title), week (guide) or page |
| △ | refresh the screen |
| Options | the selected item's menu (approve / decline, keep / delete, find in Jelly5) |

## Known limitations

- **No "Open in Jelly5".** The console does not let one app start another
  (`sceSystemServiceLaunchApp` and `sceLncUtilLaunchApp` are not available to
  apps). Options › **Find in Jelly5** tells you which title to look for in
  [Jelly5](https://github.com/02dnot/Jelly5) instead.
- **Seerr after a Cleanup deletion:** Seerr has no API to mark a title deleted.
  Its own *Media Availability Sync* job (nightly by default, or run by hand in
  Seerr › Settings › Jobs) marks it deleted, keeps its requests and lets it be
  requested again.
- **HTTPS on the console** uses TLS 1.2 (ECDHE with AES-GCM, ChaCha20-Poly1305 or
  AES-CBC; RSA and ECDSA certificates). A server that only accepts TLS 1.3 cannot be
  reached from the console.
- `config.json` holds your API keys in plain text on the console.
- Only firmware 13.60 has been tested.

## Building from source

Régie5 is developed on Linux, where the same app runs in a window (keyboard or
DualSense through SDL), and can run on sample data with no server at all.

```bash
# Debian / Ubuntu
sudo apt install build-essential clang cmake ninja-build libsdl2-dev libfreetype-dev libcurl4-openssl-dev libgl-dev

scripts/run-linux.sh --mock              # demo mode: sample data, no server contacted
scripts/run-linux.sh                     # with ~/.config/regie5/config.json
cmake --build build/linux && ctest --test-dir build/linux
```

On the keyboard: arrows, Enter (✕), Escape (○), Q/E (L1/R1), Z/C (L2/R2), R (△),
M (Options).

The PS5 build needs the [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk),
pacbrew and LLVM (`clang`, `lld`, `llvm`):

```bash
scripts/setup-ps5-toolchain.sh           # the SDK and pacbrew, checksums verified
scripts/release-ps5.sh                   # build/ps5/release-<version>/upload/PPSA99520.zip
```

The design and the porting notes are in `docs/` (in French).

## Contributing

Bug reports and ideas are welcome in the issues. Please read the scope above
first: features around download clients, indexers or release searching will not
be added.

## License and credits

Régie5 is free software under the **GNU General Public License v3.0 or later**
(see [LICENSE](LICENSE)).

It builds on the work of others, listed with their licences in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), among them:
[Jelly5](https://github.com/02dnot/Jelly5) (the liquid-glass interface it is
modelled on), [OverShifted/LiquidGlass](https://github.com/OverShifted/LiquidGlass),
BlackBearReloaded's [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) and
[ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui), the
[ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk),
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls), [cJSON](https://github.com/DaveGamble/cJSON),
[stb_image](https://github.com/nothings/stb), Nayuki's
[QR Code generator](https://www.nayuki.io/page/qr-code-generator-library),
[FreeType](https://freetype.org) and the [Inter](https://github.com/rsms/inter) typeface.

Régie5 is not affiliated with or endorsed by Sony Interactive Entertainment, the
Jellyfin project, or the Sonarr, Radarr or Seerr projects. *PlayStation*, *PS5* and
*DualSense* are trademarks of Sony Interactive Entertainment Inc.

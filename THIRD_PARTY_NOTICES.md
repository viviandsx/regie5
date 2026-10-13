# Third-party notices

Régie5 is free software under the GNU General Public License v3.0 or later
(see [LICENSE](LICENSE)). It builds on, bundles or links the following work,
each under its own licence. All of them are compatible with
GPL-3.0-or-later, under which the combined program is distributed.

## Source code adapted in this repository

| Component | Where | Licence |
| --- | --- | --- |
| [Jelly5](https://github.com/02dnot/Jelly5): the renderer interface (`gfx.h`), the focus drop, the glass pane, the ambient background, the card lift and row scrolling, the controller hints, the easing (`anim.h`) and the liquid glass shader; adapted for Régie5 | `src/ui/gfx.h`, `src/ui/drop.*`, `src/ui/style.*`, `src/ui/ambient.*`, `src/ui/hints.*`, `src/ui/anim.h`, `src/platform/gl/gl_shaders.h` | GPL-3.0-or-later |
| [cJSON](https://github.com/DaveGamble/cJSON) 1.7.18 - Copyright (c) 2009-2017 Dave Gamble and cJSON contributors | `third_party/cjson/` | MIT (`third_party/cjson/LICENSE`) |
| [stb_image](https://github.com/nothings/stb) v2.30 (commit f58f558) - Sean Barrett | `third_party/stb/stb_image.h` | Public domain or MIT (at the end of the file) |
| [QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library) (C) - Copyright (c) Project Nayuki, by way of Jelly5 | `third_party/qrcodegen/` | MIT (in the file headers) |
| The refraction profile of the glass shader follows [OverShifted/LiquidGlass](https://github.com/OverShifted/LiquidGlass) - Copyright (c) 2026 Sepehr Kalanaki | `src/platform/gl/gl_shaders.h` (by way of Jelly5) | MIT |

### PS5 build (phase 6)

| Component | Where | Licence |
| --- | --- | --- |
| [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui) - Copyright (C) 2026 BlackBearReloaded: the process heap and runtime shims (adapted: Régie5's log folder, a heap of 256, 192 or 128 MiB, reports of failed allocations), the EGL display bring-up and the DualSense reading (rewritten after it) | `src/platform/ps5/runtime/`, `src/platform/ps5/display_egl.*`, `src/platform/ps5/pad_ps5.*`, `src/platform/ps5/system_ps5.*` | GPL-3.0-or-later |
| [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui) build tooling, from [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) (compile, link, FSELF signing, packaging, and the clean-room `libc.prx` runtime shim shipped in `sce_module/`) - Copyright (C) 2026 BlackBearReloaded | fetched at a pinned commit by `scripts/build-ps5.sh`, not stored here | GPL-3.0-or-later |

## Demo pictures (not part of the app)

`demo/images/` holds posters and stills of the Blender Foundation's open movies, from
Wikimedia Commons, used only by `regie5 --demo` for the README screenshots; they are not
in the PS5 release. Big Buck Bunny, Elephants Dream, Sintel, Tears of Steel, Cosmos
Laundromat, Spring, Coffee Run, Sprite Fright, Charge and Caminandes, © Blender Foundation |
studio.blender.org, under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/) or
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) (one still under CC0), resized,
and one cropped. The file-by-file list, with authors, licences and sources, is in
[demo/CREDITS.md](demo/CREDITS.md).

## Fonts (bundled)

| Font | Licence |
| --- | --- |
| [Inter](https://github.com/rsms/inter) - Copyright (c) 2016 The Inter Project Authors | SIL Open Font License 1.1 (`assets/fonts/Inter-LICENSE.txt`) |

## Libraries linked into the program

| Library | Licence |
| --- | --- |
| [SDL2](https://libsdl.org) (Linux build) | zlib |
| [FreeType](https://freetype.org) | FreeType License (FTL). Portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved. |
| [libcurl](https://curl.se) (Linux build) | curl License (MIT-style) |
| OpenGL (the system's driver, Linux build) | not bundled |
| [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) 1.0.1 (PS5 build): OpenGL 4.6 on the PS5's GPU, built on Mesa and Gallium, linked statically - Copyright (C) 2026 BlackBearReloaded; its own third-party components (Mesa, OpenGNM…) are listed in its `THIRD_PARTY_NOTICES.md`, shipped with their licences in the release zip under `PPSA99520/licenses/ps5-opengl/` | GPL-3.0-or-later (Mesa: MIT and others, as listed there) |
| [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) (PS5 build): headers, link stubs, libc++ | GPL-3.0-or-later (SDK); libc++: Apache-2.0 with LLVM exceptions |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) 3.6.7 (HTTPS over the socket transport: PS5 build, and Linux with `REGIE5_HTTP=socket`), fetched by `scripts/fetch-mbedtls.sh`, compiled with `src/platform/posix/mbedtls_config.h` | Apache-2.0 (shipped in the release zip under `PPSA99520/licenses/mbedtls/`) |
| Mozilla's certificate authorities, as [curl.se](https://curl.se/docs/caextract.html) extracts them (`cacert-2026-09-25.pem`, PS5 build: `assets/certs/cacert.pem`) | MPL-2.0 (stated in the file) |
| libpng, zlib, bzip2 (PS5 build, from [pacbrew](https://github.com/ps5-payload-dev/pacbrew-repo), linked statically with FreeType) | libpng License, zlib, bzip2 License |

## Trademarks

Régie5 is not affiliated with or endorsed by Sony Interactive Entertainment, the
Jellyfin project, or the Sonarr, Radarr or Seerr projects. *PlayStation*, *PS5*
and *DualSense* are trademarks of Sony Interactive Entertainment Inc. They are
used here only to say what Régie5 runs on and works with.

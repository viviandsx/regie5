# Regie5 — Linux desktop build (SDL2 + OpenGL 3.3)
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(regie5 VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(REGIE5_VERSION "${PROJECT_VERSION}-dev" CACHE STRING "Version shown in the app")
# A -dev version follows project(VERSION) in a build folder made before it
# changed; one given by hand (-DREGIE5_VERSION=...) is kept.
if(REGIE5_VERSION MATCHES "-dev$" AND NOT REGIE5_VERSION STREQUAL "${PROJECT_VERSION}-dev")
    set(REGIE5_VERSION "${PROJECT_VERSION}-dev" CACHE STRING "Version shown in the app" FORCE)
endif()

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# Portable code: no SDL, no OpenGL, no platform header (see docs/ARCHITECTURE.md).

# The languages of the interface: every file of src/core/lang/ (a new one is
# picked up without touching this file; docs/TRANSLATING.md).
file(GLOB REGIE5_LANGUAGES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/core/lang/*.cpp")

# The data: configuration, models, API clients and parsers.
add_library(regie5_data STATIC
    ${REGIE5_LANGUAGES}
    third_party/cjson/cJSON.c
    src/core/app_settings.cpp
    src/core/badges.cpp
    src/core/boot_guard.cpp
    src/core/cleanup.cpp
    src/core/config.cpp
    src/core/config_page.cpp
    src/core/config_web.cpp
    src/core/diagnosis.cpp
    src/core/i18n.cpp
    src/core/input.cpp
    src/core/jelly5.cpp
    src/core/libraries.cpp
    src/core/options.cpp
    src/core/perf.cpp
    src/core/schedule.cpp
    src/core/versions.cpp
    src/core/welcome.cpp
    src/core/whats_new.cpp
    src/api/arr.cpp
    src/api/glances.cpp
    src/api/http.cpp
    src/api/http_wire.cpp
    src/api/jellyfin.cpp
    src/api/json.cpp
    src/api/mock_transport.cpp
    src/api/plex.cpp
    src/api/seerr.cpp
    src/api/services.cpp
)
target_include_directories(regie5_data PUBLIC src third_party/cjson)
target_compile_definitions(regie5_data PUBLIC REGIE5_VERSION="${REGIE5_VERSION}")
target_link_libraries(regie5_data PUBLIC Threads::Threads)
target_compile_options(regie5_data PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wno-unused-parameter>)

# The interface: drawing on top of ui/gfx.h, widgets and screens.
add_library(regie5_ui STATIC
    src/ui/ambient.cpp
    src/ui/arr_lists.cpp
    src/ui/art.cpp
    src/ui/badges.cpp
    src/ui/config_reload.cpp
    src/ui/drop.cpp
    src/ui/font.cpp
    src/ui/format.cpp
    src/ui/gfx_text.cpp
    src/ui/hints.cpp
    src/ui/phone_share.cpp
    src/ui/jelly5_link.cpp
    src/ui/poster.cpp
    src/ui/quick_connect.cpp
    src/ui/script.cpp
    src/ui/seerr_titles.cpp
    src/ui/service_rows.cpp
    src/ui/shell.cpp
    src/ui/style.cpp
    src/ui/tabbar.cpp
    src/ui/welcome_flow.cpp
    src/ui/whats_new_sheet.cpp
    src/ui/widgets.cpp
    src/ui/screens/cleanup.cpp
    src/ui/screens/discover.cpp
    src/ui/screens/downloads.cpp
    src/ui/screens/guide.cpp
    src/ui/screens/home.cpp
    src/ui/screens/issues.cpp
    src/ui/screens/jellyfin_screen.cpp
    src/ui/screens/jellyfin_stats.cpp
    src/ui/screens/journal.cpp
    src/ui/screens/library.cpp
    src/ui/screens/library_detail.cpp
    src/ui/screens/requests.cpp
    src/ui/screens/session_diag.cpp
    src/ui/screens/settings.cpp
    third_party/qrcodegen/qrcodegen.c
)
target_include_directories(regie5_ui PRIVATE third_party/stb third_party/qrcodegen)
target_link_libraries(regie5_ui PUBLIC regie5_data Freetype::Freetype)
target_compile_options(regie5_ui PRIVATE -Wall -Wextra -Wno-unused-parameter)

# HTTPS for the socket transport (the PS5's, REGIE5_HTTP=socket here): Mbed
# TLS from toolchain/ (scripts/fetch-mbedtls.sh), with Regie5's own
# configuration. Without it, https:// over sockets says "not available".
set(REGIE5_MBEDTLS "${CMAKE_SOURCE_DIR}/toolchain/mbedtls-3.6.7")
if(EXISTS "${REGIE5_MBEDTLS}/library/ssl_tls.c")
    file(GLOB REGIE5_MBEDTLS_SOURCES "${REGIE5_MBEDTLS}/library/*.c")
    add_library(regie5_tls STATIC ${REGIE5_MBEDTLS_SOURCES})
    target_include_directories(regie5_tls SYSTEM PUBLIC "${REGIE5_MBEDTLS}/include" PRIVATE "${REGIE5_MBEDTLS}/library")
    target_compile_definitions(regie5_tls PUBLIC
        MBEDTLS_CONFIG_FILE="${CMAKE_SOURCE_DIR}/src/platform/posix/mbedtls_config.h" REGIE5_TLS=1)
    target_compile_options(regie5_tls PRIVATE -w)
    set(REGIE5_TLS_LIB regie5_tls)
    message(STATUS "Regie5: HTTPS over sockets with Mbed TLS (${REGIE5_MBEDTLS})")
else()
    set(REGIE5_TLS_LIB "")
    message(STATUS "Regie5: no Mbed TLS (scripts/fetch-mbedtls.sh): https over sockets unavailable")
endif()

add_executable(regie5
    src/platform/linux/main_linux.cpp
    src/platform/linux/curl_transport.cpp
    src/platform/gl/gfx_gl.cpp
    src/platform/linux/input_sdl.cpp
    src/platform/linux/launch_linux.cpp
    src/platform/linux/paths_linux.cpp
    src/platform/linux/text_linux.cpp
    src/platform/posix/config_server.cpp
    src/platform/posix/socket_transport.cpp
    src/platform/posix/tls_glue.cpp
)
target_link_libraries(regie5 PRIVATE regie5_ui SDL2::SDL2 OpenGL::GL CURL::libcurl ${REGIE5_TLS_LIB})
target_compile_definitions(regie5 PRIVATE
    REGIE5_SOURCE_ASSETS="${CMAKE_SOURCE_DIR}/assets"
    REGIE5_SOURCE_FIXTURES="${CMAKE_SOURCE_DIR}/fixtures"
    REGIE5_SOURCE_DEMO="${CMAKE_SOURCE_DIR}/demo")
target_compile_options(regie5 PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Unit tests: the parsers on fixtures/, no network.
enable_testing()
add_executable(regie5_tests
    tests/main.cpp
    tests/test_api.cpp
    tests/test_core.cpp
    tests/test_delays.cpp
    tests/test_hostile.cpp
    tests/test_lists.cpp
    tests/test_share.cpp
    tests/platform_stub.cpp
    src/platform/posix/config_server.cpp
    src/platform/posix/socket_transport.cpp
    src/platform/posix/tls_glue.cpp
)
target_link_libraries(regie5_tests PRIVATE regie5_data ${REGIE5_TLS_LIB})
target_compile_definitions(regie5_tests PRIVATE
    REGIE5_FIXTURES="${CMAKE_SOURCE_DIR}/fixtures"
    REGIE5_DEMO="${CMAKE_SOURCE_DIR}/demo"
    REGIE5_SOURCE="${CMAKE_SOURCE_DIR}")
target_compile_options(regie5_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME regie5_tests COMMAND regie5_tests)

# The interface's parts that can run without a GPU (the picture cache), on a
# renderer that only counts (tests/gfx_stub.cpp).
add_executable(regie5_ui_tests
    tests/main.cpp
    tests/test_art.cpp
    tests/test_fonts.cpp
    tests/test_frame_budgets.cpp
    tests/gfx_stub.cpp
    tests/platform_stub.cpp
)
target_link_libraries(regie5_ui_tests PRIVATE regie5_ui)
target_compile_definitions(regie5_ui_tests PRIVATE
    REGIE5_FIXTURES="${CMAKE_SOURCE_DIR}/fixtures"
    REGIE5_ASSETS="${CMAKE_SOURCE_DIR}/assets")
target_compile_options(regie5_ui_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME regie5_ui_tests COMMAND regie5_ui_tests)

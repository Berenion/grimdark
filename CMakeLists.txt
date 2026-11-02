cmake_minimum_required(VERSION 3.24)
project(grimdark LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  message(FATAL_ERROR "grimdark must be built with MSVC (it links against the game's MSVC ABI)")
endif()
# Static CRT: the DLL is injected into a process whose own VC runtime is older than this toolset's, and a
# player has no reason to have the 2022 redistributable. Everything here (Detours, SQLite, the core, the
# injector) builds with /MT; prism.dll carries its own runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
add_compile_options(/utf-8 /W4 /permissive- /Zc:__cplusplus)

# --- Microsoft Detours (vendored, built from source) ---
set(DETOURS_SRC ${CMAKE_SOURCE_DIR}/third_party/Detours/src)
add_library(detours STATIC
  ${DETOURS_SRC}/detours.cpp ${DETOURS_SRC}/modules.cpp ${DETOURS_SRC}/disasm.cpp
  ${DETOURS_SRC}/image.cpp ${DETOURS_SRC}/creatwth.cpp)
target_include_directories(detours PUBLIC ${DETOURS_SRC})
target_compile_options(detours PRIVATE /W0)

# --- SQLite (vendored amalgamation, public domain; the rooms dbs are read-only at runtime) ---
add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite)
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION SQLITE_DEFAULT_MEMSTATUS=0
  SQLITE_OMIT_DEPRECATED SQLITE_DQS=0 SQLITE_OMIT_SHARED_CACHE)
target_compile_options(sqlite3 PRIVATE /W0)

# --- Prism (prebuilt SDK release) ---
set(PRISM_SDK ${CMAKE_SOURCE_DIR}/third_party/prism-bin/prism-sdk-v0.18.1)
set(PRISM_BIN ${PRISM_SDK}/windows/x64/dynamic/release)
set(PRISM_LIB ${PRISM_BIN}/lib/prism.lib)
set(PRISM_DLL ${PRISM_BIN}/bin/prism.dll)
if(NOT EXISTS ${PRISM_LIB} OR NOT EXISTS ${PRISM_DLL})
  message(FATAL_ERROR "prism SDK not found under ${PRISM_BIN}")
endif()
get_filename_component(PRISM_DLL_NAME ${PRISM_DLL} NAME)

# --- engine-free core: message building, strings, nav graph, typeahead (unit-tested, no Windows headers) ---
file(GLOB GDCORE_SOURCES src/core/*.cpp)
add_library(gdcore STATIC ${GDCORE_SOURCES})
target_include_directories(gdcore PUBLIC src)

file(GLOB GDCORE_TESTS tests/*.cpp)
add_executable(gdcore_tests ${GDCORE_TESTS})
target_include_directories(gdcore_tests PRIVATE third_party)
target_link_libraries(gdcore_tests PRIVATE gdcore)
add_custom_target(check COMMAND gdcore_tests DEPENDS gdcore_tests)

# --- the mod's own version, embedded at configure time (2026-10-02) ---
# CI sets GRIMDARK_VERSION (a tag "v0.4.0" or "ci-<short sha>") before configuring; a local build is "dev" (the update
# check stays silent). The same string goes to build/version.txt, which tools/package.py ships as grimdark/version.txt,
# so the DLL and the installer can never disagree.
if(DEFINED ENV{GRIMDARK_VERSION} AND NOT "$ENV{GRIMDARK_VERSION}" STREQUAL "")
  set(GRIMDARK_VERSION "$ENV{GRIMDARK_VERSION}")
else()
  set(GRIMDARK_VERSION "dev")
endif()
message(STATUS "grimdark version: ${GRIMDARK_VERSION}")
# Whose releases the update check asks for, and the name it speaks (CI: GRIMDARK_REPO = the repo it runs in, so a fork
# checks its own releases; GRIMDARK_APP_NAME = the installed name). The defaults are this repo's.
if(DEFINED ENV{GRIMDARK_REPO} AND NOT "$ENV{GRIMDARK_REPO}" STREQUAL "")
  set(GRIMDARK_REPO "$ENV{GRIMDARK_REPO}")
else()
  set(GRIMDARK_REPO "ahicks92/grimdark")
endif()
if(DEFINED ENV{GRIMDARK_APP_NAME} AND NOT "$ENV{GRIMDARK_APP_NAME}" STREQUAL "")
  set(GRIMDARK_APP_NAME "$ENV{GRIMDARK_APP_NAME}")
else()
  set(GRIMDARK_APP_NAME "Grimdark")
endif()
message(STATUS "grimdark repo: ${GRIMDARK_REPO}, app name: ${GRIMDARK_APP_NAME}")
configure_file(src/grimdark_version.h.in ${CMAKE_BINARY_DIR}/generated/grimdark_version.h @ONLY)
# The controller controls list (data/controller_controls.txt), embedded in the DLL; editing it reconfigures.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/data/controller_controls.txt)
file(READ ${CMAKE_SOURCE_DIR}/data/controller_controls.txt GRIMDARK_PAD_CONTROLS)
configure_file(src/pad_controls_text.h.in ${CMAKE_BINARY_DIR}/generated/pad_controls_text.h @ONLY)
file(WRITE ${CMAKE_BINARY_DIR}/version.txt "${GRIMDARK_VERSION}
")

# --- the injected DLL ---
add_library(grimdark SHARED
  src/dllmain.cpp src/log.cpp src/crash.cpp src/speech.cpp src/hooks.cpp src/textcap.cpp src/devserver.cpp src/audio_mute.cpp src/app.cpp src/screens/main_menu.cpp src/screens/create_character.cpp src/screens/difficulty_select.cpp
  src/audio.cpp src/world.cpp src/screens/in_game.cpp src/screens/message_box.cpp src/screens/pause_menu.cpp
  src/screens/loading.cpp src/screens/tip.cpp src/screens/conversation.cpp src/exe_ui.cpp src/screens/delete_character.cpp src/screens/options.cpp
  src/voice.cpp src/tts_onecore.cpp src/combat.cpp src/casts.cpp src/projectiles.cpp src/telegraph.cpp src/settings.cpp src/cues.cpp src/screens/announcements.cpp src/screens/cue_settings.cpp src/screens/mod_menu.cpp src/screens/sound_glossary.cpp src/notify.cpp src/quest_rewards.cpp src/gameapi.cpp src/gameapi_items.cpp src/gameapi_skills.cpp src/gameapi_devotion.cpp src/gameapi_pets.cpp src/screens/pets.cpp src/gameapi_loot.cpp src/screens/loot_filter.cpp src/gameapi_crafting.cpp src/screens/crafting.cpp src/screens/inventor.cpp
  src/screens/codex.cpp src/screens/factions.cpp src/screens/inventory.cpp src/screens/skills.cpp src/screens/quickbar.cpp
  src/screens/modals.cpp src/screens/vendor.cpp src/db.cpp src/rooms.cpp src/sonar.cpp src/hazard.cpp src/screens/riftgate.cpp
  src/screens/list_picker.cpp src/screens/count_prompt.cpp src/screens/reward_list.cpp src/screens/hotbar_manager.cpp src/screens/map_markers.cpp
  src/screens/mod_options.cpp src/version_gate.cpp src/update_check.cpp src/gamepad.cpp src/gamepad_input.cpp src/screens/pad_controls.cpp)
# C++/WinRT (OneCore speech in src/tts_onecore.cpp): the projection headers are heavy, and windowsapp.lib's
# api-ms-win-* imports are delay-loaded so a machine without them loses combat speech, not the whole mod.
set_source_files_properties(src/tts_onecore.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
# An SDK older than ~10.0.22000 (Build Tools' default 10.0.20348) ships a C++/WinRT base.h that includes
# <experimental/coroutine>, which MSVC 14.44 rejects under /std:c++20. On such an SDK the pinned C++/WinRT NuGet
# package is fetched once and the projection generated from that same SDK's metadata into the build tree (~55 MB);
# a newer SDK (CI) uses its own headers and nothing is downloaded.
set(GD_SDK_VER "$ENV{WindowsSDKVersion}")
string(REGEX REPLACE "[\\/]+$" "" GD_SDK_VER "${GD_SDK_VER}")
set(GD_SDK_BASE_H "$ENV{WindowsSdkDir}Include/${GD_SDK_VER}/cppwinrt/winrt/base.h")
set(GD_SDK_CPPWINRT_OK OFF)
if(EXISTS "${GD_SDK_BASE_H}")
  file(STRINGS "${GD_SDK_BASE_H}" GD_STD_COROUTINE REGEX "#include <coroutine>")
  if(GD_STD_COROUTINE)
    set(GD_SDK_CPPWINRT_OK ON)
  endif()
endif()
if(NOT GD_SDK_CPPWINRT_OK)
  set(GD_CPPWINRT_VERSION 2.0.240405.15)
  set(GD_CPPWINRT_DIR ${CMAKE_BINARY_DIR}/cppwinrt)
  if(NOT EXISTS ${GD_CPPWINRT_DIR}/include/winrt/base.h)
    message(STATUS "Windows SDK ${GD_SDK_VER} has a pre-C++20 C++/WinRT; generating headers with C++/WinRT ${GD_CPPWINRT_VERSION}")
    set(GD_CPPWINRT_PKG ${GD_CPPWINRT_DIR}/Microsoft.Windows.CppWinRT.${GD_CPPWINRT_VERSION}.nupkg)
    file(DOWNLOAD https://www.nuget.org/api/v2/package/Microsoft.Windows.CppWinRT/${GD_CPPWINRT_VERSION} ${GD_CPPWINRT_PKG}
      EXPECTED_HASH SHA256=e889007b5d9235931e7340ddf737d2c346eebdd23c619f1f4f2426a2aae47180 TLS_VERIFY ON STATUS GD_DL)
    list(GET GD_DL 0 GD_DL_CODE)
    if(NOT GD_DL_CODE EQUAL 0)
      message(FATAL_ERROR "C++/WinRT download failed: ${GD_DL}")
    endif()
    file(ARCHIVE_EXTRACT INPUT ${GD_CPPWINRT_PKG} DESTINATION ${GD_CPPWINRT_DIR}/pkg PATTERNS bin/cppwinrt.exe)
    execute_process(COMMAND ${GD_CPPWINRT_DIR}/pkg/bin/cppwinrt.exe -input ${GD_SDK_VER} -output ${GD_CPPWINRT_DIR}/include
      RESULT_VARIABLE GD_GEN OUTPUT_QUIET)
    if(NOT GD_GEN EQUAL 0 OR NOT EXISTS ${GD_CPPWINRT_DIR}/include/winrt/base.h)
      message(FATAL_ERROR "cppwinrt.exe failed (${GD_GEN}) generating headers for SDK ${GD_SDK_VER}")
    endif()
  endif()
  # /I paths are searched before the SDK's INCLUDE entries, so these headers win for this one file.
  set_property(SOURCE src/tts_onecore.cpp APPEND PROPERTY INCLUDE_DIRECTORIES ${GD_CPPWINRT_DIR}/include)
endif()
target_include_directories(grimdark PRIVATE src ${PRISM_SDK}/include third_party/miniaudio ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(grimdark PRIVATE gdcore detours sqlite3 ${PRISM_LIB} user32 ws2_32 ole32 delayimp windowsapp winhttp setupapi hid)
target_link_options(grimdark PRIVATE /DELAYLOAD:${PRISM_DLL_NAME}
  /DELAYLOAD:api-ms-win-core-winrt-l1-1-0.dll /DELAYLOAD:api-ms-win-core-winrt-error-l1-1-1.dll /DELAYLOAD:api-ms-win-core-winrt-string-l1-1-0.dll)
target_compile_definitions(grimdark PRIVATE PRISM_DLL_NAME="${PRISM_DLL_NAME}")
add_custom_command(TARGET grimdark POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PRISM_DLL} $<TARGET_FILE_DIR:grimdark>
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:grimdark>/assets)

# --- the rooms databases: built from data/rooms (git-friendly text + compressed grids) by tools/rooms_pack.py ---
# The DLL loads rooms.db / rooms_base.db from next to itself; the packer is stdlib-only Python (no uv, no numpy),
# so CI and a fresh checkout build them without the game or the authoring toolchain. Re-runs when any data file or
# the packer changes. The authoring tools work on unpacked copies in build/rooms/ (docs/rooms.md "Data layout").
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB_RECURSE ROOMS_DATA CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/data/rooms/*)
set(ROOMS_DBS ${CMAKE_BINARY_DIR}/assets/rooms.db ${CMAKE_BINARY_DIR}/assets/rooms_base.db)
add_custom_command(OUTPUT ${ROOMS_DBS}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/rooms_pack.py build --out ${CMAKE_BINARY_DIR}/assets
  DEPENDS ${ROOMS_DATA} ${CMAKE_SOURCE_DIR}/tools/rooms_pack.py ${CMAKE_SOURCE_DIR}/tools/gdmap/roomsdb.py
  COMMENT "packing data/rooms -> assets/rooms.db + rooms_base.db")
add_custom_target(rooms_dbs ALL DEPENDS ${ROOMS_DBS})
add_dependencies(grimdark rooms_dbs)

# --- the injector ---
add_executable(gdinject src/inject/main.cpp)
target_link_libraries(gdinject PRIVATE psapi)

# --- the player's launcher (console app; finds the Steam install, fakes Steam's environment, injects at start) ---
add_executable(gdlaunch src/launch/main.cpp)
target_include_directories(gdlaunch PRIVATE src)   # core/game_settings.h (header-only)
target_link_libraries(gdlaunch PRIVATE shell32 ole32 advapi32)

cmake_minimum_required(VERSION 3.24)
project(gbemu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GB_WARNING_FLAGS -Wall -Wextra -Werror)

# sanitizers unavailable on mingw; ci covers them on linux
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

set(GB_CORE_SOURCES core/apu.cpp core/bus.cpp core/cartridge.cpp core/cpu.cpp core/gameboy.cpp
                    core/joypad.cpp core/mapper_mbc1.cpp core/mapper_mbc3.cpp core/mapper_mbc5.cpp
                    core/mapper_rom.cpp
                    core/ppu.cpp core/serial.cpp core/timer.cpp core/trace.cpp)

add_library(gbcore STATIC ${GB_CORE_SOURCES})
target_include_directories(gbcore PUBLIC core)
target_compile_options(gbcore PRIVATE ${GB_WARNING_FLAGS})

if(EMSCRIPTEN)
    # milestone 14: same frontend through emscripten's sdl2 port; the core compiles unchanged
    add_executable(gbemu-sdl frontend/sdl/main.cpp)
    target_compile_options(gbemu-sdl PRIVATE ${GB_WARNING_FLAGS} -sUSE_SDL=2)
    target_link_libraries(gbemu-sdl PRIVATE gbcore)
    set_target_properties(gbemu-sdl PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    # emscripten default stack is 64kb; native code assumes megabytes
    target_link_options(gbemu-sdl PRIVATE -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sMINIFY_HTML=0
        -sSTACK_SIZE=1048576
        "-sEXPORTED_FUNCTIONS=_main,_wasm_load_rom,_malloc,_free"
        "-sEXPORTED_RUNTIME_METHODS=ccall"
        "--shell-file=${CMAKE_SOURCE_DIR}/frontend/wasm/shell.html"
        "--embed-file=${CMAKE_SOURCE_DIR}/assets/roms/tetris.gb@demo.gb")
else()
    # macos: homebrew installs outside cmake's default search prefixes
    if(APPLE)
        foreach(brew_prefix /opt/homebrew /usr/local)
            if(EXISTS "${brew_prefix}/lib/cmake/SDL2")
                list(APPEND CMAKE_PREFIX_PATH "${brew_prefix}")
            endif()
        endforeach()
    endif()
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        add_executable(gbemu-sdl frontend/sdl/main.cpp)
        target_compile_options(gbemu-sdl PRIVATE ${GB_WARNING_FLAGS})
        # mingw needs sdl2main linked before sdl2
        if(TARGET SDL2::SDL2main)
            target_link_libraries(gbemu-sdl PRIVATE SDL2::SDL2main)
        endif()
        target_link_libraries(gbemu-sdl PRIVATE gbcore SDL2::SDL2)
    else()
        message(WARNING "sdl2 not found; skipping gbemu-sdl target")
    endif()
endif()

if(EMSCRIPTEN)
    # tests, harness, and fuzzers are host-only
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(catch2)

# milestone 18 sub-milestone 2: the mario level compiler runs at configure/build time, not via gbdk
find_package(Python3 COMPONENTS Interpreter)

enable_testing()
# palette helper is header-only frontend code, unit-tested per milestone 08
add_executable(gbcore_tests tests/unit/apu_test.cpp tests/unit/bus_test.cpp tests/unit/cartridge_test.cpp
                            tests/unit/cpu_alu_test.cpp tests/unit/cpu_blargg_smoke_test.cpp
                            tests/unit/cpu_decode_test.cpp tests/unit/cpu_test.cpp
                            tests/unit/gameboy_test.cpp tests/unit/joypad_test.cpp
                            tests/unit/mapper_mbc1_test.cpp tests/unit/mapper_mbc3_test.cpp
                            tests/unit/mapper_mbc5_test.cpp tests/unit/mapper_rom_test.cpp
                            tests/unit/palette_test.cpp tests/unit/ppu_test.cpp
                            tests/unit/smoke_test.cpp tests/unit/state_test.cpp
                            tests/unit/timer_test.cpp
                            tests/unit/trace_test.cpp)
target_compile_options(gbcore_tests PRIVATE ${GB_WARNING_FLAGS})
target_include_directories(gbcore_tests PRIVATE frontend/sdl)
target_link_libraries(gbcore_tests PRIVATE gbcore Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(gbcore_tests)

# milestone 15: the flappy rom needs gbdk-2020's sm83 toolchain; absent, skip like a missing sdl2
set(GBDK_HOME "$ENV{GBDK_HOME}" CACHE PATH "path to a gbdk-2020 install")
set(GB_LCC "${GBDK_HOME}/bin/lcc${CMAKE_EXECUTABLE_SUFFIX}")
if(GBDK_HOME AND EXISTS "${GB_LCC}")
    set(GB_FLAPPY_ROM ${CMAKE_BINARY_DIR}/flappy.gb)
    # -yt0x03 mbc1+ram+battery, -ya1 one 8kb sram bank, -yn the 0x134 title
    add_custom_command(
        OUTPUT ${GB_FLAPPY_ROM}
        COMMAND ${GB_LCC} -Wm-yt0x03 -Wm-ya1 -Wm-ynFLAPPY -o ${GB_FLAPPY_ROM}
                ${CMAKE_SOURCE_DIR}/games/flappy/main.c
                ${CMAKE_SOURCE_DIR}/games/flappy/bird.c
                ${CMAKE_SOURCE_DIR}/games/flappy/world.c
                ${CMAKE_SOURCE_DIR}/games/flappy/hud.c
                ${CMAKE_SOURCE_DIR}/games/flappy/save.c
                ${CMAKE_SOURCE_DIR}/games/flappy/sfx.c
                ${CMAKE_SOURCE_DIR}/games/flappy/assets.c
        DEPENDS games/flappy/main.c games/flappy/bird.c games/flappy/bird.h
                games/flappy/world.c games/flappy/world.h
                games/flappy/hud.c games/flappy/hud.h
                games/flappy/save.c games/flappy/save.h
                games/flappy/sfx.c games/flappy/sfx.h
                games/flappy/assets.c games/flappy/assets.h games/flappy/flappy.h
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "building flappy.gb with gbdk lcc"
        VERBATIM)
    add_custom_target(flappy DEPENDS ${GB_FLAPPY_ROM})

    add_executable(flappy_tests games/flappy/tests/flappy_test.cpp)
    target_compile_options(flappy_tests PRIVATE ${GB_WARNING_FLAGS})
    target_compile_definitions(flappy_tests PRIVATE FLAPPY_ROM_PATH="${GB_FLAPPY_ROM}")
    target_link_libraries(flappy_tests PRIVATE gbcore Catch2::Catch2WithMain)
    add_dependencies(flappy_tests flappy)
    catch_discover_tests(flappy_tests)
    # milestone 16: second gbdk rom, same mbc1+ram+battery cart shape as flappy
    set(GB_CROSSY_ROM ${CMAKE_BINARY_DIR}/crossy.gb)
    add_custom_command(
        OUTPUT ${GB_CROSSY_ROM}
        COMMAND ${GB_LCC} -Wm-yt0x03 -Wm-ya1 -Wm-ynCROSSY -o ${GB_CROSSY_ROM}
                ${CMAKE_SOURCE_DIR}/games/crossy/main.c
                ${CMAKE_SOURCE_DIR}/games/crossy/terrain.c
                ${CMAKE_SOURCE_DIR}/games/crossy/chick.c
                ${CMAKE_SOURCE_DIR}/games/crossy/movers.c
                ${CMAKE_SOURCE_DIR}/games/crossy/eagle.c
                ${CMAKE_SOURCE_DIR}/games/crossy/hud.c
                ${CMAKE_SOURCE_DIR}/games/crossy/save.c
                ${CMAKE_SOURCE_DIR}/games/crossy/sfx.c
                ${CMAKE_SOURCE_DIR}/games/crossy/assets.c
        DEPENDS games/crossy/main.c games/crossy/crossy.h
                games/crossy/terrain.c games/crossy/terrain.h
                games/crossy/chick.c games/crossy/chick.h
                games/crossy/movers.c games/crossy/movers.h
                games/crossy/eagle.c games/crossy/eagle.h
                games/crossy/hud.c games/crossy/hud.h
                games/crossy/save.c games/crossy/save.h
                games/crossy/sfx.c games/crossy/sfx.h
                games/crossy/assets.c games/crossy/assets.h
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "building crossy.gb with gbdk lcc"
        VERBATIM)
    add_custom_target(crossy DEPENDS ${GB_CROSSY_ROM})

    add_executable(crossy_tests games/crossy/tests/crossy_test.cpp)
    target_compile_options(crossy_tests PRIVATE ${GB_WARNING_FLAGS})
    target_compile_definitions(crossy_tests PRIVATE CROSSY_ROM_PATH="${GB_CROSSY_ROM}")
    target_link_libraries(crossy_tests PRIVATE gbcore Catch2::Catch2WithMain)
    add_dependencies(crossy_tests crossy)
    catch_discover_tests(crossy_tests)

    # milestone 22: first-party cgb tetris, cgb-only mbc1+ram+battery, 32kb rom
    set(GB_TETRIS_ROM ${CMAKE_BINARY_DIR}/tetris.gb)
    add_custom_command(
        OUTPUT ${GB_TETRIS_ROM}
        COMMAND ${GB_LCC} -Wm-yC -Wm-yt0x03 -Wm-ya1 -Wm-ynTETRIS -o ${GB_TETRIS_ROM}
                ${CMAKE_SOURCE_DIR}/games/tetris/main.c
                ${CMAKE_SOURCE_DIR}/games/tetris/piece.c
                ${CMAKE_SOURCE_DIR}/games/tetris/pieces.c
                ${CMAKE_SOURCE_DIR}/games/tetris/well.c
                ${CMAKE_SOURCE_DIR}/games/tetris/panel.c
                ${CMAKE_SOURCE_DIR}/games/tetris/save.c
                ${CMAKE_SOURCE_DIR}/games/tetris/assets.c
        DEPENDS games/tetris/main.c games/tetris/tetris.h
                games/tetris/piece.c games/tetris/piece.h
                games/tetris/pieces.c games/tetris/pieces.h
                games/tetris/well.c games/tetris/well.h
                games/tetris/panel.c games/tetris/panel.h
                games/tetris/save.c games/tetris/save.h
                games/tetris/assets.c games/tetris/assets.h
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "building tetris.gb with gbdk lcc"
        VERBATIM)
    add_custom_target(tetris DEPENDS ${GB_TETRIS_ROM})

    add_executable(tetris_tests games/tetris/tests/tetris_test.cpp)
    target_compile_options(tetris_tests PRIVATE ${GB_WARNING_FLAGS})
    target_compile_definitions(tetris_tests PRIVATE TETRIS_ROM_PATH="${GB_TETRIS_ROM}")
    target_link_libraries(tetris_tests PRIVATE gbcore Catch2::Catch2WithMain)
    add_dependencies(tetris_tests tetris)
    catch_discover_tests(tetris_tests)

    # milestone 18: first cgb rom, cgb-only mbc5+ram+battery, 8 banks (128kb), 8kb sram
    # sub-milestone 2: the level bible compiles to banked c data ahead of the gbdk build
    if(NOT Python3_FOUND)
        message(WARNING "python3 not found; skipping mario rom target (needs the level compiler)")
    else()
        set(GB_MARIO_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
        set(GB_MARIO_LEVEL_C ${GB_MARIO_GEN_DIR}/level_1_1.c)
        set(GB_MARIO_LEVEL_H ${GB_MARIO_GEN_DIR}/level_1_1.h)
        set(GB_MARIO_LEVEL_PROBES_H ${GB_MARIO_GEN_DIR}/level_1_1_probes.h)
        add_custom_command(
            OUTPUT ${GB_MARIO_LEVEL_C} ${GB_MARIO_LEVEL_H} ${GB_MARIO_LEVEL_PROBES_H}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/games/mario/tools/compile_level.py
                    ${CMAKE_SOURCE_DIR}/games/mario/data/level-1-1.json ${GB_MARIO_GEN_DIR}
            DEPENDS games/mario/tools/compile_level.py games/mario/data/level-1-1.json
            COMMENT "compiling mario level 1-1 bible json to banked c data"
            VERBATIM)
        add_custom_target(mario_level DEPENDS ${GB_MARIO_LEVEL_C} ${GB_MARIO_LEVEL_H}
                                               ${GB_MARIO_LEVEL_PROBES_H})

        set(GB_MARIO_ROM ${CMAKE_BINARY_DIR}/mario.gbc)
        add_custom_command(
            OUTPUT ${GB_MARIO_ROM}
            COMMAND ${GB_LCC} -Wm-yC -Wm-yt0x1B -Wm-ya1 -Wm-ynMARIO -Wm-yo8 -I${GB_MARIO_GEN_DIR}
                    -o ${GB_MARIO_ROM}
                    ${CMAKE_SOURCE_DIR}/games/mario/src/main.c
                    ${CMAKE_SOURCE_DIR}/games/mario/src/terrain.c
                    ${CMAKE_SOURCE_DIR}/games/mario/src/assets.c
                    ${GB_MARIO_LEVEL_C}
            DEPENDS games/mario/src/main.c games/mario/src/mario.h
                    games/mario/src/terrain.c games/mario/src/terrain.h
                    games/mario/src/assets.c games/mario/src/assets.h
                    ${GB_MARIO_LEVEL_C} ${GB_MARIO_LEVEL_H} ${GB_MARIO_LEVEL_PROBES_H}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "building mario.gbc with gbdk lcc"
            VERBATIM)
        add_custom_target(mario DEPENDS ${GB_MARIO_ROM})
        add_dependencies(mario mario_level)

        add_executable(mario_tests games/mario/tests/mario_test.cpp)
        target_compile_options(mario_tests PRIVATE ${GB_WARNING_FLAGS})
        target_compile_definitions(mario_tests PRIVATE MARIO_ROM_PATH="${GB_MARIO_ROM}")
        target_include_directories(mario_tests PRIVATE ${GB_MARIO_GEN_DIR})
        target_link_libraries(mario_tests PRIVATE gbcore Catch2::Catch2WithMain)
        add_dependencies(mario_tests mario mario_level)
        catch_discover_tests(mario_tests)
    endif()
else()
    message(WARNING "gbdk not found (set GBDK_HOME); skipping flappy/crossy rom targets")
endif()

add_executable(gbrom_harness tests/roms/harness.cpp)
target_compile_options(gbrom_harness PRIVATE ${GB_WARNING_FLAGS})
target_link_libraries(gbrom_harness PRIVATE gbcore)

option(GB_BUILD_FUZZERS "build libfuzzer targets (clang only)" OFF)
if(GB_BUILD_FUZZERS)
    foreach(fuzzer fuzz_header fuzz_savestate)
        add_executable(${fuzzer} tests/fuzz/${fuzzer}.cpp ${GB_CORE_SOURCES})
        target_include_directories(${fuzzer} PRIVATE core)
        target_compile_options(${fuzzer} PRIVATE -fsanitize=fuzzer,address,undefined ${GB_WARNING_FLAGS})
        target_link_options(${fuzzer} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
endif()

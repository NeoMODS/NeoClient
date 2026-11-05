cmake_minimum_required(VERSION 3.24)

project(NeoClient VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_compile_definitions(
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
    NEO_VERSION="1.0.0"
    NEO_MC_VERSION="26.3"
)

if(MSVC)
    add_compile_options(/utf-8 /permissive- /W4)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC opengl32)

# ---------------------------------------------------------------------------
# MinHook
# ---------------------------------------------------------------------------
add_library(minhook STATIC
    third_party/minhook/src/buffer.c
    third_party/minhook/src/hook.c
    third_party/minhook/src/trampoline.c
)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_sources(minhook PRIVATE third_party/minhook/src/hde/hde64.c)
else()
    target_sources(minhook PRIVATE third_party/minhook/src/hde/hde32.c)
endif()
target_include_directories(minhook PUBLIC third_party/minhook/include)
if(NOT MSVC)
    target_compile_options(minhook PRIVATE -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Neo core (UI, modules, JNI, platform helpers)
# ---------------------------------------------------------------------------
add_library(neo_core STATIC
    src/core/client.cpp
    src/core/config.cpp
    src/core/keys.cpp
    src/core/logger.cpp
    src/core/paths.cpp
    src/game/jni_game.cpp
    src/game/jvm.cpp
    src/game/null_game.cpp
    src/modules/builtin_modules.cpp
    src/modules/module.cpp
    src/modules/module_manager.cpp
    src/ui/fonts.cpp
    src/ui/gui.cpp
    src/ui/icons.cpp
    src/ui/settings.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
)
target_include_directories(neo_core PUBLIC
    src
    third_party
    third_party/stb
    third_party/jni
    third_party/jni/win32
)
target_link_libraries(neo_core PUBLIC
    imgui
    minhook
    opengl32
    gdi32
    user32
    shell32
    shlwapi
    advapi32
    ole32
    dwmapi
)
target_compile_definitions(neo_core PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# ---------------------------------------------------------------------------
# Client DLL (injected into Minecraft)
# ---------------------------------------------------------------------------
add_library(NeoClient SHARED
    src/platform/dllmain.cpp
    src/platform/overlay_gl.cpp
)
target_link_libraries(NeoClient PRIVATE neo_core)
set_target_properties(NeoClient PROPERTIES
    PREFIX ""
    OUTPUT_NAME "NeoClient"
    SUFFIX ".dll"
)
if(NOT MSVC)
    target_link_options(NeoClient PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_custom_command(TARGET NeoClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/fonts"
        "${CMAKE_BINARY_DIR}/bin/assets/fonts"
)

# ---------------------------------------------------------------------------
# Injector
# ---------------------------------------------------------------------------
add_executable(NeoInjector src/platform/injector.cpp)
target_link_libraries(NeoInjector PRIVATE shell32)
if(NOT MSVC)
    target_link_options(NeoInjector PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# Standalone UI preview (same renderer, fake game state)
# ---------------------------------------------------------------------------
add_executable(NeoPreview WIN32
    src/platform/preview.cpp
)
target_link_libraries(NeoPreview PRIVATE neo_core)
if(NOT MSVC)
    target_link_options(NeoPreview PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

add_custom_command(TARGET NeoPreview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:NeoPreview>/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets/fonts"
        "$<TARGET_FILE_DIR:NeoPreview>/assets/fonts"
)

message(STATUS "NeoClient ${PROJECT_VERSION} - Minecraft ${NEO_MC_VERSION}")

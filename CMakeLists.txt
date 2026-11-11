cmake_minimum_required(VERSION 3.22)

if(APPLE)
    # JUCE 9 requires a modern deployment target. Keep this configurable for distribution builds.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS version")
endif()

project(DarkGrain VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)

enable_testing()

# Optional local JUCE checkout. When omitted, JUCE is fetched at configure time.
set(DARKGRAIN_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
set(DARKGRAIN_JUCE_VERSION "9.0.5" CACHE STRING "JUCE git tag used when DARKGRAIN_JUCE_PATH is empty")

if(DARKGRAIN_JUCE_PATH)
    if(NOT EXISTS "${DARKGRAIN_JUCE_PATH}/CMakeLists.txt")
        message(FATAL_ERROR "DARKGRAIN_JUCE_PATH does not point to a JUCE checkout: ${DARKGRAIN_JUCE_PATH}")
    endif()

    add_subdirectory("${DARKGRAIN_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)

    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${DARKGRAIN_JUCE_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(DARKGRAIN_FORMATS VST3 AU Standalone)
elseif(WIN32)
    set(DARKGRAIN_FORMATS VST3 Standalone)
else()
    message(FATAL_ERROR "DarkGrain targets Windows and macOS only.")
endif()

juce_add_plugin(DarkGrain
    COMPANY_NAME "Prog Filthi"
    BUNDLE_ID "com.progfilthi.darkgrain"
    PLUGIN_MANUFACTURER_CODE Pfil
    PLUGIN_CODE Dgrn
    FORMATS ${DARKGRAIN_FORMATS}
    PRODUCT_NAME "DarkGrain"
    DESCRIPTION "Granular texture and dark reverb effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

target_sources(DarkGrain
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/dsp/DarkReverb.cpp
        source/dsp/Filter.cpp
        source/dsp/GrainEngine.cpp
        source/dsp/Limiter.cpp
        source/dsp/ParticleEngine.cpp
        source/dsp/Saturation.cpp
        source/dsp/StereoWidth.cpp
        source/parameters/ParameterLayout.cpp
        source/state/PresetManager.cpp
)

target_include_directories(DarkGrain PUBLIC source)

target_compile_definitions(DarkGrain
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        DARKGRAIN_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(DarkGrain
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

darkgrain_set_warnings(DarkGrain)

option(DARKGRAIN_BUILD_TESTS "Build the DarkGrain unit test runner" ON)

if(DARKGRAIN_BUILD_TESTS)
    juce_add_console_app(DarkGrainTests PRODUCT_NAME "DarkGrainTests")

    target_sources(DarkGrainTests
        PRIVATE
            source/PluginProcessor.cpp
            source/PluginEditor.cpp
            source/dsp/DarkReverb.cpp
            source/dsp/Filter.cpp
            source/dsp/GrainEngine.cpp
            source/dsp/Limiter.cpp
            source/dsp/ParticleEngine.cpp
            source/dsp/Saturation.cpp
            source/dsp/StereoWidth.cpp
            source/parameters/ParameterLayout.cpp
            source/state/PresetManager.cpp
            tests/TestRunner.cpp
            tests/EditorAndPresetTests.cpp
            tests/GrainEngineTests.cpp
            tests/ParameterTests.cpp
            tests/ProcessorTests.cpp
            tests/ReverbTests.cpp
            tests/ScaleTests.cpp
    )

    target_include_directories(DarkGrainTests PRIVATE source)

    target_link_libraries(DarkGrainTests
        PRIVATE
            juce::juce_audio_processors
            juce::juce_gui_basics
            juce::juce_recommended_config_flags
    )

    target_compile_definitions(DarkGrainTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_UNIT_TESTS=1
            DARKGRAIN_VERSION="${PROJECT_VERSION}"
    )

    darkgrain_set_warnings(DarkGrainTests)

    add_test(NAME DarkGrainTests COMMAND DarkGrainTests)
endif()

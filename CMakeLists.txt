cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) when building on macOS
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(EchoVault VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE ------------------------------------------------------------------
# Use a local JUCE checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Plugin ----------------------------------------------------------------
set(EV_FORMATS VST3)
if(APPLE)
    list(APPEND EV_FORMATS AU)
endif()

juce_add_plugin(EchoVault
    COMPANY_NAME                "Vault Audio"
    BUNDLE_ID                   "com.vaultaudio.echovault"
    PLUGIN_MANUFACTURER_CODE    Vlta
    PLUGIN_CODE                 EcVt
    PRODUCT_NAME                "Echo Vault"
    FORMATS                     ${EV_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Reverb"
    AU_MAIN_TYPE                "kAudioUnitType_Effect")

target_sources(EchoVault PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(EchoVault PRIVATE Source)

target_compile_definitions(EchoVault PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EchoVault
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        )

# ---- Offline DSP test (not part of the plugin) -----------------------------
option(EV_BUILD_TESTS "Build the offline render test" OFF)
if(EV_BUILD_TESTS)
    add_executable(ev_render_test Tests/RenderTest.cpp)
    target_include_directories(ev_render_test PRIVATE Source)
endif()

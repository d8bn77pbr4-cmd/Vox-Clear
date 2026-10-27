cmake_minimum_required(VERSION 3.22)
project(VoxClear VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    set(VOX_FORMATS VST3 AU)
else()
    set(VOX_FORMATS VST3)
endif()

set(VOX_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DeClicker.cpp
    Source/SpectralDenoiser.cpp)

juce_add_plugin(VoxClear
    COMPANY_NAME "VoxClearAudio"
    PLUGIN_MANUFACTURER_CODE Voxc
    PLUGIN_CODE VxCl
    FORMATS ${VOX_FORMATS}
    PRODUCT_NAME "Vox Clear"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Restoration Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(VoxClear PRIVATE ${VOX_SOURCES})

target_compile_definitions(VoxClear PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VoxClear
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(VOX_BUILD_TESTS "Build the offline test runner" OFF)
if(VOX_BUILD_TESTS)
    juce_add_console_app(VoxTests PRODUCT_NAME "VoxTests")
    target_sources(VoxTests PRIVATE tests/TestMain.cpp ${VOX_SOURCES})
    target_compile_definitions(VoxTests PRIVATE JucePlugin_Name="Vox Clear" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(VoxTests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

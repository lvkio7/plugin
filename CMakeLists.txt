cmake_minimum_required(VERSION 3.22)
project(Nosfera VERSION 0.3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Baixa o JUCE automaticamente (ou usa uma cópia local com -DJUCE_DIR=caminho).
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Nosfera
    COMPANY_NAME "Nosfera Audio"
    PRODUCT_NAME "Nosfera"
    PLUGIN_MANUFACTURER_CODE Nsfa
    PLUGIN_CODE Nsfr
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone)

juce_add_binary_data(NosferaUI SOURCES ui/index.html)

target_sources(Nosfera PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Nosfera PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Nosfera
    PRIVATE
        NosferaUI
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Teste do motor de som (não faz parte do plugin).
add_executable(render_test tests/render_test.cpp)

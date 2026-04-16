cmake_minimum_required(VERSION 3.22)

project(Eqe VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MIXOLVE_EQE_USE_LOCAL_JUCE "Use a local JUCE checkout via JUCE_DIR" OFF)

if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum iOS version" FORCE)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "iOS architectures" FORCE)
    else()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    endif()
endif()

if(MIXOLVE_EQE_USE_LOCAL_JUCE)
    if(NOT DEFINED JUCE_DIR)
        message(FATAL_ERROR "Set -DJUCE_DIR=/absolute/path/to/JUCE when MIXOLVE_EQE_USE_LOCAL_JUCE=ON")
    endif()

    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)

    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
    )

    FetchContent_MakeAvailable(juce)
endif()

juce_add_binary_data(eqe_assets
    SOURCES
        assets/fonts/SometypeMono-Bold.ttf
        assets/fonts/SometypeMono-Regular.ttf
        assets/fonts/SometypeMono-OFL.txt
)

set(EQE_PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND EQE_PLUGIN_FORMATS AUv3 Standalone)
endif()

set(EQE_PLUGIN_ARGS
    COMPANY_NAME "mixolve"
    BUNDLE_ID "com.mixolve.eqe"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Mxol
    PLUGIN_CODE Eqe1
    FORMATS ${EQE_PLUGIN_FORMATS}
    PRODUCT_NAME "eqe"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "This app needs microphone access to process audio."
)

juce_add_plugin(Eqe ${EQE_PLUGIN_ARGS})

juce_generate_juce_header(Eqe)

target_sources(Eqe
    PRIVATE
        source/PluginEditor.cpp
        source/PluginEditor.h
        source/PluginProcessor.cpp
        source/PluginProcessor.h
)

target_compile_definitions(Eqe
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Eqe
    PRIVATE
        eqe_assets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

set(EQE_PACKAGE_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/eqe.vst3")

if(APPLE)
    set(EQE_PLUGIN_INSTALL_DIR "/Library/Audio/Plug-Ins/ALL/eqe.vst3")
    set(EQE_PACKAGE_ZIP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/eqe-macos.zip")
elseif(WIN32)
    set(EQE_PACKAGE_ZIP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/eqe-windows.zip")
else()
    set(EQE_PACKAGE_ZIP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/eqe.zip")
endif()

if(TARGET Eqe_VST3)
    if(APPLE)
        add_custom_command(TARGET Eqe_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${EQE_PLUGIN_INSTALL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:Eqe_VST3>" "${EQE_PLUGIN_INSTALL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${EQE_PACKAGE_STAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:Eqe_VST3>" "${EQE_PACKAGE_STAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${EQE_PACKAGE_ZIP_PATH}"
            COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}" ${CMAKE_COMMAND} -E tar "cf" "${EQE_PACKAGE_ZIP_PATH}" --format=zip -- "eqe.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${EQE_PACKAGE_STAGE_DIR}"
            VERBATIM
        )
    else()
        add_custom_command(TARGET Eqe_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${EQE_PACKAGE_STAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:Eqe_VST3>" "${EQE_PACKAGE_STAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${EQE_PACKAGE_ZIP_PATH}"
            COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_BINARY_DIR}" ${CMAKE_COMMAND} -E tar "cf" "${EQE_PACKAGE_ZIP_PATH}" --format=zip -- "eqe.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${EQE_PACKAGE_STAGE_DIR}"
            VERBATIM
        )
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(EQE_IOS_DEVICE_UDID "" CACHE STRING "UDID of the device to install the built iOS app to")
    option(EQE_IOS_INSTALL_ON_DEVICE "Install the built iOS app on the device after building" OFF)

    if(EQE_IOS_INSTALL_ON_DEVICE AND NOT EQE_IOS_DEVICE_UDID)
        message(FATAL_ERROR "Set -DEQE_IOS_DEVICE_UDID=<udid> to install the app to device")
    endif()

    if(EQE_IOS_INSTALL_ON_DEVICE)
        add_custom_target(Eqe_InstallOnDevice
            COMMAND xcrun devicectl device install app --device ${EQE_IOS_DEVICE_UDID} "$<TARGET_BUNDLE_DIR:Eqe_Standalone>"
            DEPENDS Eqe_Standalone
            VERBATIM
        )
    endif()
endif()

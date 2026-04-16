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

if(APPLE)
    find_library(MEDIA_PLAYER_FRAMEWORK MediaPlayer REQUIRED)
endif()

juce_add_binary_data(eqe_assets
    SOURCES
        assets/fonts/SometypeMono-Bold.ttf
        assets/fonts/SometypeMono-Regular.ttf
        assets/fonts/SometypeMono-OFL.txt
)

set(EQE_APP_ARGS
    COMPANY_NAME "mixolve"
    BUNDLE_ID "com.mixolve.eqe"
    PRODUCT_NAME "eqe"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "This app needs microphone access to process audio."
)

juce_add_gui_app(Eqe ${EQE_APP_ARGS})

juce_generate_juce_header(Eqe)

set_source_files_properties(source/MainComponent.cpp PROPERTIES LANGUAGE OBJCXX)

target_sources(Eqe
    PRIVATE
        source/Main.cpp
        source/MainComponent.cpp
        source/MainComponent.h
)

target_compile_definitions(Eqe
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_AU=1
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Eqe
    PRIVATE
        eqe_assets
        juce::juce_audio_utils
        juce::juce_dsp
        ${MEDIA_PLAYER_FRAMEWORK}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(EQE_IOS_DEVICE_UDID "" CACHE STRING "UDID of the device to install the built iOS app to")
    option(EQE_IOS_INSTALL_ON_DEVICE "Install the built iOS app on the device after building" OFF)

    if(EQE_IOS_INSTALL_ON_DEVICE AND NOT EQE_IOS_DEVICE_UDID)
        message(FATAL_ERROR "Set -DEQE_IOS_DEVICE_UDID=<udid> to install the app to device")
    endif()

    if(EQE_IOS_INSTALL_ON_DEVICE)
        add_custom_target(Eqe_InstallOnDevice
            COMMAND xcrun devicectl device install app --device ${EQE_IOS_DEVICE_UDID} "$<TARGET_BUNDLE_DIR:Eqe>"
            DEPENDS Eqe
            VERBATIM
        )
    endif()

    target_compile_definitions(Eqe
        PUBLIC
            JUCE_IOS=1
    )
endif()

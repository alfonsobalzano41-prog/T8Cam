cmake_minimum_required(VERSION 3.22)

# Deve stare prima di project(): macOS 11 è il minimo per Apple Silicon.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(T8Cam VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: usa una copia locale se passi -DT8CAM_JUCE_DIR=/percorso/JUCE,
# altrimenti la scarica da GitHub alla versione fissata qui sotto.
set(T8CAM_JUCE_DIR "" CACHE PATH "Local JUCE checkout (optional)")
if(T8CAM_JUCE_DIR)
    add_subdirectory(${T8CAM_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# La webcam in JUCE esiste solo su macOS e Windows.
if(APPLE)
    set(T8CAM_FORMATS AU VST3 Standalone)
    set(T8CAM_HAS_CAMERA 1)
elseif(WIN32)
    set(T8CAM_FORMATS VST3 Standalone)
    set(T8CAM_HAS_CAMERA 1)
else()
    set(T8CAM_FORMATS VST3 Standalone)
    set(T8CAM_HAS_CAMERA 0)
endif()

juce_add_plugin(T8Cam
    COMPANY_NAME                "T8"
    BUNDLE_ID                   "com.t8.t8cam"
    PLUGIN_MANUFACTURER_CODE    T8Ab
    PLUGIN_CODE                 T8Cm
    PRODUCT_NAME                "T8Cam"
    DESCRIPTION                 "Webcam inside your DAW"
    FORMATS                     ${T8CAM_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE                kAudioUnitType_Effect
    VST3_CATEGORIES             Fx Tools
    # Questi valgono per l'app Standalone (che ha il suo Info.plist).
    CAMERA_PERMISSION_ENABLED   TRUE
    CAMERA_PERMISSION_TEXT      "T8Cam usa la webcam per mostrarti la facecam."
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "T8Cam usa l'ingresso audio per gli effetti che reagiscono al suono."
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(T8Cam PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/CameraView.cpp
    Source/CameraSource.cpp
    Source/CameraPermission.cpp
    Source/GifDecoder.cpp
    Source/VideoEffects.cpp
    Source/MediaModel.cpp
    Source/MediaPanel.cpp
    Source/Presets.cpp
    Source/UpdateChecker.cpp)

if(APPLE)
    target_sources(T8Cam PRIVATE Source/CameraPermission_mac.mm)
    # Il file usa retain/release manuali.
    set_source_files_properties(Source/CameraPermission_mac.mm PROPERTIES COMPILE_FLAGS "-fno-objc-arc")
    target_link_libraries(T8Cam PRIVATE "-framework AVFoundation")
endif()

target_compile_definitions(T8Cam PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_CAMERA=${T8CAM_HAS_CAMERA}
    T8CAM_HAS_CAMERA=${T8CAM_HAS_CAMERA})

target_link_libraries(T8Cam
    PRIVATE
        juce::juce_audio_utils
        juce::juce_video
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

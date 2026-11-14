cmake_minimum_required(VERSION 3.24)
project(BeckyAIAssistant LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Becky AI Assistant must be built with MSVC on Windows.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "This SDK build is Win32/x86 only. Configure with -A Win32.")
endif()

add_library(BeckyAIAssistant SHARED
  src/plugin/Plugin.cpp
  src/mail/MimeTextExtractor.cpp
  src/settings/ApiKeyStore.cpp
  src/gemini/GeminiConnection.cpp
  src/gemini/GeminiGenerate.cpp
  src/gemini/GeminiReply.cpp
  src/ui/SettingsDialog.cpp
  src/ui/LauncherDialog.cpp
  src/ui/ConsultDialog.cpp
  src/ui/ResultDialog.cpp
  resources/Launcher.rc
  third_party/becky_sdk/BeckyAPI.cpp
  BeckyAIAssistant.def
)

target_include_directories(BeckyAIAssistant PRIVATE
  src
  third_party/becky_sdk
)

target_compile_features(BeckyAIAssistant PRIVATE cxx_std_17)
set_property(TARGET BeckyAIAssistant PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_definitions(BeckyAIAssistant PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(BeckyAIAssistant PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:/W4>
  $<$<COMPILE_LANGUAGE:CXX>:/permissive->
  $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
  $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
)
target_link_libraries(BeckyAIAssistant PRIVATE user32 gdi32 shell32 crypt32 winhttp comctl32)
set_target_properties(BeckyAIAssistant PROPERTIES OUTPUT_NAME "Becky-AI-Assistant-14" PREFIX "")

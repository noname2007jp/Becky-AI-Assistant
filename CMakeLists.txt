cmake_minimum_required(VERSION 3.24)
project(BeckyAIAssistant VERSION 0.47.0 LANGUAGES CXX RC)

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
  src/gemini/GeminiModels.cpp
  src/gemini/GeminiGenerate.cpp
  src/gemini/GeminiReply.cpp
  src/gemini/GeminiCompose.cpp
  src/gemini/GeminiProofread.cpp
  src/gemini/GeminiProofreadJson.cpp
  src/gemini/GeminiDebugLog.cpp
  src/gemini/PiiMasker.cpp
  src/ui/SettingsDialog.cpp
  src/ui/PiiMaskPreviewDialog.cpp
  src/ui/LauncherDialog.cpp
  src/ui/ConsultDialog.cpp
  src/ui/ComposeDialog.cpp
  src/ui/ProofreadDialog.cpp
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
target_compile_definitions(BeckyAIAssistant PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX BECKY_AI_ASSISTANT_VERSION=\"${PROJECT_VERSION}\")
target_compile_options(BeckyAIAssistant PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:/W4>
  $<$<COMPILE_LANGUAGE:CXX>:/permissive->
  $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
  $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
)
target_link_libraries(BeckyAIAssistant PRIVATE user32 gdi32 shell32 crypt32 winhttp comctl32)
set_target_properties(BeckyAIAssistant PROPERTIES OUTPUT_NAME "Becky-AI-Assistant-${PROJECT_VERSION_MINOR}" PREFIX "")


include(CTest)
if(BUILD_TESTING)
  add_executable(MimeTextExtractorTests
    tests/MimeTextExtractorTests.cpp
    src/mail/MimeTextExtractor.cpp
  )
  target_include_directories(MimeTextExtractorTests PRIVATE src src/mail)
  target_compile_features(MimeTextExtractorTests PRIVATE cxx_std_17)
  target_compile_definitions(MimeTextExtractorTests PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MimeTextExtractorTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME MimeTextExtractorTests COMMAND MimeTextExtractorTests)

  add_executable(PiiMaskerTests
    tests/PiiMaskerTests.cpp
    src/gemini/PiiMasker.cpp
    src/settings/ApiKeyStore.cpp
  )
  target_include_directories(PiiMaskerTests PRIVATE src)
  target_compile_features(PiiMaskerTests PRIVATE cxx_std_17)
  target_compile_definitions(PiiMaskerTests PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(PiiMaskerTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_libraries(PiiMaskerTests PRIVATE shell32 crypt32)
  add_test(NAME PiiMaskerTests COMMAND PiiMaskerTests)

  add_executable(GeminiFallbackTests tests/GeminiFallbackTests.cpp)
  target_include_directories(GeminiFallbackTests PRIVATE src)
  target_compile_features(GeminiFallbackTests PRIVATE cxx_std_17)
  target_compile_definitions(GeminiFallbackTests PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(GeminiFallbackTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME GeminiFallbackTests COMMAND GeminiFallbackTests)

  add_executable(GeminiAsyncTests tests/GeminiAsyncTests.cpp)
  target_include_directories(GeminiAsyncTests PRIVATE src)
  target_compile_features(GeminiAsyncTests PRIVATE cxx_std_17)
  target_compile_definitions(GeminiAsyncTests PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(GeminiAsyncTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_libraries(GeminiAsyncTests PRIVATE user32)
  add_test(NAME GeminiAsyncTests COMMAND GeminiAsyncTests)

  add_executable(GeminiResponseJsonTests
    tests/GeminiProofreadJsonTests.cpp
    src/gemini/GeminiProofreadJson.cpp
  )
  target_include_directories(GeminiProofreadJsonTests PRIVATE src)
  target_compile_features(GeminiProofreadJsonTests PRIVATE cxx_std_17)
  target_compile_definitions(GeminiProofreadJsonTests PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(GeminiProofreadJsonTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME GeminiProofreadJsonTests COMMAND GeminiProofreadJsonTests)
endif()

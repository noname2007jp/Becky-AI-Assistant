cmake_minimum_required(VERSION 3.24)
project(BeckyAIAssistant VERSION 0.88.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Becky AI Assistant must be built with MSVC on Windows.")

  add_executable(WorkersAIReplyFormattingTests tests/WorkersAIReplyFormattingTests.cpp)
  target_include_directories(WorkersAIReplyFormattingTests PRIVATE src/providers)
  target_compile_features(WorkersAIReplyFormattingTests PRIVATE cxx_std_17)
  target_compile_options(WorkersAIReplyFormattingTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME WorkersAIReplyFormattingTests COMMAND WorkersAIReplyFormattingTests)
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "This SDK build is Win32/x86 only. Configure with -A Win32.")
endif()

add_library(BeckyAIAssistant SHARED
  src/plugin/Plugin.cpp
  src/mail/MimeTextExtractor.cpp
  src/settings/ApiKeyStore.cpp
  src/settings/AIProviderSettings.cpp
  src/settings/AIProviderSettingsCodec.cpp
  src/providers/GroqClient.cpp
  src/providers/WorkersAIClient.cpp
  src/providers/GroqMailSummary.cpp
  src/providers/WorkersAIMailSummary.cpp
  src/providers/WorkersAIMailReply.cpp
  src/providers/GroqMailReply.cpp
  src/providers/GroqMailConsultation.cpp
  src/providers/WorkersAIMailConsultation.cpp
  src/providers/ProviderModelCatalog.cpp
  src/providers/ModelCatalogPolicy.cpp
  src/providers/ModelCatalogTimestamps.cpp
  src/providers/ProviderModelCatalogStore.cpp
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

  add_executable(MailPromptContentTests tests/MailPromptContentTests.cpp)
  target_include_directories(MailPromptContentTests PRIVATE src)
  target_compile_features(MailPromptContentTests PRIVATE cxx_std_17)
  target_compile_options(MailPromptContentTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME MailPromptContentTests COMMAND MailPromptContentTests)

  add_executable(GeminiReplyPromptContentTests tests/GeminiReplyPromptContentTests.cpp)
  target_include_directories(GeminiReplyPromptContentTests PRIVATE src)
  target_compile_features(GeminiReplyPromptContentTests PRIVATE cxx_std_17)
  target_compile_options(GeminiReplyPromptContentTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME GeminiReplyPromptContentTests COMMAND GeminiReplyPromptContentTests)

  add_executable(MailSignatureStripperTests tests/MailSignatureStripperTests.cpp)
  target_include_directories(MailSignatureStripperTests PRIVATE src)
  target_compile_features(MailSignatureStripperTests PRIVATE cxx_std_17)
  target_compile_options(MailSignatureStripperTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME MailSignatureStripperTests COMMAND MailSignatureStripperTests)

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
  target_include_directories(GeminiResponseJsonTests PRIVATE src)
  target_compile_features(GeminiResponseJsonTests PRIVATE cxx_std_17)
  target_compile_definitions(GeminiResponseJsonTests PRIVATE _MBCS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(GeminiResponseJsonTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME GeminiResponseJsonTests COMMAND GeminiResponseJsonTests)

  add_executable(GeminiTextContinuationTests tests/GeminiTextContinuationTests.cpp)
  target_include_directories(GeminiTextContinuationTests PRIVATE src)
  target_compile_features(GeminiTextContinuationTests PRIVATE cxx_std_17)
  target_compile_options(GeminiTextContinuationTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME GeminiTextContinuationTests COMMAND GeminiTextContinuationTests)

  add_executable(MarkdownRendererTests tests/MarkdownRendererTests.cpp)
  target_include_directories(MarkdownRendererTests PRIVATE src)
  target_compile_features(MarkdownRendererTests PRIVATE cxx_std_17)
  target_compile_options(MarkdownRendererTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME MarkdownRendererTests COMMAND MarkdownRendererTests)

  add_executable(ConsultHistoryStoreTests tests/ConsultHistoryStoreTests.cpp)
  target_include_directories(ConsultHistoryStoreTests PRIVATE src)
  target_compile_features(ConsultHistoryStoreTests PRIVATE cxx_std_17)
  target_compile_options(ConsultHistoryStoreTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME ConsultHistoryStoreTests COMMAND ConsultHistoryStoreTests)

  add_executable(WorkersAIModelIdTests tests/WorkersAIModelIdTests.cpp)
  target_include_directories(WorkersAIModelIdTests PRIVATE src)
  target_compile_features(WorkersAIModelIdTests PRIVATE cxx_std_17)
  target_compile_options(WorkersAIModelIdTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME WorkersAIModelIdTests COMMAND WorkersAIModelIdTests)

  add_executable(ModelDescriptionsTests tests/ModelDescriptionsTests.cpp)
  target_include_directories(ModelDescriptionsTests PRIVATE src)
  target_compile_features(ModelDescriptionsTests PRIVATE cxx_std_17)
  target_compile_options(ModelDescriptionsTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME ModelDescriptionsTests COMMAND ModelDescriptionsTests)

  add_executable(ModelCatalogPolicyTests
    tests/ModelCatalogPolicyTests.cpp
    src/providers/ModelCatalogPolicy.cpp
  )
  target_include_directories(ModelCatalogPolicyTests PRIVATE src)
  target_compile_features(ModelCatalogPolicyTests PRIVATE cxx_std_17)
  target_compile_definitions(ModelCatalogPolicyTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(ModelCatalogPolicyTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_libraries(ModelCatalogPolicyTests PRIVATE shell32 winhttp)
  add_test(NAME ModelCatalogPolicyTests COMMAND ModelCatalogPolicyTests)

  add_executable(ProviderModelCatalogStoreTests
    tests/ProviderModelCatalogStoreTests.cpp
    src/providers/ProviderModelCatalogStore.cpp
  )
  target_include_directories(ProviderModelCatalogStoreTests PRIVATE src)
  target_compile_features(ProviderModelCatalogStoreTests PRIVATE cxx_std_17)
  target_compile_definitions(ProviderModelCatalogStoreTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(ProviderModelCatalogStoreTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_libraries(ProviderModelCatalogStoreTests PRIVATE shell32)
  add_test(NAME ProviderModelCatalogStoreTests COMMAND ProviderModelCatalogStoreTests)

  add_executable(AIProviderSettingsCodecTests
    tests/AIProviderSettingsCodecTests.cpp
    src/settings/AIProviderSettingsCodec.cpp
  )
  target_include_directories(AIProviderSettingsCodecTests PRIVATE src)
  target_compile_features(AIProviderSettingsCodecTests PRIVATE cxx_std_17)
  target_compile_options(AIProviderSettingsCodecTests PRIVATE /W4 /permissive- /EHsc /utf-8)
  add_test(NAME AIProviderSettingsCodecTests COMMAND AIProviderSettingsCodecTests)
endif()

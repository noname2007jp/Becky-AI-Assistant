# Becky AI Assistant

Becky! Internet Mail向けのAIアシスタントプラグインです。

## 開発状況

現在は STEP 1（Becky!プラグインの骨組みとランチャー画面）を実装中です。

- Becky! SDK: `B2PinSDK-26400` に含まれる `BeckyApi.h` / `BeckyAPI.cpp` を使用
- 対象: Win32 (x86)、Visual Studio 2022、C++17、MBCS
- 右クリックメニュー: メール一覧と本文表示
- 初期版の対象メール: 1通。複数選択時は警告して処理を中止

## ビルド

Visual Studio 2022 の「x86 Native Tools Command Prompt」から実行します。

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

生成物は `build\\Release\\BeckyAIAssistant.dll` です。

## 注意

現段階のランチャーは画面とメニュー連携の動作確認用です。要約・返信・Gemini通信はまだ実装されていません。
APIキーやメール本文などの機密情報をリポジトリへ登録しないでください。

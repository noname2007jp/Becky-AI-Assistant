# Becky AI Assistant

Becky! Internet Mail向けのAIアシスタントプラグインです。

## 開発状況

- Becky! SDK: `B2PinSDK-26400`
- 対象: Win32 (x86)、Visual Studio 2022、C++17、MBCS
- 右クリックメニュー: メール一覧と本文表示
- 対象メール: 1通。複数選択時は警告して処理を中止
- メール本文: MIMEテキスト抽出、添付ファイルは除外
- バージョン05: 明示的な同意後にメール本文をGeminiへ送信し、要約結果を表示する機能を追加
- 要約送信前に送信対象（件名・差出人・本文）を確認。キャンセル時は通信しません。
- 要約生成はGemini `generateContent` を使用。返信文案・AI相談は未実装
- 接続テストはモデル一覧APIへのGETのみ。メール本文やメール情報は送信しません
- Gemini APIへの通信・メール本文の送信はまだ実装していません

## APIキーの保存

設定画面で入力したGemini APIキーは、Windows DPAPI（現在のWindowsユーザーに紐づく暗号化）で暗号化し、次の場所へ保存します。

`%APPDATA%\Becky-AI-Assistant\gemini-key.dat`

平文のAPIキーを設定ファイルやログへ保存しません。別のWindowsユーザーでは復号できません。

## ビルド

Visual Studio 2022 の「x86 Native Tools Command Prompt」から実行します。

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

生成物: `build\Release\Becky-AI-Assistant-03.dll`

## 注意

メール本文は、ユーザーが明示的に実行を選び、送信内容を確認するまで外部サービスへ送信しません。
APIキーやメール本文などの機密情報をリポジトリへ登録しないでください。

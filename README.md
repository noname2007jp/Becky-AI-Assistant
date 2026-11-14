# Becky AI Assistant

Becky! Internet Mail向けのAIアシスタントプラグインです。

## 開発状況

- Becky! SDK: `B2PinSDK-26400`
- 対象: Win32 (x86)、Visual Studio 2022、C++17、MBCS
- 右クリックメニュー: メール一覧と本文表示
- 対象メール: 1通。複数選択時は警告して処理を中止
- メール本文: MIMEテキスト抽出、添付ファイルは除外
- バージョン12: 返信文案のトーン（標準・やや柔らかめ・簡潔）を選択可能\n- バージョン11: 返信文案を編集し、Becky!の通常返信画面へ挿入可能\n- バージョン10: 受信メールから丁寧な返信文案を生成し、結果ウィンドウで確認・コピー可能
- バージョン09: 要約結果をMarkdown簡易整形して表示し、部分選択コピー・全文コピーに対応
- バージョン06から継続: 要約時にGeminiモデルとThinkingレベルを選択可能
- 標準選択: Gemini 3.5 Flash-Lite (Thinking Medium)
- モデル選択肢: Gemini 3.8 / 3.6 / 3.5 Flash-Lite、Gemini 2.5、Gemini 3.1 Pro Preview、Gemma 4
- 要約送信前に送信対象（件名・差出人・本文）を確認。キャンセル時は通信しません。
- Gemini APIへの通信・メール本文の送信は、明示的な同意後にのみ実行
- 返信文案は標準的な丁寧語で生成。生成前に送信内容を確認し、明示的な同意後のみGeminiへ送信
- AI相談機能は今後対応

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

生成物: `build\Release\Becky-AI-Assistant-12.dll`

## 注意

メール本文は、ユーザーが明示的に実行を選び、送信内容を確認するまで外部サービスへ送信しません。

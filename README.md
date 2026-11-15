# Becky AI Assistant

Becky! Internet Mail向けのGemini API連携AIアシスタントプラグインです。

## 現在の機能（0.27.0）

- 対象: Win32 (x86)、Visual Studio 2022、C++17、MBCS
- メール一覧・本文表示の右クリックメニューから起動
- 対象メールは1通。複数選択時は警告して処理を中止
- MIMEテキストを抽出して要約、複数回のAI相談、返信文案の作成が可能
- 相談履歴を踏まえた返信文案の作成と、Becky!の通常返信・全員返信画面への挿入に対応
- 新規メールの目的・補足条件から件名と本文を作成し、編集後にBecky!の作成画面へ反映
- 生成文案は編集可能。送信・送信操作は行わない
- Geminiモデル・Thinkingレベルの選択、返信文体の調整に対応
- 相談履歴は画面を閉じると消去し、ディスクには保存しない
- 添付ファイルはGemini APIへの送信対象外

## 個人情報のマスキング

Gemini APIへ送信するプロンプトを作成する直前に、ローカルでマスキングします。

- 自動検出: メールアドレス、電話番号、郵便番号、IPv4アドレス、法人格付き会社名、氏名表記の一部
- 追加登録: 設定画面で会社名・氏名・部署名・住所などを1行に1件ずつ登録可能
- 追加登録した語句はWindows DPAPIで暗号化して保存
- Geminiから返った文案は、表示前にローカルで元の語句へ戻す
- マスキング設定を復号できない場合は、外部送信を中止

自動検出は完全ではありません。氏名・会社名・住所など、確実に伏せたい語句は設定画面に登録してください。マスキング後の情報でも文脈から個人や組織が推測される可能性があるため、送信前の確認が必要です。

## API送信の確認

メールの要約・相談・返信文案を実行する際は、Gemini APIへ送信することを確認するダイアログを表示します。初期選択は「いいえ」です。ユーザーが明示的に許可しない限り通信しません。

## APIキー・マスキング設定の保存

Gemini APIキーと追加マスキング語句は、Windows DPAPI（現在のWindowsユーザーに紐づく暗号化）で暗号化して保存します。

- APIキー: `%APPDATA%\\Becky-AI-Assistant\\gemini-key.dat`
- マスキング語句: `%APPDATA%\\Becky-AI-Assistant\\pii-mask.dat`

別のWindowsユーザーでは復号できません。平文のAPIキー・追加マスキング語句は設定ファイルへ保存しません。

## ビルド

Visual Studio 2022 の「x86 Native Tools Command Prompt」から実行します。

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

生成物: `build\\Release\\Becky-AI-Assistant-27.dll`

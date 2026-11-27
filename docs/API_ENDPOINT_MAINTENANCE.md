# APIエンドポイントの保守方針

## 目的

Gemini／Groq／Cloudflare Workers AIのAPI URL、ホスト名、APIバージョン、パスは将来変更される可能性があります。特にAPIのバージョン廃止、旧パスの終了、認証方式・リクエスト形式・応答JSONの変更に備え、変更箇所と確認手順を明確にします。

## 現行エンドポイント一覧（2026-11-27時点）

| 提供元 | 用途 | ホスト | パス／形式 |
|---|---|---|---|
| Google Gemini | 生成 | `generativelanguage.googleapis.com` | `/v1beta/models/{model}:generateContent` |
| Google Gemini | モデル一覧・接続テスト | `generativelanguage.googleapis.com` | `/v1beta/models` |
| Google AI for Developers | 料金情報取得 | `ai.google.dev` | `/gemini-api/docs/pricing` |
| Groq | 生成 | `api.groq.com` | `/openai/v1/chat/completions` |
| Groq | モデル一覧 | `api.groq.com` | `/openai/v1/models` |
| Cloudflare Workers AI | 生成 | `api.cloudflare.com` | `/client/v4/accounts/{account_id}/ai/v1/chat/completions` |
| Cloudflare Workers AI | モデル一覧 | `api.cloudflare.com` | `/client/v4/accounts/{account_id}/ai/models/search` |

※Cloudflare Workers AIの生成APIはOpenAI互換形式を使用しています。モデル一覧APIと生成APIは別のパスです。

## 変更時の原則

1. **接続先をAPI応答から自動変更しない。** APIキー／トークンを任意ホストへ送ることを防ぐため、接続先ホストはアプリケーション側で管理します。
2. **ユーザーが任意URLを入力する設定は設けない。** 認証情報の意図しない外部送信、HTTPS証明書検証の回避、偽APIへの接続を防止します。企業向けプロキシはWindows／WinHTTPの標準プロキシ設定を使用します。
3. **ホスト・パス・APIバージョンを変更したら、生成APIとモデル一覧APIの両方を確認する。** 片方だけ移行して一覧更新や生成が使えなくなる状態を避けます。
4. **HTTPステータスだけでなく応答JSONの構造も検証する。** HTTP 200でも本文の格納場所やfinish reason等が変われば、空の回答・誤った回答として扱われる可能性があります。
5. **旧APIから新APIへの切り替えは、公式の移行期限・廃止日を確認してから行う。** 旧APIが利用できる間は、必要に応じて互換性を維持し、切り替え後に旧経路を削除します。
6. **APIキー、API Token、メール本文、認証ヘッダーをログに記録しない。** 障害調査ではプロバイダー、操作、モデルID、HTTPステータス、処理時間、リクエストID（公開して問題ない場合）を使用します。

## API変更を検知した場合の保守手順

1. 公式ドキュメント／Changelogで変更内容、適用日、旧APIの廃止日、認証・権限の変更を確認する。
2. 変更対象を「ホスト」「パス／APIバージョン」「認証」「リクエストJSON」「応答JSON」「モデル一覧・料金ページ解析」に分けて記録する。
3. エンドポイント定義と通信処理を修正する。複数箇所に同じURLが重複していないか検索する。
4. テスト用アカウントで、モデル一覧取得・接続テスト・要約・返信文案・相談を確認する。モデル一覧が取得できることだけで生成APIの正常性を判断しない。
5. 400／401／403／404／429／5xx、タイムアウト、DNS失敗、空応答、不正JSONを確認する。認証エラーや権限エラーをAPI URL変更と誤認しない。
6. Win32 ReleaseビルドとCTestを実行し、実機で最低1モデルずつ各プロバイダーの生成を確認する。
7. README、エンドポイント一覧、リリースチェックリスト、バージョン、変更履歴を更新してリリースする。

## 変更箇所を探すときの検索語

リポジトリ内で次のホスト名・パスを検索します。

- `generativelanguage.googleapis.com`
- `ai.google.dev`
- `api.groq.com`
- `api.cloudflare.com`
- `/v1beta/`
- `/openai/v1/`
- `/client/v4/`

URL変更時は、生成処理だけでなく、モデル一覧取得・接続テスト・料金情報取得・READMEの記述も確認してください。

## 更新記録

| 日付 | 内容 |
|---|---|
| 2026-11-27 | 初版。現行のGemini／Groq／Workers AIエンドポイントと変更時の確認手順を記録 |

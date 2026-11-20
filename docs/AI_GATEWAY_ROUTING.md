# Cloudflare AI Gateway接続設計・実装メモ

## 対応範囲（v0.84.0）

- Groq：Cloudflare AI GatewayのGroqネイティブエンドポイントを使用。
- Workers AI：Cloudflare AI REST APIのChat Completionsエンドポイントに `cf-aig-gateway-id` を付けて利用。
- Gemini：この版では直接接続のみ。Gateway経由は後続段階で実装する。
- 既存のプロバイダー選択、モデルID、メールプロンプト、マスキング、プレビュー、キャンセル、相談履歴は変更しない。

## Groq

Gateway URL：

`https://gateway.ai.cloudflare.com/v1/{account_id}/{gateway_id}/groq/chat/completions`

- `Authorization: Bearer {Groq API key}` を維持する。
- Gateway認証が有効な場合は `cf-aig-authorization: Bearer {Cloudflare API token}` を追加する。
- モデルIDはGroqの既存IDをそのまま使用する。
- 既存のOTPM待機・再試行・出力上限時の継続処理を維持する。

## Workers AI

Gateway URL：

`https://api.cloudflare.com/client/v4/accounts/{account_id}/ai/v1/chat/completions`

- `Authorization: Bearer {Cloudflare API token}` を使用する。
- `cf-aig-gateway-id: {gateway_id}` を必ず付ける。
- モデルIDは既存の `@cf/...` 形式を維持する。
- Gateway接続時のCloudflare API TokenにはWorkers AI Read権限が必要。
- 直接接続時は従来どおりWorkers AI API Tokenを使用する。

## ログとキャッシュ

- `gatewayRequestLogging=false` の場合は `cf-aig-collect-log: false` を送信する。
- ログを有効にした場合でも、`cf-aig-collect-log-payload: false` を常に送信し、メール本文・プロンプト・AI回答のペイロード保存を要求しない。
- `gatewayCacheEnabled=false` の場合は `cf-aig-skip-cache: true` を送信する。
- キャッシュを有効にする場合、Gateway側でもキャッシュを有効にする必要がある。Gateway側でキャッシュを有効にした場合、メール内容がキャッシュに保持される可能性があるため、明示的なユーザー選択を必須とする。
- Logpush等の別経路でのログエクスポートは、リクエスト単位のログヘッダーとは別設定である点に注意する。

## 資格情報

- 既存のGemini／Groq／Workers AI資格情報は直接接続用として維持する。
- Cloudflare Account ID、Gateway ID、Cloudflare API Tokenは既存のDPAPI保護された設定ストアに保存する。
- Groqでは既存Groq APIキーを上流認証に使用し、Cloudflare API TokenはAuthenticated Gatewayを使用する場合に追加する。
- Workers AIではGateway経由の認証にCloudflare API Tokenを使用する。直接接続用Workers AI API Tokenは変更しない。
- 認証ヘッダー・APIキー・本文・プロンプト・生成結果をプロバイダーログへ記録しない。

## 受け入れ確認

1. Groq直接接続で要約・返信・相談が従来どおり動作する。
2. Groq Gateway接続で同じ操作が動作する。
3. Workers AI直接接続で要約・返信・相談が従来どおり動作する。
4. Workers AI Gateway接続で同じ操作が動作する。
5. GatewayログOFF時はリクエストログが除外される。ログON時もペイロード保存ヘッダーはfalse。
6. キャッシュOFF時はskip-cacheヘッダーが送られる。
7. Gateway認証・API権限・Gateway IDの誤りが原因別に表示される。
8. GeminiでGatewayを選択した場合、直接接続へ黙ってフォールバックせず、未対応であることを表示する。
9. Win32 Release、Windows x86、CTestを通過する。

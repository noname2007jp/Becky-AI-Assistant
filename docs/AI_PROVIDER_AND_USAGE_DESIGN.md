# 複数AIプロバイダー・利用状況管理 設計方針

## 目的

Gemini専用の現行実装を維持しながら、学習利用・データ保持の条件を確認できるAIサービスを追加する。あわせて、無料枠の利用状況と概算残り処理件数をBecky!内で確認できるようにする。

## 対象サービスと接続方式

| 表示名 | 接続方式 | 認証情報 | 備考 |
|---|---|---|---|
| Google Gemini | Gemini Developer API | APIキー | 無料枠は入力データがGoogle製品改善に利用されるため、業務メール用途の推奨対象外。有料枠は製品改善に利用されない |
| Groq | OpenAI互換Chat Completions API | Groq APIキー | 推論データは既定で保持しない。Data ControlsでZDRを有効化できる |
| Cloudflare Workers AI | Workers AI Chat Completions API | Account ID、API Token | 標準推論のCustomer Contentはモデル学習に利用されず、処理後に保持されない |
| Groq via Cloudflare AI Gateway | AI Gateway経由のGroq | Cloudflare Account ID、Gateway ID、Groq APIキー（BYOK） | Gatewayキャッシュを任意で利用可能 |
| Workers AI via AI Gateway | AI Gateway REST API | Cloudflare Account ID、API Token、Gateway ID | Workers AIモデルをGateway経由で利用 |

AI GatewayはAIモデルではなく接続経路として扱う。AIサービス（Gemini / Groq / Workers AI）と接続経路（直接 / AI Gateway）を分離する。

## 学習利用・データ保持の必須条件

- 学習利用なしを業務利用の必須条件とする。
- 学習利用の有無とデータ保持の有無は別項目として表示する。
- Gemini無料枠は「学習・製品改善に利用される」と明示し、業務メール用途の推奨対象から除外する。
- Gemini有料枠は「製品改善に利用されない」と表示する。ただし、不正利用・法令対応等の例外的な保持条件は公式規約を参照する。
- Groqは既定で推論の入力・出力を保持しない。ZDRを有効化した場合は、信頼性・不正利用監視目的の一時保持も無効化される。ZDR設定の状態はユーザーがGroq Consoleで確認する。
- Workers AIは標準推論で入力・出力を保持せず、モデル学習にも利用しない。状態を持つ機能やAI Gatewayのログ・キャッシュを有効にした場合は別途扱う。
- AI Gatewayを使う場合、ログ収集は無効化し、リクエストごとにログ本文保存を無効化する。キャッシュは入力・出力をTTL中保持するため、初期値OFFとし、明示的な有効化と注意表示を必須にする。
- マスキング設定は現行仕様を維持する。AIサービスを切り替えても、マスキング確認を省略しない。

## AI Gatewayキャッシュの扱い

- キャッシュは完全一致したリクエストだけを対象とし、意味の近いメールを再利用する機能ではない。
- キャッシュHIT時はプロバイダーへの再リクエストを避けられるため、プロバイダー側のリクエスト・トークン消費を節約できる。
- メール本文・相談内容がキャッシュに保存されるため、初期設定はOFF。
- 有効化はユーザーが明示的に選択する。キャッシュTTLを指定できるようにする。
- 相談履歴を含む継続相談、返信文案など個別性が高い操作は既定でキャッシュ対象外とする。
- キャッシュHIT/MISSは応答ヘッダー `cf-aig-cache-status` から取得し、ローカル利用履歴へ記録する。
- 同時刻の同一リクエストはキャッシュ登録前に重複実行される場合があるため、必ずしも重複分がすべて節約されるとは限らない。

## 利用状況タブ

設定画面に「AI利用状況」タブを追加する。

表示項目:
- プロバイダー / モデル / 操作（要約・相談・返信・新規作成・校正）
- 本日・直近7日間のリクエスト数
- 入力・出力トークン数
- Workers AIのNeurons消費量と無料枠に対する概算残量
- Groqの応答ヘッダーから取得できた残りリクエスト数・トークン数・リセット情報
- AI GatewayのキャッシュHIT / MISS数、HIT率、節約できたプロバイダー呼び出し数
- 直近の利用量から推定した残り処理件数（推定値であることを明記）

### 数値の精度

- Groq: 応答ヘッダーで取得した値を「直近応答時点のレート制限残量」として表示する。アカウント・モデル・時間窓に依存するため、永久的な残量とは扱わない。
- Workers AI: API応答のusage情報とモデル別Neurons単価から日次消費を算出し、10,000 Neurons/日の無料割当から概算する。アカウント全体の他アプリ利用分はローカルから把握できないため「このプラグインの推定値」と明記する。
- Gemini: usageMetadataが取得できる場合にトークン実績を記録する。無料枠・有料枠の実際の残量はAPIから確実に取得できないため、残り件数を断定しない。
- AI Gateway: キャッシュHIT/MISSの応答実績を集計する。HIT数を「節約できた可能性のある上流API呼び出し数」として表示する。

## ローカル利用履歴とプライバシー

- 保存先は `%APPDATA%\\Becky-AI-Assistant\\usage-stats.dat`。
- 保存するのは日時、プロバイダー、モデル、操作名、入力・出力トークン数、Neurons、HTTP状態、キャッシュ状態、レート制限ヘッダーなどのメタデータのみ。
- メール本文、プロンプト、AI応答、APIキーは利用履歴へ保存しない。
- APIキー・Cloudflare Token等の秘密情報はWindows DPAPIで暗号化して保存する。
- 利用履歴の削除操作を用意する。

## 実装フェーズ

1. プロバイダー設定・秘密情報保存を追加し、Geminiの既存設定との後方互換性を維持する。
2. 共通AIリクエスト/レスポンス層を追加し、既存の要約・相談・返信・新規作成・校正を段階的に共通化する。
3. Groq直接接続を追加し、モデル一覧取得・接続テスト・レート制限ヘッダー取得を実装する。
4. Workers AI直接接続を追加し、Account ID / API Token / モデル一覧・Neurons usageを実装する。
5. Cloudflare AI Gateway経由のGroq / Workers AI接続を追加する。ログ本文保存は無効、キャッシュは初期OFF。
6. ローカル利用履歴、概算残り件数、キャッシュ実績を表示する「AI利用状況」タブを追加する。
7. Win32 CIテストとBecky!実機受け入れ試験を実施する。

## 公式仕様の参照先

- [Gemini API pricing and data use](https://ai.google.dev/gemini-api/docs/pricing)
- [Groq Your Data](https://console.groq.com/docs/your-data)
- [Groq Rate Limits](https://console.groq.com/docs/rate-limits)
- [Cloudflare Workers AI Data Usage](https://developers.cloudflare.com/workers-ai/platform/data-usage/)
- [Cloudflare Workers AI Pricing](https://developers.cloudflare.com/workers-ai/platform/pricing/)
- [Cloudflare AI Gateway Caching](https://developers.cloudflare.com/ai-gateway/features/caching/)
- [Cloudflare AI Gateway Logging](https://developers.cloudflare.com/ai-gateway/observability/logging/)
- [Cloudflare AI Gateway REST API](https://developers.cloudflare.com/ai-gateway/usage/rest-api/)

仕様・料金・無料枠は変更されるため、画面やREADMEには確認日を付け、固定値として保証しない。

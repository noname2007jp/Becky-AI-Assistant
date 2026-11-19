#pragma once
#include <string>
#include <algorithm>
#include <cwctype>

// Curated, human-readable model guidance. Unknown/new IDs are deliberately
// described as unverified rather than assigned guessed capabilities.
inline std::wstring GetModelDescription(int provider, const std::wstring& modelId)
{
    std::wstring id = modelId;
    std::transform(id.begin(), id.end(), id.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

    if (provider == 0) { // Gemini
        if (id.find(L"flash-lite") != std::wstring::npos)
            return L"軽量・低コスト重視。短いメールの要約、定型的な返信案、簡単な校正向け。複雑な推論や長文分析では上位モデルを推奨します。\r\nコンテキスト長・出力上限・料金区分はGoogle公式モデル情報を確認してください。";
        if (id.find(L"flash") != std::wstring::npos)
            return L"速度と品質のバランス型。メール要約、返信案、校正、日常的な相談に適しています。\r\nコンテキスト長・出力上限・料金区分はGoogle公式モデル情報を確認してください。";
        if (id.find(L"pro") != std::wstring::npos)
            return L"高性能・推論重視。複雑なメール内容の分析、長文の相談、慎重な文章作成に適しています。応答速度や利用料金はFlash系より大きくなる場合があります。\r\n詳細な上限・料金はGoogle公式モデル情報を確認してください。";
        if (id.find(L"gemma") != std::wstring::npos)
            return L"オープンモデル系。軽量な文章生成・要約用途の候補です。Geminiシリーズと機能・対応能力が異なるため、実際の用途で確認してください。\r\nコンテキスト長・出力上限・料金はGoogle公式情報を確認してください。";
        return L"モデルの詳細情報は未登録です。モデルIDから性能・無料枠・利用制限を推測せず、Google公式モデル一覧と料金情報を確認してください。";
    }

    if (provider == 1) { // Groq
        if (id == L"openai/gpt-oss-120b")
            return L"高性能な汎用・推論モデル。複雑なメール相談、長文の要約、論点整理、返信案の作成に向いています。Tool Use／JSON対応。\r\nコンテキスト: 131,072 tokens／最大出力: 65,536 tokens／速度目安: 約500 tokens/sec。";
        if (id == L"openai/gpt-oss-20b")
            return L"応答速度を重視した汎用モデル。日常的なメール要約、返信案、校正に向いています。推論・Tool Use／JSON対応。\r\nコンテキスト: 131,072 tokens／最大出力: 65,536 tokens／速度目安: 約1,000 tokens/sec。";
        if (id == L"qwen/qwen3.8-27b")
            return L"推論と指示追従に強いマルチモーダルモデル。複雑な相談や多言語の文章処理に向いています。Thinking／Instruct、Tool Use／JSON対応。\r\nコンテキスト: 131,072 tokens／最大出力: 16,384 tokens／速度目安: 約450 tokens/sec。";
        if (id == L"meta-models/muse-spark-open")
            return L"推論を重視したPreviewモデル。複雑な問題解決向けですが、Previewのため仕様変更・提供終了の可能性があります。\r\nコンテキスト: 131,072 tokens／最大出力: 131,072 tokens／速度目安: 約350 tokens/sec。";
        if (id == L"minimaxai/minimax-m2.7")
            return L"大規模な推論・コーディング向けモデル。複雑な分析や長文の相談に向いています。Enterprise区分のため、Free枠で利用できるとは限りません。\r\nコンテキスト: 196,608 tokens／最大出力: 131,072 tokens／速度目安: 約260 tokens/sec。";
        if (id == L"llama-3.3-70b-versatile")
            return L"Meta Llama 3.3 70B。汎用的な文章生成・要約・指示追従向け。Groqの現行一覧ではEnterprise扱いのため、利用条件を確認してください。\r\nコンテキスト: 131,072 tokens／最大出力: 32,768 tokens／速度目安: 約280 tokens/sec。";
        if (id == L"llama-3.1-8b-instant")
            return L"軽量・高速な文章生成モデル。短い要約や定型処理向け。ただしGroqでは提供終了済みのモデルIDです。新規選択には使用しないでください。";
        if (id == L"groq/compound" || id == L"groq/compound-mini")
            return L"検索・コード実行などのツールを組み合わせるエージェント型システムでしたが、2026年9月21日に提供終了しています。";
        return L"Groqのモデル一覧から取得したモデルです。詳細仕様は未登録のため、性能・料金・Free枠・レート制限はGroq公式モデルページで確認してください。モデル一覧への掲載はFree利用を保証しません。";
    }

    if (provider == 2) { // Cloudflare Workers AI
        if (id.find(L"llama") != std::wstring::npos)
            return L"Meta Llama系のテキスト生成モデルです。一般的な要約・文章作成・質問応答の候補です。モデルサイズにより速度・品質・必要Neuronsが異なります。\r\nこのモデルID固有のコンテキスト長・出力上限・Neurons消費量は未確認です。";
        if (id.find(L"qwen") != std::wstring::npos)
            return L"Qwen系モデルです。多言語処理や指示追従を重視する用途の候補です。モデルの世代・サイズにより推論能力と速度が異なります。\r\nこのモデルID固有のコンテキスト長・出力上限・Neurons消費量は未確認です。";
        if (id.find(L"deepseek") != std::wstring::npos)
            return L"DeepSeek系モデルです。推論・論理的な分析を重視する用途の候補です。蒸留版など派生モデルでは性能・出力形式が異なります。\r\nこのモデルID固有のコンテキスト長・出力上限・Neurons消費量は未確認です。";
        if (id.find(L"gemma") != std::wstring::npos)
            return L"Google Gemma系のオープンモデルです。軽量な文章生成・要約用途の候補です。モデルサイズによって応答速度と品質が異なります。\r\nこのモデルID固有のコンテキスト長・出力上限・Neurons消費量は未確認です。";
        return L"Cloudflare Workers AIのテキスト生成モデルです。モデル固有の説明・コンテキスト長・出力上限・Neurons消費量は未確認です。Cloudflare公式モデルカードで確認してください。Free枠の対象可否はモデルIDだけでは判定できません。";
    }
    return L"モデルIDを選択すると、登録済みの特徴を表示します。";
}

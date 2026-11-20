# Unified AI Model Catalog Design

## Goal

Manage Gemini, Groq, and Cloudflare Workers AI models without requiring a DLL rebuild when a model is added, retired, or its recommendation changes.

The catalog has two independent sources:
1. Provider API catalog: authoritative runtime availability and capabilities returned by each provider's API.
2. Maintained policy catalog: product guidance maintained as JSON in this repository (recommendation, exclusions, supported plugin operations, and quality notes).

The provider API remains authoritative for whether an account can call a model. The policy JSON must never override an API error or claim that a model is available when the provider does not expose it.

## Runtime flow

1. Load last-known-good provider model catalogs from the user's AppData directory.
2. Fetch each provider's model list using its official API and credentials already entered by the user.
3. Normalize provider records into a common internal model record.
4. Filter out models that cannot perform text generation / chat completion used by this plugin.
5. Fetch the policy JSON from this repository's raw-content URL with a bounded timeout and size limit.
6. Merge policy metadata by exact provider + model ID. Unknown IDs remain visible as 未評価 if they pass API capability filtering.
7. Save provider API catalogs and last-known-good policy JSON atomically. Display each catalog's local file last-write time as the last successful fetch time, and display the cached policy file's last-write time separately. A failed refresh must not erase working cached data.
8. A model's first actual generation request is the final account-level availability test. Record sanitized results only; never persist prompts, email content, generated text, or credentials.

## Common normalized record

- provider: gemini, groq, or workers_ai
- model_id: exact callable ID sent to the provider
- display_name: provider-supplied display name, otherwise model ID
- text_generation: whether provider API says the model supports the required generation task
- chat_completions: whether the endpoint used by this plugin is supported
- deprecated / experimental: provider metadata when available
- availability: listed, verified, temporarily_unavailable, or unsupported
- last_checked_utc: timestamp of last API refresh or successful test

Do not infer capabilities from model names or family names. If provider metadata does not establish Chat Completions support, mark it unknown and use a conservative capability probe or exclude it until verified.

## Policy JSON

The remote policy file is config/model-catalog-policy.json. Its schema is versioned independently of the DLL.

Each model entry is keyed by exact provider and exact model ID and may contain:
- status: recommended, allowed, caution, or blocked
- operations: plugin operations known to work (summary, reply, consultation, proofreading, compose)
- quality: unverified, tested, or poor
- language_notes: concise Japanese notes, with evidence/date in tested_on
- reason: why a model is cautioned or blocked
- replacement_model_id: optional suggested replacement

A recommendation is advisory. It must not silently change the user's selected model. Blocked models should be hidden from normal selection by default, but an existing saved selection should be preserved and shown with a warning.

## Provider-specific API adapters

- Gemini: models.list; require generateContent in supportedGenerationMethods. Keep official pricing/free-tier parsing separate from capability and quality.
- Groq: GET /openai/v1/models; retain provider-returned IDs and metadata. The model list is not a guarantee of free-tier access or quality. Use official supported-model/deprecation information as policy input, not HTML scraping.
- Workers AI: account-scoped model search with task=Text Generation; select callable model name (normally @cf/...), never numeric catalog record ID. Require compatibility with the Chat Completions endpoint. Task listing alone is not proof of compatibility.

## Privacy and reliability

- The policy JSON is public metadata only: no API keys, account IDs, email addresses, prompts, or usage records.
- Fetch policy over HTTPS only; cap response size; validate schema version, provider IDs, model ID lengths, enum values, and duplicate keys.
- Use a short timeout and no UI-thread network operations.
- Keep last-known-good policy and provider catalogs on any HTTP, parse, or schema error.
- Continue working offline with cached provider catalogs and policy.
- Do not send model probes automatically at startup. Probes consume quota; run only on explicit user action or as part of a real user-requested operation.
- Never label a model as free solely because it appears in a provider API catalog.

## Rollout

1. Add and validate policy schema and initial curated policy data.
2. Add common normalized records and provider adapters while preserving existing settings and catalog files.
3. Add remote policy download, cache, and merge.
4. Update settings UI to show capability and policy status distinctly.
5. Add user-triggered model test and operation-specific quality evaluation.
6. Remove duplicated hard-coded descriptions only after parity is verified.

## Acceptance criteria

- All three providers use the same normalized model record and policy status vocabulary.
- API list changes do not require a DLL rebuild.
- Policy recommendation changes do not require a DLL rebuild.
- An API outage or invalid policy file leaves last-known-good catalogs usable.
- Unsupported modalities/tasks are not offered as mail text-generation models.
- An untested model is never presented as quality-verified or recommended.
- Existing saved model IDs are not silently replaced.
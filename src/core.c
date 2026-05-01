#include <string.h>
#include "llm.h"

/* forward declarations for all providers */
LLMResponse openai_generate(const LLMRequest *req);
LLMResponse claude_generate(const LLMRequest *req);
LLMResponse groq_generate(const LLMRequest *req);
LLMResponse together_generate(const LLMRequest *req);
LLMResponse perplexity_generate(const LLMRequest *req);
LLMResponse deepseek_generate(const LLMRequest *req);
LLMResponse fireworks_generate(const LLMRequest *req);
LLMResponse xai_generate(const LLMRequest *req);
LLMResponse cerebras_generate(const LLMRequest *req);
LLMResponse sambanova_generate(const LLMRequest *req);
LLMResponse openrouter_generate(const LLMRequest *req);
LLMResponse ollama_generate(const LLMRequest *req);
LLMResponse gemini_generate(const LLMRequest *req);
LLMResponse cohere_generate(const LLMRequest *req);
LLMResponse mistral_generate(const LLMRequest *req);
LLMResponse replicate_generate(const LLMRequest *req);
LLMResponse huggingface_generate(const LLMRequest *req);

/**
 * Provider dispatch table.
 * Maps provider name strings to their generate functions.
 */
static const struct {
    const char *name;
    LLMResponse (*fn)(const LLMRequest *req);
} providers[] = {
    { "openai",      openai_generate },
    { "claude",      claude_generate },
    { "groq",        groq_generate },
    { "together",    together_generate },
    { "perplexity",  perplexity_generate },
    { "deepseek",    deepseek_generate },
    { "fireworks",   fireworks_generate },
    { "xai",         xai_generate },
    { "cerebras",    cerebras_generate },
    { "sambanova",   sambanova_generate },
    { "openrouter",  openrouter_generate },
    { "ollama",      ollama_generate },
    { "gemini",      gemini_generate },
    { "cohere",      cohere_generate },
    { "mistral",     mistral_generate },
    { "replicate",   replicate_generate },
    { "huggingface", huggingface_generate },
    { NULL, NULL }
};

static LLMResponse find_and_call(const char *provider, const LLMRequest *req) {
    for (int i = 0; providers[i].name; i++) {
        if (strcmp(provider, providers[i].name) == 0) {
            return providers[i].fn(req);
        }
    }

    LLMResponse res = {0};
    res.success = 0;
    res.error = "Unknown provider";
    return res;
}

LLMResponse llm_generate(const char *provider, const LLMRequest *req)
{
    return find_and_call(provider, req);
}

LLMResponse llm_generate_stream(const char *provider, const LLMRequest *req)
{
    if (!req->stream || !req->stream_cb) {
        LLMResponse res = {0};
        res.success = 0;
        res.error = "Streaming requires stream=1 and stream_cb to be set";
        return res;
    }

    if (req->max_tokens <= 0) {
        LLMResponse res = {0};
        res.success = 0;
        res.error = "Streaming requires max_tokens > 0";
        return res;
    }

    return find_and_call(provider, req);
}

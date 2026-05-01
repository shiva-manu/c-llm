/**
 * perplexity.c - Perplexity AI provider (OpenAI-compatible).
 * Env: PERPLEXITY_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse perplexity_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.perplexity.ai/chat/completions",
        "PERPLEXITY_API_KEY", req);
}

/**
 * openrouter.c - OpenRouter provider (OpenAI-compatible).
 * Env: OPENROUTER_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse openrouter_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://openrouter.ai/api/v1/chat/completions",
        "OPENROUTER_API_KEY", req);
}

/**
 * sambanova.c - SambaNova provider (OpenAI-compatible).
 * Env: SAMBANOVA_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse sambanova_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.sambanova.ai/v1/chat/completions",
        "SAMBANOVA_API_KEY", req);
}

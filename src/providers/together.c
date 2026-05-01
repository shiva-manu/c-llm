/**
 * together.c - Together AI provider (OpenAI-compatible).
 * Env: TOGETHER_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse together_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.together.xyz/v1/chat/completions",
        "TOGETHER_API_KEY", req);
}

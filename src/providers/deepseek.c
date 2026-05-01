/**
 * deepseek.c - DeepSeek AI provider (OpenAI-compatible).
 * Env: DEEPSEEK_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse deepseek_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.deepseek.com/v1/chat/completions",
        "DEEPSEEK_API_KEY", req);
}

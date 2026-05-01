/**
 * fireworks.c - Fireworks AI provider (OpenAI-compatible).
 * Env: FIREWORKS_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse fireworks_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.fireworks.ai/inference/v1/chat/completions",
        "FIREWORKS_API_KEY", req);
}

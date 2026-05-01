/**
 * cerebras.c - Cerebras provider (OpenAI-compatible).
 * Env: CEREBRAS_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse cerebras_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.cerebras.ai/v1/chat/completions",
        "CEREBRAS_API_KEY", req);
}

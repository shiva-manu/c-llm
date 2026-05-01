/**
 * xai.c - xAI (Grok) provider (OpenAI-compatible).
 * Env: XAI_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse xai_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.x.ai/v1/chat/completions",
        "XAI_API_KEY", req);
}

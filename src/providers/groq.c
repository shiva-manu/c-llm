/**
 * groq.c - Groq LLM provider (OpenAI-compatible).
 * Env: GROQ_API_KEY
 */

#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse groq_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.groq.com/openai/v1/chat/completions",
        "GROQ_API_KEY", req);
}

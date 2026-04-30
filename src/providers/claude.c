/**
 * claude.c
 *
 * Placeholder backend for Anthropic Claude.
 *
 * This keeps the library linkable while the Claude integration is
 * still under development.
 */

#include "llm.h"

LLMResponse claude_generate(const LLMRequest *req)
{
    (void)req;

    LLMResponse res = {0};
    res.success = 0;
    res.error = "Claude provider not implemented yet";
    return res;
}


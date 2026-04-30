/**
 * llm.h
 *
 * Public interface for the LLM client library.
 * Defines the request/response structures and backend dispatch.
 *
 * Usage:
 *   LLMRequest req = {
 *       .model      = "gpt-4o-mini",
 *       .prompt     = "Hello!",
 *       .max_tokens = 100,
 *       .temperature = 0.7f
 *   };
 *   LLMResponse res = llm_generate("openai", &req);
 *   // on success: free(res.text)
 *   // on failure: res.error is a string literal, do NOT free
 */

#ifndef LLM_H
#define LLM_H

/**
 * LLMRequest - input parameters for a language model call.
 *
 * @model:       model identifier (e.g. "gpt-4o-mini", "gpt-4").
 * @prompt:      the user message to send.
 * @max_tokens:  maximum tokens the model should generate.
 * @temperature: sampling temperature, 0.0 = deterministic, 2.0 = very random.
 */
typedef struct {
    const char *model;
    const char *prompt;
    int         max_tokens;
    float       temperature;
} LLMRequest;

/**
 * LLMResponse - result returned by a backend.
 *
 * @success: 1 if the request succeeded, 0 on failure.
 * @text:    heap-allocated response body on success (caller must free).
 * @error:   human-readable error message on failure (string literal, do NOT free).
 */
typedef struct {
    int          success;
    char        *text;
    const char  *error;
} LLMResponse;

/**
 * llm_generate - dispatch a request to the named provider.
 *
 * @provider: backend name (e.g. "openai").
 * @req:      the request to send. Not modified.
 *
 * Return:
 *   LLMResponse. On success, .text is heap-allocated and the caller
 *   must free() it. On failure, .error is a static string literal.
 */
LLMResponse llm_generate(const char *provider, const LLMRequest *req);

#endif /* LLM_H */

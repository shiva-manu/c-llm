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
 *       .system     = "You are a helpful assistant.",
 *       .max_tokens = 100,
 *       .temperature = 0.7f
 *   };
 *   LLMResponse res = llm_generate("openai", &req);
 *   // on success: res.content has extracted text, free(res.text) for raw JSON
 *   // on failure: res.error is a string literal, do NOT free
 */

#ifndef LLM_H
#define LLM_H

/**
 * LLMStreamCallback - called for each token during streaming.
 *
 * @token:    the token text (null-terminated, valid only for the callback duration).
 * @user_data: opaque pointer passed through from LLMRequest.
 */
typedef void (*LLMStreamCallback)(const char *token, void *user_data);

/**
 * LLMRequest - input parameters for a language model call.
 *
 * @model:         model identifier (e.g. "gpt-4o-mini", "gpt-4").
 * @prompt:        the user message to send.
 * @system:        optional system prompt (NULL for none).
 * @max_tokens:    maximum tokens the model should generate.
 * @temperature:   sampling temperature, 0.0 = deterministic, 2.0 = very random.
 * @stream:        if non-zero, use streaming mode (call stream_cb for each token).
 * @stream_cb:     callback function for streaming tokens.
 * @stream_user_data: opaque pointer passed to stream_cb.
 */
typedef struct {
    const char       *model;
    const char       *prompt;
    const char       *system;
    int               max_tokens;
    float             temperature;
    int               stream;
    LLMStreamCallback stream_cb;
    void             *stream_user_data;
} LLMRequest;

/**
 * LLMResponse - result returned by a backend.
 *
 * @success: 1 if the request succeeded, 0 on failure.
 * @text:    heap-allocated raw JSON response on success (caller must free).
 * @content: heap-allocated extracted text content on success (caller must free).
 *           NULL if parsing failed or not applicable.
 * @error:   human-readable error message on failure (string literal, do NOT free).
 */
typedef struct {
    int          success;
    char        *text;
    char        *content;
    const char  *error;
} LLMResponse;

/**
 * llm_generate - dispatch a request to the named provider.
 *
 * @provider: backend name (e.g. "openai", "claude", "groq").
 * @req:      the request to send. Not modified.
 *
 * Return:
 *   LLMResponse. On success, .text is raw JSON (caller must free),
 *   .content is extracted text (caller must free).
 *   On failure, .error is a static string literal.
 */
LLMResponse llm_generate(const char *provider, const LLMRequest *req);

/**
 * llm_generate_stream - dispatch a streaming request to the named provider.
 *
 * The request must have stream=1, stream_cb set, and max_tokens > 0.
 * Tokens are delivered via the callback as they arrive.
 *
 * @provider: backend name.
 * @req:      the request to send (must have stream=1 and stream_cb set).
 *
 * Return:
 *   LLMResponse with success=1 on completion, or error on failure.
 *   .text and .content are NULL for streaming responses.
 */
LLMResponse llm_generate_stream(const char *provider, const LLMRequest *req);

#endif /* LLM_H */

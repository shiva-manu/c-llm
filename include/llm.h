/**
 * llm.h
 *
 * Public interface for the AnyLM client library.
 * Provides a unified API for 17+ LLM providers with streaming,
 * multi-turn conversations, vision, tool calling, and embeddings.
 *
 * Copyright (c) 2026 Shivamani - MIT License
 */

#ifndef LLM_H
#define LLM_H

/* ========================================================================
 * Callbacks
 * ======================================================================== */

/**
 * LLMStreamCallback - called for each token during streaming.
 *
 * @token:     token text (null-terminated, valid only for callback duration).
 * @user_data: opaque pointer from LLMRequest.stream_user_data.
 */
typedef void (*LLMStreamCallback)(const char *token, void *user_data);

/* ========================================================================
 * Multi-turn conversation types
 * ======================================================================== */

/**
 * LLMContentPart - a single content part in a multimodal message.
 *
 * @type:      "text" or "image_url".
 * @text:      the text content (when type == "text").
 * @image_url: image URL or data URI (when type == "image_url").
 *             Format: "https://..." or "data:image/jpeg;base64,...".
 */
typedef struct {
    const char *type;
    const char *text;
    const char *image_url;
} LLMContentPart;

/**
 * LLMMessage - a single message in a conversation.
 *
 * @role:      "system", "user", or "assistant".
 * @content:   simple text content (used when parts is NULL).
 * @parts:     multimodal content array (takes priority over content).
 * @num_parts: number of parts in the array.
 */
typedef struct {
    const char            *role;
    const char            *content;
    const LLMContentPart  *parts;
    int                    num_parts;
} LLMMessage;

/* ========================================================================
 * Tool/function calling types
 * ======================================================================== */

/**
 * LLMTool - a tool definition for function calling.
 *
 * @name:        function name (e.g. "get_weather").
 * @description: what the function does.
 * @parameters:  JSON schema string for the function parameters.
 */
typedef struct {
    const char *name;
    const char *description;
    const char *parameters;
} LLMTool;

/**
 * LLMToolCall - a tool call returned by the model.
 *
 * @id:        unique identifier for this tool call.
 * @name:      function name to call.
 * @arguments: JSON string of arguments.
 */
typedef struct {
    const char *id;
    const char *name;
    const char *arguments;
} LLMToolCall;

/* ========================================================================
 * Request
 * ======================================================================== */

/**
 * LLMRequest - input parameters for a language model call.
 *
 * Simple mode (backwards compatible):
 *   Set .prompt and optionally .system. Leave .messages NULL.
 *
 * Multi-turn mode:
 *   Set .messages and .num_messages. .prompt and .system are ignored.
 *
 * @model:           model identifier (e.g. "gpt-4o-mini", "claude-3-haiku").
 * @prompt:          simple mode: user message text.
 * @system:          simple mode: system prompt (NULL for none).
 * @messages:        multi-turn mode: array of messages (takes priority over prompt/system).
 * @num_messages:    number of messages in the array.
 * @max_tokens:      maximum tokens to generate.
 * @temperature:     0.0 = deterministic, 2.0 = very random.
 * @json_mode:       if non-zero, force JSON output from the model.
 * @tools:           array of tool definitions (NULL for none).
 * @num_tools:       number of tools in the array.
 * @timeout_ms:      request timeout in milliseconds (0 = no timeout).
 * @stream:          if non-zero, use streaming mode.
 * @stream_cb:       callback for streaming tokens.
 * @stream_user_data: opaque pointer passed to stream_cb.
 */
typedef struct {
    const char        *model;
    const char        *prompt;
    const char        *system;
    const LLMMessage  *messages;
    int                num_messages;
    int                max_tokens;
    float              temperature;
    int                json_mode;
    const LLMTool     *tools;
    int                num_tools;
    long               timeout_ms;
    int                stream;
    LLMStreamCallback  stream_cb;
    void              *stream_user_data;
} LLMRequest;

/* ========================================================================
 * Response
 * ======================================================================== */

/**
 * LLMResponse - result returned by a backend.
 *
 * @success:       1 if the request succeeded, 0 on failure.
 * @text:          heap-allocated raw JSON response (caller must free).
 * @content:       heap-allocated extracted text (caller must free). NULL if N/A.
 * @tool_calls:    heap-allocated array of tool calls (caller must free). NULL if none.
 * @num_tool_calls: number of tool calls in the array.
 * @error:         human-readable error on failure (static string, do NOT free).
 */
typedef struct {
    int           success;
    char         *text;
    char         *content;
    LLMToolCall  *tool_calls;
    int           num_tool_calls;
    const char   *error;
} LLMResponse;

/* ========================================================================
 * Embeddings types
 * ======================================================================== */

/**
 * LLMEmbedRequest - input for an embedding call.
 *
 * @model:      embedding model (e.g. "text-embedding-3-small").
 * @input:      text to embed.
 * @dimensions: desired dimensions (0 = model default).
 */
typedef struct {
    const char *model;
    const char *input;
    int         dimensions;
} LLMEmbedRequest;

/**
 * LLMEmbedResponse - result of an embedding call.
 *
 * @success:       1 on success, 0 on failure.
 * @embedding:     heap-allocated float array (caller must free).
 * @num_dimensions: number of floats in the embedding array.
 * @text:          heap-allocated raw JSON response (caller must free).
 * @error:         error message on failure (static string, do NOT free).
 */
typedef struct {
    int         success;
    float      *embedding;
    int         num_dimensions;
    char       *text;
    const char *error;
} LLMEmbedResponse;

/* ========================================================================
 * Public functions
 * ======================================================================== */

/**
 * llm_generate - dispatch a chat request to the named provider.
 *
 * Supports both simple mode (prompt/system) and multi-turn mode (messages).
 * Returns extracted text in .content, raw JSON in .text.
 */
LLMResponse llm_generate(const char *provider, const LLMRequest *req);

/**
 * llm_generate_stream - dispatch a streaming chat request.
 *
 * Requires stream=1 and stream_cb to be set.
 * Tokens arrive via the callback; .text and .content are NULL on success.
 */
LLMResponse llm_generate_stream(const char *provider, const LLMRequest *req);

/**
 * llm_embed - generate an embedding vector for the given text.
 *
 * Supported providers: "openai", "cohere", "gemini".
 * Returns a float array in .embedding, raw JSON in .text.
 */
LLMEmbedResponse llm_embed(const char *provider, const LLMEmbedRequest *req);

#endif /* LLM_H */

/**
 * parse.h
 *
 * Response JSON parsing helpers.
 * Extracts the text content from each provider's raw JSON response.
 */

#ifndef PARSE_H
#define PARSE_H

/**
 * parse_openai_response - extract text from OpenAI-compatible JSON.
 * Works for: openai, groq, together, perplexity, deepseek, fireworks,
 *            xai, cerebras, sambanova, openrouter, ollama, mistral.
 * Format: {"choices":[{"message":{"content":"..."}}]}
 */
char *parse_openai_response(const char *json);

/**
 * parse_claude_response - extract text from Anthropic Claude JSON.
 * Format: {"content":[{"text":"..."}]}
 */
char *parse_claude_response(const char *json);

/**
 * parse_gemini_response - extract text from Google Gemini JSON.
 * Format: {"candidates":[{"content":{"parts":[{"text":"..."}]}}]}
 */
char *parse_gemini_response(const char *json);

/**
 * parse_cohere_response - extract text from Cohere JSON.
 * Format: {"message":{"content":[{"text":"..."}]}}
 */
char *parse_cohere_response(const char *json);

/**
 * parse_replicate_response - extract text from Replicate JSON.
 * Format: {"output":["line1","line2",...]}
 */
char *parse_replicate_response(const char *json);

/**
 * parse_huggingface_response - extract text from Hugging Face JSON.
 * Format: [{"generated_text":"..."}]
 */
char *parse_huggingface_response(const char *json);

/**
 * parse_stream_openai - extract delta content from an SSE streaming chunk.
 * Format: {"choices":[{"delta":{"content":"..."}}]}
 * Returns NULL if no delta content in this chunk.
 */
char *parse_stream_openai(const char *json);

/**
 * parse_stream_claude - extract delta content from Claude streaming chunk.
 * Format: {"type":"content_block_delta","delta":{"text":"..."}}
 */
char *parse_stream_claude(const char *json);

/**
 * parse_stream_gemini - extract text from Gemini streaming chunk.
 * Same format as non-streaming: {"candidates":[{"content":{"parts":[{"text":"..."}]}}]}
 */
char *parse_stream_gemini(const char *json);

#endif /* PARSE_H */

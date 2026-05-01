/**
 * parse.h
 *
 * Response JSON parsing helpers.
 */

#ifndef PARSE_H
#define PARSE_H

#include "llm.h"

/* ---- Chat response parsers ---- */

char *parse_openai_response(const char *json);
char *parse_claude_response(const char *json);
char *parse_gemini_response(const char *json);
char *parse_cohere_response(const char *json);
char *parse_replicate_response(const char *json);
char *parse_huggingface_response(const char *json);

/* ---- Streaming parsers ---- */

char *parse_stream_openai(const char *json);
char *parse_stream_claude(const char *json);
char *parse_stream_gemini(const char *json);

/* ---- Tool call parsers ---- */

/**
 * Parse tool calls from a response. Returns a heap-allocated array
 * of LLMToolCall and sets *count. Returns NULL if no tool calls.
 * Caller must free each .id, .name, .arguments, and the array itself.
 */
LLMToolCall *parse_openai_tool_calls(const char *json, int *count);
LLMToolCall *parse_claude_tool_calls(const char *json, int *count);
LLMToolCall *parse_gemini_tool_calls(const char *json, int *count);

/* ---- Embedding parsers ---- */

/**
 * Parse embedding vectors. Returns heap-allocated float array,
 * sets *dimensions. Caller must free the array.
 */
float *parse_openai_embedding(const char *json, int *dimensions);
float *parse_cohere_embedding(const char *json, int *dimensions);
float *parse_gemini_embedding(const char *json, int *dimensions);

#endif /* PARSE_H */

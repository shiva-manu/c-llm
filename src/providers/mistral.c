/**
 * mistral.c
 *
 * Mistral AI Chat Completions API provider.
 *
 * Environment:
 *   MISTRAL_API_KEY must be set before calling mistral_generate().
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Build the JSON messages array with optional system prompt.
 */
static char *build_messages_json(const LLMRequest *req) {
    char *esc_prompt = json_escape(req->prompt);
    if (!esc_prompt) return NULL;

    if (req->system) {
        char *esc_system = json_escape(req->system);
        if (!esc_system) { free(esc_prompt); return NULL; }
        size_t len = strlen(esc_system) + strlen(esc_prompt) + 64;
        char *msg = malloc(len);
        if (!msg) { free(esc_system); free(esc_prompt); return NULL; }
        snprintf(msg, len,
            "[{\"role\":\"system\",\"content\":%s},{\"role\":\"user\",\"content\":%s}]",
            esc_system, esc_prompt);
        free(esc_system);
        free(esc_prompt);
        return msg;
    }

    size_t len = strlen(esc_prompt) + 64;
    char *msg = malloc(len);
    if (!msg) { free(esc_prompt); return NULL; }
    snprintf(msg, len, "[{\"role\":\"user\",\"content\":%s}]", esc_prompt);
    free(esc_prompt);
    return msg;
}

/* Streaming chunk context */
struct mistral_stream_ctx {
    LLMStreamCallback cb;
    void *user_data;
    char *buffer;
    size_t buf_len;
};

static int mistral_stream_chunk(const char *data, size_t size, void *user_data) {
    struct mistral_stream_ctx *ctx = (struct mistral_stream_ctx *)user_data;

    char *tmp = realloc(ctx->buffer, ctx->buf_len + size + 1);
    if (!tmp) return -1;
    ctx->buffer = tmp;
    memcpy(ctx->buffer + ctx->buf_len, data, size);
    ctx->buf_len += size;
    ctx->buffer[ctx->buf_len] = '\0';

    char *line_start = ctx->buffer;
    char *newline;
    size_t consumed = 0;

    while ((newline = memchr(line_start, '\n', ctx->buf_len - (line_start - ctx->buffer)))) {
        *newline = '\0';
        if (strncmp(line_start, "data: ", 6) == 0) {
            const char *payload = line_start + 6;
            if (strcmp(payload, "[DONE]") != 0) {
                char *token = parse_stream_openai(payload);
                if (token) {
                    ctx->cb(token, ctx->user_data);
                    free(token);
                }
            }
        }
        consumed = (newline + 1) - ctx->buffer;
        line_start = newline + 1;
    }

    if (consumed > 0) {
        memmove(ctx->buffer, ctx->buffer + consumed, ctx->buf_len - consumed);
        ctx->buf_len -= consumed;
        ctx->buffer[ctx->buf_len] = '\0';
    }
    return 0;
}

LLMResponse mistral_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("MISTRAL_API_KEY");
    if (!api_key) {
        res.error = "Missing MISTRAL_API_KEY environment variable";
        return res;
    }

    char *esc_model = json_escape(req->model);
    if (!esc_model) { res.error = "Memory allocation failed"; return res; }

    char *messages = build_messages_json(req);
    if (!messages) { free(esc_model); res.error = "Memory allocation failed"; return res; }

    size_t body_len = strlen(esc_model) + strlen(messages) + 256;
    char *body = malloc(body_len);
    if (!body) { free(esc_model); free(messages); res.error = "Memory allocation failed"; return res; }

    if (req->stream && req->stream_cb) {
        snprintf(body, body_len,
            "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f,\"stream\":true}",
            esc_model, messages, req->max_tokens, req->temperature);
    } else {
        snprintf(body, body_len,
            "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f}",
            esc_model, messages, req->max_tokens, req->temperature);
    }

    free(esc_model); free(messages);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    if (req->stream && req->stream_cb) {
        struct mistral_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream("https://api.mistral.ai/v1/chat/completions",
                                  headers, body, mistral_stream_chunk, &ctx);
        free(body); free(ctx.buffer);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    char *response = http_post("https://api.mistral.ai/v1/chat/completions", headers, body);
    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_openai_response(response);
    res.success = 1;
    return res;
}

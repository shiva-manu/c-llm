/**
 * claude.c
 *
 * Anthropic Claude Messages API provider.
 *
 * Environment:
 *   ANTHROPIC_API_KEY must be set before calling claude_generate().
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Streaming chunk context for Claude */
struct claude_stream_ctx {
    LLMStreamCallback cb;
    void *user_data;
    char *buffer;
    size_t buf_len;
};

static int claude_stream_chunk(const char *data, size_t size, void *user_data) {
    struct claude_stream_ctx *ctx = (struct claude_stream_ctx *)user_data;

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
            char *token = parse_stream_claude(line_start + 6);
            if (token) {
                ctx->cb(token, ctx->user_data);
                free(token);
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

LLMResponse claude_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("ANTHROPIC_API_KEY");
    if (!api_key) {
        res.error = "Missing ANTHROPIC_API_KEY environment variable";
        return res;
    }

    char *esc_model  = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);
    if (!esc_model || !esc_prompt) {
        free(esc_model); free(esc_prompt);
        res.error = "Memory allocation failed";
        return res;
    }

    /* Build body with optional system prompt */
    char *esc_system = NULL;
    if (req->system) {
        esc_system = json_escape(req->system);
        if (!esc_system) {
            free(esc_model); free(esc_prompt);
            res.error = "Memory allocation failed";
            return res;
        }
    }

    size_t body_len = strlen(esc_model) + strlen(esc_prompt) +
                      (esc_system ? strlen(esc_system) : 0) + 512;
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(esc_prompt); free(esc_system);
        res.error = "Memory allocation failed";
        return res;
    }

    if (esc_system && req->stream && req->stream_cb) {
        snprintf(body, body_len,
            "{\"model\":%s,\"max_tokens\":%d,\"temperature\":%.2f,"
            "\"system\":%s,\"messages\":[{\"role\":\"user\",\"content\":%s}],\"stream\":true}",
            esc_model, req->max_tokens, req->temperature, esc_system, esc_prompt);
    } else if (esc_system) {
        snprintf(body, body_len,
            "{\"model\":%s,\"max_tokens\":%d,\"temperature\":%.2f,"
            "\"system\":%s,\"messages\":[{\"role\":\"user\",\"content\":%s}]}",
            esc_model, req->max_tokens, req->temperature, esc_system, esc_prompt);
    } else if (req->stream && req->stream_cb) {
        snprintf(body, body_len,
            "{\"model\":%s,\"max_tokens\":%d,\"temperature\":%.2f,"
            "\"messages\":[{\"role\":\"user\",\"content\":%s}],\"stream\":true}",
            esc_model, req->max_tokens, req->temperature, esc_prompt);
    } else {
        snprintf(body, body_len,
            "{\"model\":%s,\"max_tokens\":%d,\"temperature\":%.2f,"
            "\"messages\":[{\"role\":\"user\",\"content\":%s}]}",
            esc_model, req->max_tokens, req->temperature, esc_prompt);
    }

    free(esc_model); free(esc_prompt); free(esc_system);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    char auth[512];
    snprintf(auth, sizeof(auth), "x-api-key: %s", api_key);
    headers = curl_slist_append(headers, auth);

    if (req->stream && req->stream_cb) {
        struct claude_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream("https://api.anthropic.com/v1/messages", headers, body,
                                  claude_stream_chunk, &ctx);
        free(body); free(ctx.buffer);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    char *response = http_post("https://api.anthropic.com/v1/messages", headers, body);
    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_claude_response(response);
    res.success = 1;
    return res;
}

/**
 * gemini.c
 *
 * Google Gemini (Generative Language API) provider.
 *
 * Environment:
 *   GEMINI_API_KEY must be set before calling gemini_generate().
 *
 * The API key is passed as a URL query parameter, not in headers.
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Streaming chunk context for Gemini */
struct gemini_stream_ctx {
    LLMStreamCallback cb;
    void *user_data;
    char *buffer;
    size_t buf_len;
};

static int gemini_stream_chunk(const char *data, size_t size, void *user_data) {
    struct gemini_stream_ctx *ctx = (struct gemini_stream_ctx *)user_data;

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
            char *token = parse_stream_gemini(line_start + 6);
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

LLMResponse gemini_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key) {
        res.error = "Missing GEMINI_API_KEY environment variable";
        return res;
    }

    char *esc_model  = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);
    if (!esc_model || !esc_prompt) {
        free(esc_model); free(esc_prompt);
        res.error = "Memory allocation failed";
        return res;
    }

    char *esc_system = NULL;
    if (req->system) {
        esc_system = json_escape(req->system);
        if (!esc_system) { free(esc_model); free(esc_prompt); res.error = "Memory allocation failed"; return res; }
    }

    size_t body_len = strlen(esc_prompt) + (esc_system ? strlen(esc_system) : 0) + 512;
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(esc_prompt); free(esc_system);
        res.error = "Memory allocation failed";
        return res;
    }

    if (esc_system) {
        snprintf(body, body_len,
            "{\"contents\":[{\"parts\":[{\"text\":%s}]}],"
            "\"systemInstruction\":{\"parts\":[{\"text\":%s}]},"
            "\"generationConfig\":{\"maxOutputTokens\":%d,\"temperature\":%.2f}}",
            esc_prompt, esc_system, req->max_tokens, req->temperature);
    } else {
        snprintf(body, body_len,
            "{\"contents\":[{\"parts\":[{\"text\":%s}]}],"
            "\"generationConfig\":{\"maxOutputTokens\":%d,\"temperature\":%.2f}}",
            esc_prompt, req->max_tokens, req->temperature);
    }

    free(esc_prompt); free(esc_system);

    /* Strip quotes from esc_model for URL path */
    size_t model_len = strlen(esc_model);
    char model_buf[256] = {0};
    if (model_len > 2) {
        size_t copy_len = model_len - 2;
        if (copy_len >= sizeof(model_buf)) copy_len = sizeof(model_buf) - 1;
        memcpy(model_buf, esc_model + 1, copy_len);
    }
    free(esc_model);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (req->stream && req->stream_cb) {
        char url[1024];
        snprintf(url, sizeof(url),
            "https://generativelanguage.googleapis.com/v1beta/models/%s:streamGenerateContent?alt=sse&key=%s",
            model_buf, api_key);

        struct gemini_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream(url, headers, body, gemini_stream_chunk, &ctx);
        free(body); free(ctx.buffer);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    char url[1024];
    snprintf(url, sizeof(url),
        "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
        model_buf, api_key);

    char *response = http_post(url, headers, body);
    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_gemini_response(response);
    res.success = 1;
    return res;
}

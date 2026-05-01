/**
 * openai.c
 *
 * OpenAI Chat Completions API provider.
 *
 * Environment:
 *   OPENAI_API_KEY must be set before calling openai_generate().
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

static LLMResponse do_openai_request(const char *url, const char *api_key,
                                      const LLMRequest *req, int is_stream) {
    LLMResponse res = {0};

    char *esc_model = json_escape(req->model);
    if (!esc_model) { res.error = "Memory allocation failed"; return res; }

    char *messages = build_messages_json(req);
    if (!messages) { free(esc_model); res.error = "Memory allocation failed"; return res; }

    size_t body_len = strlen(esc_model) + strlen(messages) + 256;
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(messages);
        res.error = "Memory allocation failed";
        return res;
    }

    if (is_stream) {
        snprintf(body, body_len,
            "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f,\"stream\":true}",
            esc_model, messages, req->max_tokens, req->temperature);
    } else {
        snprintf(body, body_len,
            "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f}",
            esc_model, messages, req->max_tokens, req->temperature);
    }

    free(esc_model);
    free(messages);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    if (is_stream) {
        /* Streaming mode */
        struct { LLMStreamCallback cb; void *ud; } ctx = { req->stream_cb, req->stream_user_data };
        /* We reuse the callback signature - the stream parser is called from within */
        int rc = http_post_stream(url, headers, body, NULL, &ctx);
        free(body);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    char *response = http_post(url, headers, body);
    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_openai_response(response);
    res.success = 1;
    return res;
}

/* Streaming chunk context */
struct openai_stream_ctx {
    LLMStreamCallback cb;
    void *user_data;
    char *buffer;
    size_t buf_len;
};

static int openai_stream_chunk(const char *data, size_t size, void *user_data) {
    struct openai_stream_ctx *ctx = (struct openai_stream_ctx *)user_data;

    /* Append to buffer */
    char *tmp = realloc(ctx->buffer, ctx->buf_len + size + 1);
    if (!tmp) return -1;
    ctx->buffer = tmp;
    memcpy(ctx->buffer + ctx->buf_len, data, size);
    ctx->buf_len += size;
    ctx->buffer[ctx->buf_len] = '\0';

    /* Process complete SSE lines */
    char *line_start = ctx->buffer;
    char *newline;
    size_t consumed = 0;

    while ((newline = memchr(line_start, '\n', ctx->buf_len - (line_start - ctx->buffer)))) {
        *newline = '\0';
        size_t line_len = newline - line_start;

        if (line_len > 0) {
            /* Check for "data: " prefix */
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
        }

        consumed = (newline + 1) - ctx->buffer;
        line_start = newline + 1;
    }

    /* Move remaining data to front */
    if (consumed > 0) {
        memmove(ctx->buffer, ctx->buffer + consumed, ctx->buf_len - consumed);
        ctx->buf_len -= consumed;
        ctx->buffer[ctx->buf_len] = '\0';
    }

    return 0;
}

static LLMResponse do_openai_request_stream(const char *url, const char *api_key,
                                             const LLMRequest *req) {
    LLMResponse res = {0};

    char *esc_model = json_escape(req->model);
    if (!esc_model) { res.error = "Memory allocation failed"; return res; }

    char *messages = build_messages_json(req);
    if (!messages) { free(esc_model); res.error = "Memory allocation failed"; return res; }

    size_t body_len = strlen(esc_model) + strlen(messages) + 256;
    char *body = malloc(body_len);
    if (!body) { free(esc_model); free(messages); res.error = "Memory allocation failed"; return res; }

    snprintf(body, body_len,
        "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f,\"stream\":true}",
        esc_model, messages, req->max_tokens, req->temperature);

    free(esc_model);
    free(messages);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    struct openai_stream_ctx ctx = {
        req->stream_cb, req->stream_user_data, NULL, 0
    };

    int rc = http_post_stream(url, headers, body, openai_stream_chunk, &ctx);

    free(body);
    free(ctx.buffer);
    curl_slist_free_all(headers);

    if (rc != 0) { res.error = "Streaming request failed"; return res; }
    res.success = 1;
    return res;
}

LLMResponse openai_generate(const LLMRequest *req) {
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        LLMResponse res = {0};
        res.error = "Missing OPENAI_API_KEY environment variable";
        return res;
    }
    if (req->stream && req->stream_cb) {
        return do_openai_request_stream("https://api.openai.com/v1/chat/completions", api_key, req);
    }
    return do_openai_request("https://api.openai.com/v1/chat/completions", api_key, req, 0);
}

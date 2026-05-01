/**
 * ollama.c - Ollama provider (OpenAI-compatible, local inference).
 * Env: OLLAMA_API_KEY (optional)
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char *serialize_msg(const LLMMessage *msg) {
    char *r = json_escape(msg->role);
    char *c = json_escape(msg->content ? msg->content : "");
    size_t len = (r?strlen(r):0) + (c?strlen(c):0) + 32;
    char *out = malloc(len);
    if (out) snprintf(out, len, "{\"role\":%s,\"content\":%s}", r, c);
    free(r); free(c);
    return out;
}

struct ollama_stream_ctx {
    LLMStreamCallback cb;
    void *user_data;
    char *buffer;
    size_t buf_len;
};

static int ollama_stream_chunk(const char *data, size_t size, void *ud) {
    struct ollama_stream_ctx *ctx = (struct ollama_stream_ctx *)ud;
    char *tmp = realloc(ctx->buffer, ctx->buf_len + size + 1);
    if (!tmp) return -1;
    ctx->buffer = tmp;
    memcpy(ctx->buffer + ctx->buf_len, data, size);
    ctx->buf_len += size;
    ctx->buffer[ctx->buf_len] = '\0';

    char *line = ctx->buffer;
    char *nl;
    size_t consumed = 0;
    while ((nl = memchr(line, '\n', ctx->buf_len - (line - ctx->buffer)))) {
        *nl = '\0';
        if (strncmp(line, "data: ", 6) == 0 && strcmp(line+6, "[DONE]") != 0) {
            char *token = parse_stream_openai(line + 6);
            if (token) { ctx->cb(token, ctx->user_data); free(token); }
        }
        consumed = (nl + 1) - ctx->buffer;
        line = nl + 1;
    }
    if (consumed > 0) {
        memmove(ctx->buffer, ctx->buffer + consumed, ctx->buf_len - consumed);
        ctx->buf_len -= consumed;
        ctx->buffer[ctx->buf_len] = '\0';
    }
    return 0;
}

LLMResponse ollama_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("OLLAMA_API_KEY");
    if (!api_key) api_key = "ollama";

    char *esc_model = json_escape(req->model);
    if (!esc_model) { res.error = "Memory allocation failed"; return res; }

    /* Build messages */
    char *messages_json = NULL;
    if (req->messages && req->num_messages > 0) {
        char **parts = malloc(req->num_messages * sizeof(char *));
        if (!parts) { free(esc_model); res.error = "Memory allocation failed"; return res; }
        size_t cap = 4;
        for (int i = 0; i < req->num_messages; i++) {
            parts[i] = serialize_msg(&req->messages[i]);
            if (parts[i]) cap += strlen(parts[i]) + 2;
        }
        messages_json = malloc(cap);
        if (!messages_json) {
            for (int i = 0; i < req->num_messages; i++) free(parts[i]);
            free(parts); free(esc_model);
            res.error = "Memory allocation failed"; return res;
        }
        int pos = snprintf(messages_json, cap, "[");
        for (int i = 0; i < req->num_messages; i++) {
            if (i > 0) messages_json[pos++] = ',';
            if (parts[i]) { int l=strlen(parts[i]); memcpy(messages_json+pos, parts[i], l); pos+=l; }
        }
        messages_json[pos] = ']'; messages_json[pos+1] = '\0';
        for (int i = 0; i < req->num_messages; i++) free(parts[i]);
        free(parts);
    } else {
        LLMMessage sm = {"system", req->system, NULL, 0};
        LLMMessage um = {"user", req->prompt, NULL, 0};
        char *s = req->system ? serialize_msg(&sm) : NULL;
        char *u = serialize_msg(&um);
        size_t len = (s?strlen(s):0) + (u?strlen(u):0) + 8;
        messages_json = malloc(len);
        if (!messages_json) { free(s); free(u); free(esc_model); res.error = "Memory allocation failed"; return res; }
        int pos = snprintf(messages_json, len, "[");
        if (s) { int l=strlen(s); memcpy(messages_json+pos, s, l); pos+=l; }
        if (s && u) messages_json[pos++] = ',';
        if (u) { int l=strlen(u); memcpy(messages_json+pos, u, l); pos+=l; }
        messages_json[pos] = ']'; messages_json[pos+1] = '\0';
        free(s); free(u);
    }

    size_t body_len = strlen(esc_model) + strlen(messages_json) + 256;
    char *body = malloc(body_len);
    if (!body) { free(esc_model); free(messages_json); res.error = "Memory allocation failed"; return res; }

    int pos = snprintf(body, body_len,
        "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f",
        esc_model, messages_json, req->max_tokens, req->temperature);
    if (req->json_mode) pos += snprintf(body+pos, body_len-pos, ",\"response_format\":{\"type\":\"json_object\"}");
    if (req->stream) pos += snprintf(body+pos, body_len-pos, ",\"stream\":true");
    body[pos] = '}'; body[pos+1] = '\0';

    free(esc_model); free(messages_json);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    if (req->stream && req->stream_cb) {
        struct ollama_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream("http://localhost:11434/v1/chat/completions",
                                  headers, body, req->timeout_ms, ollama_stream_chunk, &ctx);
        free(body); free(ctx.buffer);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    int http_status = 0;
    char *response = http_post_retry("http://localhost:11434/v1/chat/completions",
                                     headers, body, req->timeout_ms, &http_status);
    free(body);
    curl_slist_free_all(headers);

    if (!response) {
        static char err[128];
        snprintf(err, sizeof(err), "HTTP request failed (status %d)", http_status);
        res.error = err;
        return res;
    }

    res.text = response;
    res.content = parse_openai_response(response);
    res.tool_calls = parse_openai_tool_calls(response, &res.num_tool_calls);
    res.success = 1;
    return res;
}

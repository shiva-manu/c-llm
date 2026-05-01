/**
 * gemini.c - Google Gemini provider.
 * Env: GEMINI_API_KEY
 */

#define _POSIX_C_SOURCE 200809L
#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

    char *line = ctx->buffer;
    char *nl;
    size_t consumed = 0;
    while ((nl = memchr(line, '\n', ctx->buf_len - (line - ctx->buffer)))) {
        *nl = '\0';
        if (strncmp(line, "data: ", 6) == 0) {
            char *token = parse_stream_gemini(line + 6);
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

LLMResponse gemini_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key) { res.error = "Missing GEMINI_API_KEY environment variable"; return res; }

    /* Extract model name (strip quotes from json_escape) */
    char *esc_model = json_escape(req->model);
    if (!esc_model) { res.error = "Memory allocation failed"; return res; }
    char model_buf[256] = {0};
    size_t ml = strlen(esc_model);
    if (ml > 2) {
        size_t cl = ml - 2;
        if (cl >= sizeof(model_buf)) cl = sizeof(model_buf) - 1;
        memcpy(model_buf, esc_model + 1, cl);
    }
    free(esc_model);

    /* Build contents array */
    char *contents_json = NULL;
    char *system_text = NULL;

    if (req->messages && req->num_messages > 0) {
        size_t cap = 4;
        for (int i = 0; i < req->num_messages; i++) {
            if (strcmp(req->messages[i].role, "system") == 0) {
                system_text = strdup(req->messages[i].content ? req->messages[i].content : "");
                continue;
            }
            char *esc_c = json_escape(req->messages[i].content ? req->messages[i].content : "");
            size_t part_len = esc_c ? strlen(esc_c) : 0;
            cap += part_len + 128;
            free(esc_c);
        }
        contents_json = malloc(cap);
        if (!contents_json) { free(system_text); res.error = "Memory allocation failed"; return res; }
        int pos = snprintf(contents_json, cap, "[");
        for (int i = 0; i < req->num_messages; i++) {
            if (strcmp(req->messages[i].role, "system") == 0) continue;
            if (pos > 1) contents_json[pos++] = ',';
            const char *role = strcmp(req->messages[i].role, "assistant") == 0 ? "model" : "user";
            char *esc_c = json_escape(req->messages[i].content ? req->messages[i].content : "");
            pos += snprintf(contents_json+pos, cap-pos,
                "{\"role\":\"%s\",\"parts\":[{\"text\":%s}]}", role, esc_c);
            free(esc_c);
        }
        contents_json[pos] = ']'; contents_json[pos+1] = '\0';
    } else {
        if (req->system) system_text = strdup(req->system);
        char *esc_p = json_escape(req->prompt ? req->prompt : "");
        size_t len = (esc_p ? strlen(esc_p) : 0) + 64;
        contents_json = malloc(len);
        if (!contents_json) { free(esc_p); free(system_text); res.error = "Memory allocation failed"; return res; }
        snprintf(contents_json, len, "[{\"role\":\"user\",\"parts\":[{\"text\":%s}]}]", esc_p);
        free(esc_p);
    }

    /* Build tools JSON */
    char *tools_json = NULL;
    if (req->tools && req->num_tools > 0) {
        size_t tlen = 32;
        for (int i = 0; i < req->num_tools; i++) {
            char *n=json_escape(req->tools[i].name); char *d=json_escape(req->tools[i].description); char *p=json_escape(req->tools[i].parameters);
            tlen += (n?strlen(n):0)+(d?strlen(d):0)+(p?strlen(p):0)+128;
            free(n); free(d); free(p);
        }
        tools_json = malloc(tlen);
        if (tools_json) {
            int pos = snprintf(tools_json, tlen, "\"tools\":[{\"functionDeclarations\":[");
            for (int i = 0; i < req->num_tools; i++) {
                if (i > 0) tools_json[pos++] = ',';
                char *n=json_escape(req->tools[i].name); char *d=json_escape(req->tools[i].description); char *p=json_escape(req->tools[i].parameters);
                pos += snprintf(tools_json+pos, tlen-pos,
                    "{\"name\":%s,\"description\":%s,\"parameters\":%s}", n, d, p);
                free(n); free(d); free(p);
            }
            snprintf(tools_json+pos, tlen-pos, "]}]");
        }
    }

    /* Build body */
    size_t body_len = strlen(contents_json) + 512;
    char *esc_sys = NULL;
    if (system_text) { esc_sys = json_escape(system_text); if(esc_sys) body_len += strlen(esc_sys); }
    if (tools_json) body_len += strlen(tools_json);
    char *body = malloc(body_len);
    if (!body) {
        free(contents_json); free(esc_sys); free(tools_json); free(system_text);
        res.error = "Memory allocation failed";
        return res;
    }

    int pos = snprintf(body, body_len, "{\"contents\":%s", contents_json);
    if (esc_sys) pos += snprintf(body+pos, body_len-pos, ",\"systemInstruction\":{\"parts\":[{\"text\":%s}]}", esc_sys);
    pos += snprintf(body+pos, body_len-pos,
        ",\"generationConfig\":{\"maxOutputTokens\":%d,\"temperature\":%.2f", req->max_tokens, req->temperature);
    if (req->json_mode) pos += snprintf(body+pos, body_len-pos, ",\"responseMimeType\":\"application/json\"");
    pos += snprintf(body+pos, body_len-pos, "}");
    if (tools_json) pos += snprintf(body+pos, body_len-pos, ",%s", tools_json);
    body[pos] = '}'; body[pos+1] = '\0';

    free(contents_json); free(esc_sys); free(tools_json); free(system_text);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (req->stream && req->stream_cb) {
        char url[1024];
        snprintf(url, sizeof(url),
            "https://generativelanguage.googleapis.com/v1beta/models/%s:streamGenerateContent?alt=sse&key=%s",
            model_buf, api_key);
        struct gemini_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream(url, headers, body, req->timeout_ms, gemini_stream_chunk, &ctx);
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

    int http_status = 0;
    char *response = http_post_retry(url, headers, body, req->timeout_ms, &http_status);
    free(body);
    curl_slist_free_all(headers);

    if (!response) {
        static char err[128];
        snprintf(err, sizeof(err), "HTTP request failed (status %d)", http_status);
        res.error = err;
        return res;
    }

    res.text = response;
    res.content = parse_gemini_response(response);
    res.tool_calls = parse_gemini_tool_calls(response, &res.num_tool_calls);
    res.success = 1;
    return res;
}

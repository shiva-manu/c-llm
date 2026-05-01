/**
 * openai.c - OpenAI Chat Completions API provider.
 * Env: OPENAI_API_KEY
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Build a single message JSON object from an LLMMessage */
static char *serialize_openai_message(const LLMMessage *msg) {
    char *esc_role = json_escape(msg->role);

    if (msg->parts && msg->num_parts > 0) {
        /* Multimodal: build content array */
        size_t len = strlen(esc_role) + 32;
        for (int i = 0; i < msg->num_parts; i++) {
            if (strcmp(msg->parts[i].type, "text") == 0 && msg->parts[i].text) {
                char *t = json_escape(msg->parts[i].text);
                if (t) { len += strlen(t) + 64; free(t); }
            } else if (strcmp(msg->parts[i].type, "image_url") == 0 && msg->parts[i].image_url) {
                char *u = json_escape(msg->parts[i].image_url);
                if (u) { len += strlen(u) + 96; free(u); }
            }
        }
        char *out = malloc(len);
        if (!out) { free(esc_role); return NULL; }
        int pos = snprintf(out, len, "{\"role\":%s,\"content\":[", esc_role);
        for (int i = 0; i < msg->num_parts; i++) {
            if (i > 0) out[pos++] = ',';
            if (strcmp(msg->parts[i].type, "text") == 0 && msg->parts[i].text) {
                char *t = json_escape(msg->parts[i].text);
                pos += snprintf(out + pos, len - pos,
                    "{\"type\":\"text\",\"text\":%s}", t);
                free(t);
            } else if (strcmp(msg->parts[i].type, "image_url") == 0 && msg->parts[i].image_url) {
                char *u = json_escape(msg->parts[i].image_url);
                pos += snprintf(out + pos, len - pos,
                    "{\"type\":\"image_url\",\"image_url\":{\"url\":%s}}", u);
                free(u);
            }
        }
        snprintf(out + pos, len - pos, "]}");
        free(esc_role);
        return out;
    }

    /* Simple text message */
    char *esc_content = json_escape(msg->content ? msg->content : "");
    size_t len = strlen(esc_role) + strlen(esc_content) + 32;
    char *out = malloc(len);
    if (!out) { free(esc_role); free(esc_content); return NULL; }
    snprintf(out, len, "{\"role\":%s,\"content\":%s}", esc_role, esc_content);
    free(esc_role);
    free(esc_content);
    return out;
}

/* Build the full request body */
static char *build_body(const LLMRequest *req) {
    char *esc_model = json_escape(req->model);
    if (!esc_model) return NULL;

    /* Build messages array */
    char *messages_json = NULL;
    size_t msg_cap = 0;

    if (req->messages && req->num_messages > 0) {
        /* Multi-turn mode */
        char **parts = malloc(req->num_messages * sizeof(char *));
        if (!parts) { free(esc_model); return NULL; }
        msg_cap = 4;
        for (int i = 0; i < req->num_messages; i++) {
            parts[i] = serialize_openai_message(&req->messages[i]);
            if (parts[i]) msg_cap += strlen(parts[i]) + 2;
        }
        messages_json = malloc(msg_cap);
        if (!messages_json) {
            for (int i = 0; i < req->num_messages; i++) free(parts[i]);
            free(parts); free(esc_model); return NULL;
        }
        int pos = snprintf(messages_json, msg_cap, "[");
        for (int i = 0; i < req->num_messages; i++) {
            if (i > 0) messages_json[pos++] = ',';
            if (parts[i]) {
                int len = strlen(parts[i]);
                memcpy(messages_json + pos, parts[i], len);
                pos += len;
            }
        }
        messages_json[pos] = ']'; messages_json[pos+1] = '\0';
        for (int i = 0; i < req->num_messages; i++) free(parts[i]);
        free(parts);
    } else {
        /* Simple mode */
        LLMMessage sys_msg = {"system", req->system, NULL, 0};
        LLMMessage usr_msg = {"user", req->prompt, NULL, 0};
        char *s = req->system ? serialize_openai_message(&sys_msg) : NULL;
        char *u = serialize_openai_message(&usr_msg);
        size_t len = (s ? strlen(s) : 0) + (u ? strlen(u) : 0) + 8;
        messages_json = malloc(len);
        if (!messages_json) { free(s); free(u); free(esc_model); return NULL; }
        int pos = snprintf(messages_json, len, "[");
        if (s) { int l = strlen(s); memcpy(messages_json+pos, s, l); pos += l; }
        if (s && u) messages_json[pos++] = ',';
        if (u) { int l = strlen(u); memcpy(messages_json+pos, u, l); pos += l; }
        messages_json[pos] = ']'; messages_json[pos+1] = '\0';
        free(s); free(u);
    }

    /* Build tools JSON */
    char *tools_json = NULL;
    if (req->tools && req->num_tools > 0) {
        size_t tlen = 16;
        for (int i = 0; i < req->num_tools; i++) {
            char *n = json_escape(req->tools[i].name);
            char *d = json_escape(req->tools[i].description);
            char *p = json_escape(req->tools[i].parameters);
            tlen += (n?strlen(n):0) + (d?strlen(d):0) + (p?strlen(p):0) + 128;
            free(n); free(d); free(p);
        }
        tools_json = malloc(tlen);
        if (tools_json) {
            int pos = snprintf(tools_json, tlen, "\"tools\":[");
            for (int i = 0; i < req->num_tools; i++) {
                if (i > 0) tools_json[pos++] = ',';
                char *n = json_escape(req->tools[i].name);
                char *d = json_escape(req->tools[i].description);
                char *p = json_escape(req->tools[i].parameters);
                pos += snprintf(tools_json + pos, tlen - pos,
                    "{\"type\":\"function\",\"function\":{\"name\":%s,\"description\":%s,\"parameters\":%s}}",
                    n, d, p);
                free(n); free(d); free(p);
            }
            tools_json[pos] = ']'; tools_json[pos+1] = '\0';
        }
    }

    /* Build the full body */
    size_t body_len = strlen(esc_model) + strlen(messages_json) + 512;
    if (tools_json) body_len += strlen(tools_json);
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(messages_json); free(tools_json);
        return NULL;
    }

    int pos = snprintf(body, body_len,
        "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f",
        esc_model, messages_json, req->max_tokens, req->temperature);

    if (req->json_mode) {
        pos += snprintf(body + pos, body_len - pos,
            ",\"response_format\":{\"type\":\"json_object\"}");
    }
    if (tools_json) {
        pos += snprintf(body + pos, body_len - pos, ",%s", tools_json);
    }
    if (req->stream) {
        pos += snprintf(body + pos, body_len - pos, ",\"stream\":true");
    }
    body[pos] = '}'; body[pos+1] = '\0';

    free(esc_model); free(messages_json); free(tools_json);
    return body;
}

/* SSE stream chunk handler */
struct openai_stream_ctx {
    LLMStreamCallback cb;
    void *user_data;
    char *buffer;
    size_t buf_len;
};

static int openai_stream_chunk(const char *data, size_t size, void *user_data) {
    struct openai_stream_ctx *ctx = (struct openai_stream_ctx *)user_data;

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
            const char *payload = line + 6;
            if (strcmp(payload, "[DONE]") != 0) {
                char *token = parse_stream_openai(payload);
                if (token) { ctx->cb(token, ctx->user_data); free(token); }
            }
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

LLMResponse openai_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) { res.error = "Missing OPENAI_API_KEY environment variable"; return res; }

    char *body = build_body(req);
    if (!body) { res.error = "Memory allocation failed"; return res; }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    if (req->stream && req->stream_cb) {
        struct openai_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream("https://api.openai.com/v1/chat/completions",
                                  headers, body, req->timeout_ms, openai_stream_chunk, &ctx);
        free(body); free(ctx.buffer);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    int http_status = 0;
    char *response = http_post_retry("https://api.openai.com/v1/chat/completions",
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

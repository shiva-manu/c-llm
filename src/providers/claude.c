/**
 * claude.c - Anthropic Claude Messages API provider.
 * Env: ANTHROPIC_API_KEY
 */

#define _POSIX_C_SOURCE 200809L
#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char *serialize_claude_message(const LLMMessage *msg) {
    char *esc_role = json_escape(msg->role);

    if (msg->parts && msg->num_parts > 0) {
        size_t len = strlen(esc_role) + 32;
        for (int i = 0; i < msg->num_parts; i++) {
            if (msg->parts[i].text) { char *t=json_escape(msg->parts[i].text); if(t){len+=strlen(t)+64;free(t);} }
            if (msg->parts[i].image_url) { len += 256; }
        }
        char *out = malloc(len);
        if (!out) { free(esc_role); return NULL; }
        int pos = snprintf(out, len, "{\"role\":%s,\"content\":[", esc_role);
        for (int i = 0; i < msg->num_parts; i++) {
            if (i > 0) out[pos++] = ',';
            if (strcmp(msg->parts[i].type, "text") == 0 && msg->parts[i].text) {
                char *t = json_escape(msg->parts[i].text);
                pos += snprintf(out+pos, len-pos, "{\"type\":\"text\",\"text\":%s}", t);
                free(t);
            } else if (strcmp(msg->parts[i].type, "image_url") == 0 && msg->parts[i].image_url) {
                /* Claude expects base64 with media_type */
                const char *url = msg->parts[i].image_url;
                if (strncmp(url, "data:", 5) == 0) {
                    /* data:image/jpeg;base64,<data> */
                    const char *mt_start = url + 5;
                    const char *b64 = strchr(mt_start, ',');
                    if (b64) {
                        size_t mt_len = b64 - mt_start;
                        /* Remove ;base64 suffix */
                        const char *semi = memchr(mt_start, ';', mt_len);
                        if (semi) mt_len = semi - mt_start;
                        char media_type[64] = {0};
                        if (mt_len < sizeof(media_type)) {
                            memcpy(media_type, mt_start, mt_len);
                        }
                        b64++;
                        char *emt = json_escape(media_type);
                        char *eb64 = json_escape(b64);
                        pos += snprintf(out+pos, len-pos,
                            "{\"type\":\"image\",\"source\":{\"type\":\"base64\",\"media_type\":%s,\"data\":%s}}",
                            emt, eb64);
                        free(emt); free(eb64);
                    }
                }
            }
        }
        snprintf(out+pos, len-pos, "]}");
        free(esc_role);
        return out;
    }

    char *esc_content = json_escape(msg->content ? msg->content : "");
    size_t len = strlen(esc_role) + strlen(esc_content) + 32;
    char *out = malloc(len);
    if (!out) { free(esc_role); free(esc_content); return NULL; }
    snprintf(out, len, "{\"role\":%s,\"content\":%s}", esc_role, esc_content);
    free(esc_role); free(esc_content);
    return out;
}

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

    char *line = ctx->buffer;
    char *nl;
    size_t consumed = 0;
    while ((nl = memchr(line, '\n', ctx->buf_len - (line - ctx->buffer)))) {
        *nl = '\0';
        if (strncmp(line, "data: ", 6) == 0) {
            char *token = parse_stream_claude(line + 6);
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

LLMResponse claude_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("ANTHROPIC_API_KEY");
    if (!api_key) { res.error = "Missing ANTHROPIC_API_KEY environment variable"; return res; }

    /* Extract system message and build messages array */
    char *system_text = NULL;
    char *messages_json = NULL;

    if (req->messages && req->num_messages > 0) {
        /* Multi-turn: extract system, serialize rest */
        size_t cap = 4;
        for (int i = 0; i < req->num_messages; i++) {
            if (strcmp(req->messages[i].role, "system") == 0) {
                system_text = strdup(req->messages[i].content ? req->messages[i].content : "");
                continue;
            }
            char *s = serialize_claude_message(&req->messages[i]);
            if (s) { cap += strlen(s) + 2; free(s); }
        }
        messages_json = malloc(cap);
        if (!messages_json) { free(system_text); res.error = "Memory allocation failed"; return res; }
        int pos = snprintf(messages_json, cap, "[");
        for (int i = 0; i < req->num_messages; i++) {
            if (strcmp(req->messages[i].role, "system") == 0) continue;
            if (pos > 1) messages_json[pos++] = ',';
            char *s = serialize_claude_message(&req->messages[i]);
            if (s) { int l = strlen(s); memcpy(messages_json+pos, s, l); pos += l; free(s); }
        }
        messages_json[pos] = ']'; messages_json[pos+1] = '\0';
    } else {
        /* Simple mode */
        if (req->system) system_text = strdup(req->system);
        LLMMessage um = {"user", req->prompt, NULL, 0};
        char *u = serialize_claude_message(&um);
        size_t len = (u ? strlen(u) : 0) + 8;
        messages_json = malloc(len);
        if (!messages_json) { free(u); free(system_text); res.error = "Memory allocation failed"; return res; }
        snprintf(messages_json, len, "[%s]", u ? u : "");
        free(u);
    }

    /* Build tools JSON */
    char *tools_json = NULL;
    if (req->tools && req->num_tools > 0) {
        size_t tlen = 16;
        for (int i = 0; i < req->num_tools; i++) {
            char *n=json_escape(req->tools[i].name); char *d=json_escape(req->tools[i].description); char *p=json_escape(req->tools[i].parameters);
            tlen += (n?strlen(n):0)+(d?strlen(d):0)+(p?strlen(p):0)+128;
            free(n); free(d); free(p);
        }
        tools_json = malloc(tlen);
        if (tools_json) {
            int pos = snprintf(tools_json, tlen, "\"tools\":[");
            for (int i = 0; i < req->num_tools; i++) {
                if (i > 0) tools_json[pos++] = ',';
                char *n=json_escape(req->tools[i].name); char *d=json_escape(req->tools[i].description); char *p=json_escape(req->tools[i].parameters);
                pos += snprintf(tools_json+pos, tlen-pos,
                    "{\"name\":%s,\"description\":%s,\"input_schema\":%s}", n, d, p);
                free(n); free(d); free(p);
            }
            tools_json[pos] = ']'; tools_json[pos+1] = '\0';
        }
    }

    /* Build full body */
    char *esc_model = json_escape(req->model);
    char *esc_system = system_text ? json_escape(system_text) : NULL;
    if (req->json_mode && !esc_system) {
        esc_system = strdup("\"Respond only with valid JSON.\"");
    } else if (req->json_mode && esc_system) {
        /* Prepend JSON instruction to system */
        size_t len = strlen(esc_system) + 64;
        char *combined = malloc(len);
        if (combined) {
            snprintf(combined, len, "\"Respond only with valid JSON. %s", esc_system + 1); /* skip opening quote */
            free(esc_system);
            esc_system = combined;
        }
    }

    size_t body_len = (esc_model?strlen(esc_model):0) + strlen(messages_json) + 512;
    if (esc_system) body_len += strlen(esc_system);
    if (tools_json) body_len += strlen(tools_json);
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(esc_system); free(messages_json); free(tools_json); free(system_text);
        res.error = "Memory allocation failed";
        return res;
    }

    int pos = snprintf(body, body_len,
        "{\"model\":%s,\"max_tokens\":%d,\"temperature\":%.2f",
        esc_model, req->max_tokens, req->temperature);
    if (esc_system) pos += snprintf(body+pos, body_len-pos, ",\"system\":%s", esc_system);
    pos += snprintf(body+pos, body_len-pos, ",\"messages\":%s", messages_json);
    if (tools_json) pos += snprintf(body+pos, body_len-pos, ",%s", tools_json);
    if (req->stream) pos += snprintf(body+pos, body_len-pos, ",\"stream\":true");
    body[pos] = '}'; body[pos+1] = '\0';

    free(esc_model); free(esc_system); free(messages_json); free(tools_json); free(system_text);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    char auth[512];
    snprintf(auth, sizeof(auth), "x-api-key: %s", api_key);
    headers = curl_slist_append(headers, auth);

    if (req->stream && req->stream_cb) {
        struct claude_stream_ctx ctx = { req->stream_cb, req->stream_user_data, NULL, 0 };
        int rc = http_post_stream("https://api.anthropic.com/v1/messages",
                                  headers, body, req->timeout_ms, claude_stream_chunk, &ctx);
        free(body); free(ctx.buffer);
        curl_slist_free_all(headers);
        if (rc != 0) { res.error = "Streaming request failed"; return res; }
        res.success = 1;
        return res;
    }

    int http_status = 0;
    char *response = http_post_retry("https://api.anthropic.com/v1/messages",
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
    res.content = parse_claude_response(response);
    res.tool_calls = parse_claude_tool_calls(response, &res.num_tool_calls);
    res.success = 1;
    return res;
}

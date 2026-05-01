/**
 * cohere.c - Cohere Chat API provider.
 * Env: COHERE_API_KEY
 */

#define _POSIX_C_SOURCE 200809L
#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMResponse cohere_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("COHERE_API_KEY");
    if (!api_key) { res.error = "Missing COHERE_API_KEY environment variable"; return res; }

    char *esc_model = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);
    char *esc_system = NULL;
    if (req->system) esc_system = json_escape(req->system);
    if (req->json_mode && !esc_system) esc_system = strdup("\"Respond only with valid JSON.\"");

    /* Build messages array */
    char *messages_json = NULL;
    if (req->messages && req->num_messages > 0) {
        size_t cap = 4;
        for (int i = 0; i < req->num_messages; i++) {
            if (strcmp(req->messages[i].role, "system") == 0) {
                if (!esc_system) esc_system = json_escape(req->messages[i].content ? req->messages[i].content : "");
                continue;
            }
            char *c = json_escape(req->messages[i].content ? req->messages[i].content : "");
            cap += (c?strlen(c):0) + 128;
            free(c);
        }
        messages_json = malloc(cap);
        if (messages_json) {
            int pos = snprintf(messages_json, cap, "[");
            for (int i = 0; i < req->num_messages; i++) {
                if (strcmp(req->messages[i].role, "system") == 0) continue;
                if (pos > 1) messages_json[pos++] = ',';
                char *c = json_escape(req->messages[i].content ? req->messages[i].content : "");
                pos += snprintf(messages_json+pos, cap-pos,
                    "{\"role\":\"%s\",\"content\":%s}", req->messages[i].role, c);
                free(c);
            }
            messages_json[pos] = ']'; messages_json[pos+1] = '\0';
        }
    } else {
        size_t len = (esc_prompt?strlen(esc_prompt):0) + 64;
        messages_json = malloc(len);
        if (messages_json) snprintf(messages_json, len, "[{\"role\":\"user\",\"content\":%s}]", esc_prompt);
    }

    size_t body_len = (esc_model?strlen(esc_model):0) + (messages_json?strlen(messages_json):0) + 512;
    if (esc_system) body_len += strlen(esc_system);
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(esc_prompt); free(esc_system); free(messages_json);
        res.error = "Memory allocation failed"; return res;
    }

    int pos = snprintf(body, body_len,
        "{\"model\":%s,\"messages\":%s,\"max_tokens\":%d,\"temperature\":%.2f",
        esc_model, messages_json, req->max_tokens, req->temperature);
    if (esc_system) pos += snprintf(body+pos, body_len-pos, ",\"preamble\":%s", esc_system);
    body[pos] = '}'; body[pos+1] = '\0';

    free(esc_model); free(esc_prompt); free(esc_system); free(messages_json);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    int http_status = 0;
    char *response = http_post_retry("https://api.cohere.com/v2/chat",
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
    res.content = parse_cohere_response(response);
    res.success = 1;
    return res;
}

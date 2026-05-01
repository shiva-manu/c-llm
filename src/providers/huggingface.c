/**
 * huggingface.c - Hugging Face Inference API provider.
 * Env: HF_API_TOKEN
 */

#define _POSIX_C_SOURCE 200809L
#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMResponse huggingface_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_token = getenv("HF_API_TOKEN");
    if (!api_token) { res.error = "Missing HF_API_TOKEN environment variable"; return res; }

    /* Build input text with optional system prompt */
    char *input_text = NULL;
    if (req->system) {
        size_t len = strlen(req->system) + (req->prompt ? strlen(req->prompt) : 0) + 32;
        input_text = malloc(len);
        if (input_text) snprintf(input_text, len, "System: %s\n\nUser: %s", req->system, req->prompt ? req->prompt : "");
    } else {
        input_text = strdup(req->prompt ? req->prompt : "");
    }
    if (!input_text) { res.error = "Memory allocation failed"; return res; }

    char *esc_input = json_escape(input_text);
    free(input_text);
    if (!esc_input) { res.error = "Memory allocation failed"; return res; }

    size_t body_len = strlen(esc_input) + 256;
    char *body = malloc(body_len);
    if (!body) { free(esc_input); res.error = "Memory allocation failed"; return res; }

    snprintf(body, body_len,
        "{\"inputs\":%s,\"parameters\":{\"max_new_tokens\":%d,\"temperature\":%.2f}}",
        esc_input, req->max_tokens, req->temperature);
    free(esc_input);

    char url[1024];
    snprintf(url, sizeof(url), "https://api-inference.huggingface.co/models/%s", req->model);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_token);
    headers = curl_slist_append(headers, auth);

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
    res.content = parse_huggingface_response(response);
    res.success = 1;
    return res;
}

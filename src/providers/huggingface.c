/**
 * huggingface.c
 *
 * Hugging Face Inference API provider.
 *
 * Environment:
 *   HF_API_TOKEN must be set before calling huggingface_generate().
 *
 * The model field is used as the model ID in the URL path.
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMResponse huggingface_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_token = getenv("HF_API_TOKEN");
    if (!api_token) {
        res.error = "Missing HF_API_TOKEN environment variable";
        return res;
    }

    char *esc_prompt = json_escape(req->prompt);
    if (!esc_prompt) {
        res.error = "Memory allocation failed";
        return res;
    }

    /* Build body with optional system prompt prepended to input */
    char *esc_system = NULL;
    if (req->system) {
        esc_system = json_escape(req->system);
        if (!esc_system) { free(esc_prompt); res.error = "Memory allocation failed"; return res; }
    }

    size_t body_len = strlen(esc_prompt) + (esc_system ? strlen(esc_system) : 0) + 256;
    char *body = malloc(body_len);
    if (!body) { free(esc_prompt); free(esc_system); res.error = "Memory allocation failed"; return res; }

    if (esc_system) {
        /* Prepend system prompt as context */
        snprintf(body, body_len,
            "{\"inputs\":\"System: %s\\n\\nUser: %s\",\"parameters\":{\"max_new_tokens\":%d,\"temperature\":%.2f}}",
            esc_system, esc_prompt, req->max_tokens, req->temperature);
    } else {
        snprintf(body, body_len,
            "{\"inputs\":%s,\"parameters\":{\"max_new_tokens\":%d,\"temperature\":%.2f}}",
            esc_prompt, req->max_tokens, req->temperature);
    }

    free(esc_prompt); free(esc_system);

    char url[1024];
    snprintf(url, sizeof(url),
        "https://api-inference.huggingface.co/models/%s", req->model);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_token);
    headers = curl_slist_append(headers, auth);

    char *response = http_post(url, headers, body);
    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_huggingface_response(response);
    res.success = 1;
    return res;
}

/**
 * replicate.c
 *
 * Replicate Predictions API provider.
 *
 * Replicate uses a two-step flow:
 *   1. POST to create a prediction (returns an ID + status URL).
 *   2. Poll the status URL until the prediction completes.
 *
 * Environment:
 *   REPLICATE_API_TOKEN must be set before calling replicate_generate().
 *
 * Note: The model field should use the format "owner/name:version" or
 *       "owner/name" for latest version.
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

LLMResponse replicate_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_token = getenv("REPLICATE_API_TOKEN");
    if (!api_token) {
        res.error = "Missing REPLICATE_API_TOKEN environment variable";
        return res;
    }

    char *esc_model  = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);
    if (!esc_model || !esc_prompt) {
        free(esc_model); free(esc_prompt);
        res.error = "Memory allocation failed";
        return res;
    }

    /* Build input with optional system prompt */
    char *esc_system = NULL;
    if (req->system) {
        esc_system = json_escape(req->system);
        if (!esc_system) { free(esc_model); free(esc_prompt); res.error = "Memory allocation failed"; return res; }
    }

    size_t input_len = strlen(esc_prompt) + (esc_system ? strlen(esc_system) : 0) + 128;
    char *input = malloc(input_len);
    if (!input) { free(esc_model); free(esc_prompt); free(esc_system); res.error = "Memory allocation failed"; return res; }

    if (esc_system) {
        snprintf(input, input_len,
            "\"input\":{\"system_prompt\":%s,\"prompt\":%s,\"max_tokens\":%d}",
            esc_system, esc_prompt, req->max_tokens);
    } else {
        snprintf(input, input_len,
            "\"input\":{\"prompt\":%s,\"max_tokens\":%d}",
            esc_prompt, req->max_tokens);
    }

    free(esc_prompt); free(esc_system);

    size_t body_len = strlen(esc_model) + strlen(input) + 64;
    char *body = malloc(body_len);
    if (!body) { free(esc_model); free(input); res.error = "Memory allocation failed"; return res; }

    snprintf(body, body_len, "{\"version\":%s,%s}", esc_model, input);
    free(esc_model); free(input);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_token);
    headers = curl_slist_append(headers, auth);

    char *response = http_post(
        "https://api.replicate.com/v1/predictions", headers, body);

    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_replicate_response(response);
    res.success = 1;
    return res;
}

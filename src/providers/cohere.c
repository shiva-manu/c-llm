/**
 * cohere.c
 *
 * Cohere Chat API provider.
 *
 * Environment:
 *   COHERE_API_KEY must be set before calling cohere_generate().
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMResponse cohere_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_key = getenv("COHERE_API_KEY");
    if (!api_key) {
        res.error = "Missing COHERE_API_KEY environment variable";
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

    size_t body_len = strlen(esc_model) + strlen(esc_prompt) +
                      (esc_system ? strlen(esc_system) : 0) + 512;
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model); free(esc_prompt); free(esc_system);
        res.error = "Memory allocation failed";
        return res;
    }

    /* Cohere uses "preamble" for system prompt */
    if (esc_system) {
        snprintf(body, body_len,
            "{\"model\":%s,\"messages\":[{\"role\":\"user\",\"content\":%s}],"
            "\"preamble\":%s,\"max_tokens\":%d,\"temperature\":%.2f}",
            esc_model, esc_prompt, esc_system, req->max_tokens, req->temperature);
    } else {
        snprintf(body, body_len,
            "{\"model\":%s,\"messages\":[{\"role\":\"user\",\"content\":%s}],"
            "\"max_tokens\":%d,\"temperature\":%.2f}",
            esc_model, esc_prompt, req->max_tokens, req->temperature);
    }

    free(esc_model); free(esc_prompt); free(esc_system);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    char *response = http_post("https://api.cohere.com/v2/chat", headers, body);
    free(body);
    curl_slist_free_all(headers);

    if (!response) { res.error = "HTTP request failed"; return res; }

    res.text = response;
    res.content = parse_cohere_response(response);
    res.success = 1;
    return res;
}

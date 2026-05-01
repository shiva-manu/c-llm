/**
 * replicate.c - Replicate Predictions API provider.
 * Env: REPLICATE_API_TOKEN
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMResponse replicate_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    const char *api_token = getenv("REPLICATE_API_TOKEN");
    if (!api_token) { res.error = "Missing REPLICATE_API_TOKEN environment variable"; return res; }

    char *esc_model = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);
    char *esc_system = req->system ? json_escape(req->system) : NULL;

    /* Build input JSON */
    size_t input_len = (esc_prompt?strlen(esc_prompt):0) + (esc_system?strlen(esc_system):0) + 128;
    char *input = malloc(input_len);
    if (!input) { free(esc_model); free(esc_prompt); free(esc_system); res.error = "Memory allocation failed"; return res; }

    int pos = snprintf(input, input_len, "\"input\":{");
    if (esc_system) pos += snprintf(input+pos, input_len-pos, "\"system_prompt\":%s,", esc_system);
    pos += snprintf(input+pos, input_len-pos, "\"prompt\":%s,\"max_tokens\":%d}", esc_prompt, req->max_tokens);

    size_t body_len = (esc_model?strlen(esc_model):0) + strlen(input) + 64;
    char *body = malloc(body_len);
    if (!body) { free(esc_model); free(esc_prompt); free(esc_system); free(input); res.error = "Memory allocation failed"; return res; }
    snprintf(body, body_len, "{\"version\":%s,%s}", esc_model, input);

    free(esc_model); free(esc_prompt); free(esc_system); free(input);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_token);
    headers = curl_slist_append(headers, auth);

    int http_status = 0;
    char *response = http_post_retry("https://api.replicate.com/v1/predictions",
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
    res.content = parse_replicate_response(response);
    res.success = 1;
    return res;
}

/**
 * gemini_embed.c - Google Gemini Embedding API.
 * Env: GEMINI_API_KEY
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMEmbedResponse gemini_embed(const LLMEmbedRequest *req) {
    LLMEmbedResponse res = {0};

    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key) { res.error = "Missing GEMINI_API_KEY environment variable"; return res; }

    /* Strip quotes from model name for URL */
    char *esc_model = json_escape(req->model);
    char *esc_input = json_escape(req->input);
    if (!esc_model || !esc_input) {
        free(esc_model); free(esc_input);
        res.error = "Memory allocation failed"; return res;
    }

    char model_buf[256] = {0};
    size_t ml = strlen(esc_model);
    if (ml > 2) {
        size_t cl = ml - 2;
        if (cl >= sizeof(model_buf)) cl = sizeof(model_buf) - 1;
        memcpy(model_buf, esc_model + 1, cl);
    }
    free(esc_model);

    size_t body_len = strlen(esc_input) + 128;
    char *body = malloc(body_len);
    if (!body) { free(esc_input); res.error = "Memory allocation failed"; return res; }

    snprintf(body, body_len, "{\"content\":{\"parts\":[{\"text\":%s}]}}", esc_input);
    free(esc_input);

    char url[1024];
    snprintf(url, sizeof(url),
        "https://generativelanguage.googleapis.com/v1beta/models/%s:embedContent?key=%s",
        model_buf, api_key);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    int http_status = 0;
    char *response = http_post_retry(url, headers, body, 0, &http_status);
    free(body);
    curl_slist_free_all(headers);

    if (!response) {
        static char err[128];
        snprintf(err, sizeof(err), "HTTP request failed (status %d)", http_status);
        res.error = err;
        return res;
    }

    res.text = response;
    res.embedding = parse_gemini_embedding(response, &res.num_dimensions);
    if (!res.embedding) { res.error = "Failed to parse embedding"; free(response); res.text = NULL; return res; }
    res.success = 1;
    return res;
}

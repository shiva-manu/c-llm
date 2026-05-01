/**
 * openai_embed.c - OpenAI Embeddings API.
 * Env: OPENAI_API_KEY
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMEmbedResponse openai_embed(const LLMEmbedRequest *req) {
    LLMEmbedResponse res = {0};

    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) { res.error = "Missing OPENAI_API_KEY environment variable"; return res; }

    char *esc_model = json_escape(req->model);
    char *esc_input = json_escape(req->input);
    if (!esc_model || !esc_input) {
        free(esc_model); free(esc_input);
        res.error = "Memory allocation failed"; return res;
    }

    size_t body_len = strlen(esc_model) + strlen(esc_input) + 128;
    char *body = malloc(body_len);
    if (!body) { free(esc_model); free(esc_input); res.error = "Memory allocation failed"; return res; }

    if (req->dimensions > 0) {
        snprintf(body, body_len, "{\"model\":%s,\"input\":%s,\"dimensions\":%d}",
                 esc_model, esc_input, req->dimensions);
    } else {
        snprintf(body, body_len, "{\"model\":%s,\"input\":%s}", esc_model, esc_input);
    }
    free(esc_model); free(esc_input);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth[512];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    int http_status = 0;
    char *response = http_post_retry("https://api.openai.com/v1/embeddings",
                                     headers, body, 0, &http_status);
    free(body);
    curl_slist_free_all(headers);

    if (!response) {
        static char err[128];
        snprintf(err, sizeof(err), "HTTP request failed (status %d)", http_status);
        res.error = err;
        return res;
    }

    res.text = response;
    res.embedding = parse_openai_embedding(response, &res.num_dimensions);
    if (!res.embedding) { res.error = "Failed to parse embedding"; free(response); res.text = NULL; return res; }
    res.success = 1;
    return res;
}

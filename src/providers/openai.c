/**
 * openai.c
 *
 * Thin wrapper around the OpenAI Chat Completions API.
 * Uses http_post() from http_post.c to perform the actual HTTP call.
 *
 * Compile:
 *   gcc openai.c http_post.c -lcurl -o openai
 *
 * Environment:
 *   OPENAI_API_KEY must be set before calling openai_generate().
 */

#include "llm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

/* Defined in http_post.c */
extern char *http_post(const char *url, struct curl_slist *headers, const char *body);

/**
 * json_escape - escape a string for safe inclusion inside a JSON value.
 *
 * Wraps the string in double quotes and escapes special characters
 * (" , \ , and control characters) so the resulting string is valid JSON.
 *
 * @src: the raw, null-terminated input string.
 *
 * Return:
 *   A heap-allocated, null-terminated, JSON-safe string including the
 *   surrounding double quotes (e.g. "hello \"world\"").
 *   Returns NULL on allocation failure.
 *   The caller must free() the result.
 */
static char *json_escape(const char *src) {
    /* Worst case: every character is a 2-char escape, plus quotes and terminator */
    size_t max_len = strlen(src) * 2 + 3;
    char *out = malloc(max_len);
    if (!out) return NULL;

    char *dst = out;
    *dst++ = '"';  /* opening quote */

    for (const char *p = src; *p; p++) {
        switch (*p) {
            case '"':  *dst++ = '\\'; *dst++ = '"';  break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n';  break;
            case '\r': *dst++ = '\\'; *dst++ = 'r';  break;
            case '\t': *dst++ = '\\'; *dst++ = 't';  break;
            default:
                /* Escape other control characters */
                if ((unsigned char)*p < 0x20) {
                    dst += sprintf(dst, "\\u%04x", (unsigned char)*p);
                } else {
                    *dst++ = *p;
                }
                break;
        }
    }

    *dst++ = '"';  /* closing quote */
    *dst   = '\0';
    return out;
}

/**
 * openai_generate - send a prompt to the OpenAI Chat Completions API.
 *
 * @req: pointer to an LLMRequest containing:
 *         - model:  model name (e.g. "gpt-4", "gpt-3.5-turbo")
 *         - prompt: the user message text
 *
 * Return:
 *   An LLMResponse where:
 *     - .success == 1 and .text contains the raw JSON response on success.
 *     - .success == 0 and .error contains a human-readable message on failure.
 *
 * Notes:
 *   - The caller is responsible for freeing res.text on success.
 *   - The raw JSON response has NOT been parsed; you may want to extract
 *     choices[0].message.content with a JSON parser (e.g. cJSON, jansson).
 */
LLMResponse openai_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    /* ------------------------------------------------------------------ */
    /* 1. Retrieve the API key from the environment                        */
    /* ------------------------------------------------------------------ */
    const char *api_key = getenv("OPENAI_API_KEY");
    if (!api_key) {
        res.success = 0;
        res.error   = "Missing OPENAI_API_KEY environment variable";
        return res;
    }

    /* ------------------------------------------------------------------ */
    /* 2. Escape the model and prompt for safe JSON embedding               */
    /* ------------------------------------------------------------------ */
    char *esc_model  = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);

    if (!esc_model || !esc_prompt) {
        free(esc_model);
        free(esc_prompt);
        res.success = 0;
        res.error   = "Memory allocation failed";
        return res;
    }

    /* ------------------------------------------------------------------ */
    /* 3. Build the JSON body dynamically to avoid buffer overflow          */
    /* ------------------------------------------------------------------ */
    /**
     * Template: {"model":"<esc_model>","messages":[{"role":"user","content":"<esc_prompt>"}]}
     * We add 128 bytes of slack for the fixed JSON scaffolding.
     */
    size_t body_len = strlen(esc_model) + strlen(esc_prompt) + 128;
    char *body = malloc(body_len);
    if (!body) {
        free(esc_model);
        free(esc_prompt);
        res.success = 0;
        res.error   = "Memory allocation failed";
        return res;
    }

    snprintf(body, body_len,
        "{ \"model\": %s, \"messages\": [{\"role\":\"user\",\"content\":%s}] }",
        esc_model, esc_prompt);

    /* Escaped strings are no longer needed after building the body */
    free(esc_model);
    free(esc_prompt);

    /* ------------------------------------------------------------------ */
    /* 4. Set up HTTP headers                                              */
    /* ------------------------------------------------------------------ */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!headers) {
        free(body);
        res.success = 0;
        res.error   = "Failed to create header list";
        return res;
    }

    char auth[256];
    snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth);

    /* ------------------------------------------------------------------ */
    /* 5. Send the POST request                                            */
    /* ------------------------------------------------------------------ */
    char *response = http_post(
        "https://api.openai.com/v1/chat/completions",
        headers,
        body
    );

    /* Free resources we no longer need */
    free(body);
    curl_slist_free_all(headers);

    /* ------------------------------------------------------------------ */
    /* 6. Check the result                                                 */
    /* ------------------------------------------------------------------ */
    if (!response) {
        res.success = 0;
        res.error   = "HTTP request failed";
        return res;
    }

    /**
     * At this point, response contains raw JSON like:
     *   {"choices":[{"message":{"content":"Hello!"}}], ...}
     *
     * For a production implementation you would parse this with
     * cJSON / jansson / parson and extract choices[0].message.content.
     * For now we return the full JSON and let the caller decide.
     */
    res.text    = response;
    res.success = 1;
    return res;
}

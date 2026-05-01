/**
 * parse.c
 *
 * Response JSON parsing using cJSON.
 * Extracts text content from each provider's response format.
 */

#define _POSIX_C_SOURCE 200809L
#include "parse.h"
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>

/**
 * Helper: duplicate a cJSON string value into a heap-allocated C string.
 * Returns NULL if the item is not a string or allocation fails.
 */
static char *dup_string(const cJSON *item) {
    if (!cJSON_IsString(item) || !item->valuestring) return NULL;
    char *out = strdup(item->valuestring);
    return out;
}

char *parse_openai_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices)) { cJSON_Delete(root); return NULL; }

    cJSON *first = cJSON_GetArrayItem(choices, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON *message = cJSON_GetObjectItem(first, "message");
    if (!message) { cJSON_Delete(root); return NULL; }

    cJSON *content = cJSON_GetObjectItem(message, "content");
    char *result = dup_string(content);
    cJSON_Delete(root);
    return result;
}

char *parse_claude_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *content_arr = cJSON_GetObjectItem(root, "content");
    if (!cJSON_IsArray(content_arr)) { cJSON_Delete(root); return NULL; }

    cJSON *first = cJSON_GetArrayItem(content_arr, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON *text = cJSON_GetObjectItem(first, "text");
    char *result = dup_string(text);
    cJSON_Delete(root);
    return result;
}

char *parse_gemini_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
    if (!cJSON_IsArray(candidates)) { cJSON_Delete(root); return NULL; }

    cJSON *first = cJSON_GetArrayItem(candidates, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON *content = cJSON_GetObjectItem(first, "content");
    if (!content) { cJSON_Delete(root); return NULL; }

    cJSON *parts = cJSON_GetObjectItem(content, "parts");
    if (!cJSON_IsArray(parts)) { cJSON_Delete(root); return NULL; }

    cJSON *part0 = cJSON_GetArrayItem(parts, 0);
    if (!part0) { cJSON_Delete(root); return NULL; }

    cJSON *text = cJSON_GetObjectItem(part0, "text");
    char *result = dup_string(text);
    cJSON_Delete(root);
    return result;
}

char *parse_cohere_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *message = cJSON_GetObjectItem(root, "message");
    if (!message) { cJSON_Delete(root); return NULL; }

    cJSON *content_arr = cJSON_GetObjectItem(message, "content");
    if (!cJSON_IsArray(content_arr)) { cJSON_Delete(root); return NULL; }

    cJSON *first = cJSON_GetArrayItem(content_arr, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON *text = cJSON_GetObjectItem(first, "text");
    char *result = dup_string(text);
    cJSON_Delete(root);
    return result;
}

char *parse_replicate_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *output = cJSON_GetObjectItem(root, "output");
    if (!cJSON_IsArray(output)) { cJSON_Delete(root); return NULL; }

    /* Join all output strings with newlines */
    size_t total_len = 0;
    int count = cJSON_GetArraySize(output);
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(output, i);
        if (cJSON_IsString(item) && item->valuestring) {
            total_len += strlen(item->valuestring) + 1; /* +1 for newline */
        }
    }

    if (total_len == 0) { cJSON_Delete(root); return NULL; }

    char *result = malloc(total_len + 1);
    if (!result) { cJSON_Delete(root); return NULL; }

    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(output, i);
        if (cJSON_IsString(item) && item->valuestring) {
            if (result[0] != '\0') strcat(result, "\n");
            strcat(result, item->valuestring);
        }
    }

    cJSON_Delete(root);
    return result;
}

char *parse_huggingface_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    /* HF returns either an array or an object */
    if (cJSON_IsArray(root)) {
        cJSON *first = cJSON_GetArrayItem(root, 0);
        if (!first) { cJSON_Delete(root); return NULL; }

        cJSON *text = cJSON_GetObjectItem(first, "generated_text");
        char *result = dup_string(text);
        cJSON_Delete(root);
        return result;
    }

    /* Some models return {"generated_text":"..."} directly */
    cJSON *text = cJSON_GetObjectItem(root, "generated_text");
    char *result = dup_string(text);
    cJSON_Delete(root);
    return result;
}

/* ---- Streaming parsers ---- */

char *parse_stream_openai(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices)) { cJSON_Delete(root); return NULL; }

    cJSON *first = cJSON_GetArrayItem(choices, 0);
    if (!first) { cJSON_Delete(root); return NULL; }

    cJSON *delta = cJSON_GetObjectItem(first, "delta");
    if (!delta) { cJSON_Delete(root); return NULL; }

    cJSON *content = cJSON_GetObjectItem(delta, "content");
    char *result = dup_string(content);
    cJSON_Delete(root);
    return result;
}

char *parse_stream_claude(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "content_block_delta") != 0) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *delta = cJSON_GetObjectItem(root, "delta");
    if (!delta) { cJSON_Delete(root); return NULL; }

    cJSON *text = cJSON_GetObjectItem(delta, "text");
    char *result = dup_string(text);
    cJSON_Delete(root);
    return result;
}

char *parse_stream_gemini(const char *json) {
    /* Gemini streaming uses the same response format */
    return parse_gemini_response(json);
}

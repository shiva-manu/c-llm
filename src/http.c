/**
 * http.c
 *
 * HTTP POST helpers with timeout, retry, and streaming support.
 */

#define _GNU_SOURCE
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================================================================
 * Response buffer
 * ======================================================================== */

struct Memory {
    char *response;
    size_t size;
};

static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    char *ptr = realloc(mem->response, mem->size + total + 1);
    if (!ptr) return 0;

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, total);
    mem->size += total;
    mem->response[mem->size] = '\0';
    return total;
}

/* ========================================================================
 * Retry-After header parsing
 * ======================================================================== */

struct HeaderCtx {
    int retry_after;  /* seconds, -1 if not present */
};

static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    size_t total = size * nitems;
    struct HeaderCtx *ctx = (struct HeaderCtx *)userdata;

    if (total > 13 && strncasecmp(buffer, "Retry-After:", 12) == 0) {
        const char *val = buffer + 12;
        while (*val == ' ') val++;
        ctx->retry_after = atoi(val);
        if (ctx->retry_after < 0) ctx->retry_after = 0;
    }
    return total;
}

/* ========================================================================
 * Sleep helper (cross-platform)
 * ======================================================================== */

static void sleep_ms(long ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* ========================================================================
 * http_post - single POST request with timeout
 * ======================================================================== */

char *http_post(const char *url, struct curl_slist *headers,
                const char *body, long timeout_ms)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    struct Memory chunk = {malloc(1), 0};
    if (!chunk.response) {
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

    if (timeout_ms > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.response);
        chunk.response = NULL;
    }

    curl_easy_cleanup(curl);
    return chunk.response;
}

/* ========================================================================
 * http_post_retry - POST with retry and exponential backoff
 * ======================================================================== */

static int is_retryable_status(long status)
{
    return status == 429 || status == 500 || status == 502 || status == 503;
}

char *http_post_retry(const char *url, struct curl_slist *headers,
                      const char *body, long timeout_ms, int *http_status)
{
    const int max_retries = 3;
    long backoff_ms = 1000; /* start at 1 second */

    if (http_status) *http_status = 0;

    for (int attempt = 0; attempt <= max_retries; attempt++) {
        CURL *curl = curl_easy_init();
        if (!curl) return NULL;

        struct Memory chunk = {malloc(1), 0};
        struct HeaderCtx hctx = {-1};

        if (!chunk.response) {
            curl_easy_cleanup(curl);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hctx);

        if (timeout_ms > 0) {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        }

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.response);
            curl_easy_cleanup(curl);
            if (http_status) *http_status = 0;
            return NULL;
        }

        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if (http_status) *http_status = (int)status;

        if (!is_retryable_status(status)) {
            curl_easy_cleanup(curl);
            return chunk.response; /* success or non-retryable error */
        }

        if (attempt == max_retries) {
            /* Exhausted retries */
            fprintf(stderr, "HTTP %ld: max retries exhausted\n", status);
            free(chunk.response);
            curl_easy_cleanup(curl);
            return NULL;
        }

        /* Retryable: sleep then retry */
        long wait_ms = (hctx.retry_after > 0) ? hctx.retry_after * 1000L : backoff_ms;
        fprintf(stderr, "HTTP %ld, retrying in %ldms (attempt %d/%d)\n",
                status, wait_ms, attempt + 1, max_retries);
        free(chunk.response);
        curl_easy_cleanup(curl);

        sleep_ms(wait_ms);
        backoff_ms *= 2; /* exponential backoff */
    }

    return NULL; /* unreachable */
}

/* ========================================================================
 * Streaming
 * ======================================================================== */

struct StreamCtx {
    http_stream_cb cb;
    void *user_data;
};

static size_t stream_write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct StreamCtx *ctx = (struct StreamCtx *)userp;
    if (ctx->cb((const char *)data, total, ctx->user_data) != 0) {
        return 0;
    }
    return total;
}

int http_post_stream(const char *url, struct curl_slist *headers,
                     const char *body, long timeout_ms,
                     http_stream_cb cb, void *user_data)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct StreamCtx ctx = { cb, user_data };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    if (timeout_ms > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_cleanup(curl);
    return 0;
}

/* ========================================================================
 * json_escape
 * ======================================================================== */

char *json_escape(const char *src) {
    size_t max_len = strlen(src) * 2 + 3;
    char *out = malloc(max_len);
    if (!out) return NULL;

    char *dst = out;
    *dst++ = '"';

    for (const char *p = src; *p; p++) {
        switch (*p) {
            case '"':  *dst++ = '\\'; *dst++ = '"';  break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n';  break;
            case '\r': *dst++ = '\\'; *dst++ = 'r';  break;
            case '\t': *dst++ = '\\'; *dst++ = 't';  break;
            default:
                if ((unsigned char)*p < 0x20) {
                    dst += sprintf(dst, "\\u%04x", (unsigned char)*p);
                } else {
                    *dst++ = *p;
                }
                break;
        }
    }

    *dst++ = '"';
    *dst   = '\0';
    return out;
}

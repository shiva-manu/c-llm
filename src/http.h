/**
 * http.h
 *
 * Internal header for HTTP and JSON utilities shared across providers.
 */

#ifndef HTTP_H
#define HTTP_H

#include <curl/curl.h>

/**
 * Stream chunk callback for SSE processing.
 * Return 0 to continue, non-zero to abort.
 */
typedef int (*http_stream_cb)(const char *data, size_t size, void *user_data);

/**
 * http_post - send an HTTP POST request.
 *
 * @url:        full URL.
 * @headers:    custom headers (caller retains ownership).
 * @body:       POST body.
 * @timeout_ms: request timeout (0 = no timeout).
 *
 * Return: heap-allocated response body (caller frees), or NULL on failure.
 */
char *http_post(const char *url, struct curl_slist *headers,
                const char *body, long timeout_ms);

/**
 * http_post_retry - http_post with retry and exponential backoff.
 *
 * Retries on HTTP 429, 500, 502, 503. Max 3 attempts.
 * Respects Retry-After header. Returns HTTP status via out-param.
 *
 * @url:          full URL.
 * @headers:      custom headers.
 * @body:         POST body.
 * @timeout_ms:   per-request timeout (0 = no timeout).
 * @http_status:  out-param for HTTP status code (0 on connection error).
 *
 * Return: heap-allocated response body (caller frees), or NULL on failure.
 */
char *http_post_retry(const char *url, struct curl_slist *headers,
                      const char *body, long timeout_ms, int *http_status);

/**
 * http_post_stream - send a streaming HTTP POST (SSE).
 *
 * @url:        full URL.
 * @headers:    custom headers.
 * @body:       POST body.
 * @timeout_ms: per-request timeout (0 = no timeout).
 * @cb:         callback for each data chunk.
 * @user_data:  opaque pointer passed to cb.
 *
 * Return: 0 on success, -1 on failure.
 */
int http_post_stream(const char *url, struct curl_slist *headers,
                     const char *body, long timeout_ms,
                     http_stream_cb cb, void *user_data);

/**
 * json_escape - escape a string for JSON inclusion (with surrounding quotes).
 * Caller must free(). Returns NULL on allocation failure.
 */
char *json_escape(const char *src);

#endif /* HTTP_H */

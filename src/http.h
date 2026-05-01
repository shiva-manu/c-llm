/**
 * http.h
 *
 * Internal header for HTTP and JSON utilities shared across providers.
 * Not part of the public API.
 */

#ifndef HTTP_H
#define HTTP_H

#include <curl/curl.h>

/**
 * Stream chunk callback - called for each chunk of data received.
 *
 * @data:      pointer to the chunk (not null-terminated).
 * @size:      size of the chunk in bytes.
 * @user_data: opaque pointer passed through from http_post_stream.
 *
 * Return: 0 to continue, non-zero to abort the transfer.
 */
typedef int (*http_stream_cb)(const char *data, size_t size, void *user_data);

/**
 * http_post - send an HTTP POST request and return the response body.
 *
 * @url:     the full URL to POST to.
 * @headers: linked list of custom headers (caller retains ownership).
 * @body:    null-terminated POST body (e.g. JSON).
 *
 * Return: heap-allocated response body (caller must free), or NULL on failure.
 */
char *http_post(const char *url, struct curl_slist *headers, const char *body);

/**
 * http_post_stream - send a streaming HTTP POST request.
 *
 * Calls the callback for each chunk of data received (for SSE processing).
 *
 * @url:      the full URL to POST to.
 * @headers:  linked list of custom headers.
 * @body:     null-terminated POST body.
 * @cb:       callback invoked for each data chunk.
 * @user_data: opaque pointer passed to the callback.
 *
 * Return: 0 on success, non-zero on failure.
 */
int http_post_stream(const char *url, struct curl_slist *headers,
                     const char *body, http_stream_cb cb, void *user_data);

/**
 * json_escape - escape a string for safe inclusion inside a JSON value.
 *
 * Wraps the string in double quotes and escapes special characters.
 *
 * @src: the raw, null-terminated input string.
 *
 * Return: heap-allocated JSON-safe string including surrounding quotes.
 *         Caller must free(). Returns NULL on allocation failure.
 */
char *json_escape(const char *src);

#endif /* HTTP_H */

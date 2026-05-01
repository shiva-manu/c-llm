/**
 * http.c
 *
 * Minimal HTTP POST helper and JSON utilities built on top of libcurl.
 */

#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Struct Memory -dynamically growing buffer for libcurl callbacks.
 *
 * @response: heap-allocated buffer that holds the accumulated response data.
 * @size: current number of bytes stored in @response (excluding null terminator).
 */
struct Memory
{
    char *response;
    size_t size;
};

/**
 * write_callback - libcurl write callback that appends received data to a buffer.
 *
 * This function is registered via CURLOPT_WRITEFUNCTION. libcurl calls it
 * once for every chunk of data it received from the server. Each call
 * grows the buffer in struct Memory and appends the new chunk.
 *
 * @data: pointer to the delivered data (not null-terminated).
 * @size: size of each element (always 1 for byte streams).
 * @nmemb: number of elements delivered.
 * @userp: pointer to user-supvided data(our struct Memory).
 *
 * Return:
 *    The number of bytes consumed (size * nmemb) on success.
 *    Returning anything elseee tells libcurl to abort the transfer.
 */

static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    // Calculate total bytes in this chunk
    size_t total = size * nmemb;

    // Cast the opaque user pointer back to our buffer struct
    struct Memory *mem = (struct Memory *)userp;

    /**
     * Grow the buffer to fit:
     *   - existing data (mem->size)
     *   - new chunk     (total)
     *   - null terminator (+1)
     */
    char *ptr = realloc(mem->response, mem->size + total + 1);
    if (!ptr)
    {
        // Out of memory: return 0 to signal an error to libcurl
        return 0;
    }

    mem->response = ptr;

    // Append the new chunk right after the existing data
    memcpy(&(mem->response[mem->size]), data, total);

    // Update the tracked size
    mem->size += total;

    // Null-terminate so the buffer is always a valid C string
    mem->response[mem->size] = '\0';

    // Tell libcurl we consumed all bytes
    return total;
}

/**
 * http_post -send an HTTP POST request and return the response body.
 *
 * @url: the full url to send the post request to.
 * @headers: a linked list of custom headers (or NULL for none).
 *           The caller retains ownership; this function does not free it.
 * @body: the POST body (e.g JSON). Must be a null-terminated string.
 *
 * Return:
 *   A heap-allocated, null-terminated string containning the full response
 *   body on success. The caller must free() it when done.
 *   Returns Null on any  failure (init error,out of memory,request failure).
 */

char *http_post(const char *url, struct curl_slist *headers, const char *body)
{
    // Initialize a libcurl easy handle
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        return NULL; // Failed to initialize libcurl
    }
    /**
     * Allocate a 1-byte seed buffer so that realloc() in the callback
     * never receives NULL as its first argument.
     * size starts at 0 because no data has been received yet.
     */
    struct Memory chunk = {malloc(1), 0};
    if (!chunk.response)
    {
        curl_easy_cleanup(curl);
        return NULL;
    }
    /*---- Configure the request--------------------------------------*/
    /*Target URL*/
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Custom HTTP headers (Content-Type, Authorization, etc)
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // POST body - libcurl will use POST automatically when this is set
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);

    // Register our callback so libcurl delivers response data to us
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    // Pass our Memory struct as the fourth argument to write_callback
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

    /*---- Execute the request------------------------------------------ */
    // Check for errors during the request
    CURLcode res = curl_easy_perform(curl);

    /*Always clean up the handle regardless of success or failure*/
    if (res != CURLE_OK)
    {
        // print the error to stderr
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        // Clean up any memory that might have been allocated before failure
        if (chunk.response)
            free(chunk.response);
        chunk.response = NULL;
    }
    curl_easy_cleanup(curl);
    // The Caller of this function Must free() the returned pointer!
    return chunk.response;
}

/**
 * Streaming write callback - passes each chunk to the user callback.
 */
struct StreamCtx {
    http_stream_cb cb;
    void *user_data;
};

static size_t stream_write_callback(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    struct StreamCtx *ctx = (struct StreamCtx *)userp;
    if (ctx->cb((const char *)data, total, ctx->user_data) != 0) {
        return 0; /* abort transfer */
    }
    return total;
}

int http_post_stream(const char *url, struct curl_slist *headers,
                     const char *body, http_stream_cb cb, void *user_data)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct StreamCtx ctx = { cb, user_data };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_cleanup(curl);
    return 0;
}

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
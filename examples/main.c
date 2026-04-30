/**
 * main.c
 *
 * Entry point demonstrating the LLM client library.
 * Sends a prompt to a chosen backend (e.g. "openai") and prints the result.
 *
 * Compile:
 *   gcc -std=c99 -Wall -Wextra -Iinclude -o llm_demo \
 *     examples/main.c src/core.c src/http.c src/providers/openai.c src/providers/claude.c -lcurl
 *
 * Run:
 *   OPENAI_API_KEY="sk-..." ./llm_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "llm.h"

int main(void)
{
    /* ------------------------------------------------------------------ */
    /* 1. Initialise libcurl globally (required before any curl calls)      */
    /* ------------------------------------------------------------------ */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* ------------------------------------------------------------------ */
    /* 2. Build the request                                                */
    /* ------------------------------------------------------------------ */
    LLMRequest req = {
        .model      = "gpt-4o-mini",
        .prompt     = "Hello from C!",
        .max_tokens = 100
    };

    /* ------------------------------------------------------------------ */
    /* 3. Send the request                                                 */
    /* ------------------------------------------------------------------ */
    LLMResponse res = llm_generate("openai", &req);

    /* ------------------------------------------------------------------ */
    /* 4. Handle the response                                              */
    /* ------------------------------------------------------------------ */
    if (res.success) {
        printf("Response:\n%s\n", res.text);
        free(res.text); /* res.text is heap-allocated */
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
        /* res.error is a string literal — do NOT free it */
    }

    /* ------------------------------------------------------------------ */
    /* 5. Global cleanup                                                   */
    /* ------------------------------------------------------------------ */
    curl_global_cleanup();

    return res.success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/**
 * main.c
 *
 * Entry point demonstrating the LLM client library.
 * Shows non-streaming and streaming usage with multiple providers.
 *
 * Compile (CMake):
 *   cmake -S . -B build && cmake --build build
 *
 * Compile (single command):
 *   gcc -std=c99 -Wall -Wextra -Iinclude -o llm_demo examples/main.c src/core.c src/http.c src/parse.c src/providers/openai.c src/providers/claude.c src/providers/groq.c src/providers/together.c src/providers/perplexity.c src/providers/deepseek.c src/providers/fireworks.c src/providers/xai.c src/providers/cerebras.c src/providers/sambanova.c src/providers/openrouter.c src/providers/ollama.c src/providers/gemini.c src/providers/cohere.c src/providers/mistral.c src/providers/replicate.c src/providers/huggingface.c src/providers/openai_compat.c -lcurl -lcjson
 *
 * Run:
 *   OPENAI_API_KEY="sk-..." ./build/example
 */

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "llm.h"

/**
 * on_token - streaming callback that prints each token as it arrives.
 */
static void on_token(const char *token, void *user_data) {
    (void)user_data;
    printf("%s", token);
    fflush(stdout);
}

/**
 * demo_basic - non-streaming request with system prompt and parsed response.
 */
static void demo_basic(const char *name, const char *model)
{
    printf("=== %s (%s) ===\n", name, model);

    LLMRequest req = {
        .model      = model,
        .prompt     = "Explain what a hash table is in 2 sentences.",
        .system     = "You are a concise computer science tutor.",
        .max_tokens = 200,
        .temperature = 0.7f
    };

    LLMResponse res = llm_generate(name, &req);

    if (res.success) {
        printf("Content: %s\n", res.content ? res.content : "(no parsed content)");
        free(res.content);
        free(res.text);  /* raw JSON */
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/**
 * demo_stream - streaming request that prints tokens as they arrive.
 */
static void demo_stream(const char *name, const char *model)
{
    printf("=== %s (%s) [streaming] ===\n", name, model);

    LLMRequest req = {
        .model          = model,
        .prompt         = "Write a haiku about programming.",
        .system         = "You are a creative poet.",
        .max_tokens     = 100,
        .temperature    = 0.9f,
        .stream         = 1,
        .stream_cb      = on_token,
        .stream_user_data = NULL
    };

    LLMResponse res = llm_generate_stream(name, &req);

    if (res.success) {
        printf("\n[stream complete]\n");
    } else {
        fprintf(stderr, "\nError: %s\n", res.error);
    }
    printf("\n");
}

int main(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* --- Non-streaming examples --- */

    demo_basic("openai", "gpt-4o-mini");
    /* demo_basic("claude", "claude-3-haiku-20240307"); */
    /* demo_basic("gemini", "gemini-2.0-flash"); */
    /* demo_basic("groq", "llama-3.3-70b-versatile"); */
    /* demo_basic("together", "meta-llama/Llama-3-70b-chat-hf"); */
    /* demo_basic("perplexity", "llama-3.1-sonar-small-128k-online"); */
    /* demo_basic("deepseek", "deepseek-chat"); */
    /* demo_basic("fireworks", "accounts/fireworks/models/llama-v3p1-70b-instruct"); */
    /* demo_basic("xai", "grok-2"); */
    /* demo_basic("cerebras", "llama3.1-8b"); */
    /* demo_basic("sambanova", "Meta-Llama-3.1-8B-Instruct"); */
    /* demo_basic("openrouter", "meta-llama/llama-3.1-8b-instruct"); */
    /* demo_basic("ollama", "llama3.2"); */
    /* demo_basic("cohere", "command-r-plus"); */
    /* demo_basic("mistral", "mistral-large-latest"); */
    /* demo_basic("replicate", "meta/llama-2-70b-chat"); */
    /* demo_basic("huggingface", "meta-llama/Llama-3.1-8B-Instruct"); */

    /* --- Streaming examples --- */

    /* demo_stream("openai", "gpt-4o-mini"); */
    /* demo_stream("claude", "claude-3-haiku-20240307"); */
    /* demo_stream("groq", "llama-3.3-70b-versatile"); */
    /* demo_stream("ollama", "llama3.2"); */

    curl_global_cleanup();
    return EXIT_SUCCESS;
}

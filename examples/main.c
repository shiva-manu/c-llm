/**
 * main.c - AnyLM demo program.
 *
 * Demonstrates all features: basic chat, system prompts, multi-turn,
 * streaming, JSON mode, tools, and embeddings.
 *
 * Compile:
 *   gcc -std=c99 -Wall -Wextra -Iinclude -o llm_demo examples/main.c src/core.c src/http.c src/parse.c src/providers/openai.c src/providers/claude.c src/providers/groq.c src/providers/together.c src/providers/perplexity.c src/providers/deepseek.c src/providers/fireworks.c src/providers/xai.c src/providers/cerebras.c src/providers/sambanova.c src/providers/openrouter.c src/providers/ollama.c src/providers/gemini.c src/providers/cohere.c src/providers/mistral.c src/providers/replicate.c src/providers/huggingface.c src/providers/openai_compat.c src/providers/openai_embed.c src/providers/cohere_embed.c src/providers/gemini_embed.c -lcurl -lcjson
 *
 * Run:
 *   OPENAI_API_KEY="sk-..." ./llm_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "llm.h"

/* ---- Streaming callback ---- */
static void on_token(const char *token, void *ud) {
    (void)ud;
    printf("%s", token);
    fflush(stdout);
}

/* ---- 1. Basic chat with system prompt ---- */
static void demo_basic(void) {
    printf("=== 1. Basic Chat ===\n");
    LLMRequest req = {
        .model      = "gpt-4o-mini",
        .prompt     = "What is a hash table in one sentence?",
        .system     = "You are a concise computer science tutor.",
        .max_tokens = 100,
        .temperature = 0.7f
    };
    LLMResponse res = llm_generate("openai", &req);
    if (res.success) {
        printf("Content: %s\n", res.content ? res.content : "(none)");
        free(res.content); free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 2. Multi-turn conversation ---- */
static void demo_multiturn(void) {
    printf("=== 2. Multi-turn Conversation ===\n");
    LLMMessage msgs[] = {
        {"system",    "You are a helpful cooking assistant.", NULL, 0},
        {"user",      "What's a good pasta sauce?",          NULL, 0},
        {"assistant", "A simple marinara is always great!",   NULL, 0},
        {"user",      "What ingredients do I need?",          NULL, 0},
    };
    LLMRequest req = {
        .model       = "gpt-4o-mini",
        .messages    = msgs,
        .num_messages = 4,
        .max_tokens  = 200,
        .temperature = 0.7f
    };
    LLMResponse res = llm_generate("openai", &req);
    if (res.success) {
        printf("Content: %s\n", res.content ? res.content : "(none)");
        free(res.content); free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 3. Streaming ---- */
static void demo_streaming(void) {
    printf("=== 3. Streaming ===\n");
    LLMRequest req = {
        .model          = "gpt-4o-mini",
        .prompt         = "Write a haiku about C programming.",
        .system         = "You are a creative poet.",
        .max_tokens     = 100,
        .temperature    = 0.9f,
        .stream         = 1,
        .stream_cb      = on_token,
        .stream_user_data = NULL
    };
    LLMResponse res = llm_generate_stream("openai", &req);
    if (res.success) {
        printf("\n[stream complete]\n");
    } else {
        fprintf(stderr, "\nError: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 4. JSON mode ---- */
static void demo_json_mode(void) {
    printf("=== 4. JSON Mode ===\n");
    LLMRequest req = {
        .model      = "gpt-4o-mini",
        .prompt     = "List 3 programming languages with their year of creation.",
        .system     = "Respond with JSON: [{\"name\":\"...\",\"year\":...}]",
        .max_tokens = 200,
        .json_mode  = 1
    };
    LLMResponse res = llm_generate("openai", &req);
    if (res.success) {
        printf("JSON: %s\n", res.content ? res.content : "(none)");
        free(res.content); free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 5. Tool calling ---- */
static void demo_tools(void) {
    printf("=== 5. Tool Calling ===\n");
    LLMTool tools[] = {
        {
            "get_weather",
            "Get the current weather for a location",
            "{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\"}},\"required\":[\"location\"]}"
        }
    };
    LLMRequest req = {
        .model     = "gpt-4o-mini",
        .prompt    = "What's the weather in Tokyo?",
        .max_tokens = 200,
        .tools     = tools,
        .num_tools = 1
    };
    LLMResponse res = llm_generate("openai", &req);
    if (res.success) {
        if (res.tool_calls && res.num_tool_calls > 0) {
            printf("Tool calls:\n");
            for (int i = 0; i < res.num_tool_calls; i++) {
                printf("  [%d] %s(%s)\n", i, res.tool_calls[i].name, res.tool_calls[i].arguments);
                free((char*)res.tool_calls[i].id);
                free((char*)res.tool_calls[i].name);
                free((char*)res.tool_calls[i].arguments);
            }
            free(res.tool_calls);
        } else {
            printf("Content: %s\n", res.content ? res.content : "(none)");
        }
        free(res.content); free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 6. Embeddings ---- */
static void demo_embeddings(void) {
    printf("=== 6. Embeddings ===\n");
    LLMEmbedRequest req = {
        .model = "text-embedding-3-small",
        .input = "The quick brown fox jumps over the lazy dog.",
        .dimensions = 0
    };
    LLMEmbedResponse res = llm_embed("openai", &req);
    if (res.success) {
        printf("Embedding dimensions: %d\n", res.num_dimensions);
        printf("First 5 values: [");
        for (int i = 0; i < 5 && i < res.num_dimensions; i++) {
            if (i > 0) printf(", ");
            printf("%.6f", res.embedding[i]);
        }
        printf("...]\n");
        free(res.embedding); free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 7. Request timeout ---- */
static void demo_timeout(void) {
    printf("=== 7. Request Timeout ===\n");
    LLMRequest req = {
        .model      = "gpt-4o-mini",
        .prompt     = "Hello!",
        .max_tokens = 10,
        .timeout_ms = 5000  /* 5 second timeout */
    };
    LLMResponse res = llm_generate("openai", &req);
    if (res.success) {
        printf("Content: %s\n", res.content ? res.content : "(none)");
        free(res.content); free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }
    printf("\n");
}

/* ---- 8. Provider switching ---- */
static void demo_providers(void) {
    printf("=== 8. Provider Switching ===\n");
    const char *names[] = {"openai", "groq", "claude", "mistral", NULL};
    LLMRequest req = {
        .model      = "gpt-4o-mini",
        .prompt     = "Say hi in 3 words.",
        .max_tokens = 20
    };
    for (int i = 0; names[i]; i++) {
        req.model = "gpt-4o-mini"; /* default */
        if (strcmp(names[i], "groq") == 0) req.model = "llama-3.3-70b-versatile";
        if (strcmp(names[i], "claude") == 0) req.model = "claude-3-haiku-20240307";
        if (strcmp(names[i], "mistral") == 0) req.model = "mistral-tiny";

        LLMResponse res = llm_generate(names[i], &req);
        if (res.success) {
            printf("%-10s: %s\n", names[i], res.content ? res.content : "(none)");
            free(res.content); free(res.text);
        } else {
            printf("%-10s: [skip] %s\n", names[i], res.error);
        }
    }
    printf("\n");
}

int main(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    demo_basic();
    /* demo_multiturn(); */
    /* demo_streaming(); */
    /* demo_json_mode(); */
    /* demo_tools(); */
    /* demo_embeddings(); */
    /* demo_timeout(); */
    /* demo_providers(); */

    curl_global_cleanup();
    return 0;
}

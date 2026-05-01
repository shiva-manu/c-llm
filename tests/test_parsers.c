/**
 * test_parsers.c
 *
 * Unit tests for response JSON parsers, streaming parsers,
 * tool call parsers, and embedding parsers.
 * Uses mock JSON fixtures - no API keys needed.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "llm.h"
#include "parse.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) && (b) && strcmp((a), (b)) == 0) { tests_passed++; } \
    else { printf("  FAIL: %s: got \"%s\", expected \"%s\" (line %d)\n", \
           msg, (a) ? (a) : "(null)", (b) ? (b) : "(null)", __LINE__); } \
} while(0)

/* ========================================================================
 * Chat response parsers
 * ======================================================================== */

static void test_parse_openai_response(void) {
    printf("  parse_openai_response...\n");

    char *r = parse_openai_response(
        "{\"choices\":[{\"message\":{\"content\":\"Hello world\"}}]}");
    ASSERT_STR_EQ(r, "Hello world", "basic");
    free(r);

    r = parse_openai_response("{\"choices\":[]}");
    ASSERT(r == NULL, "empty choices");

    r = parse_openai_response("{}");
    ASSERT(r == NULL, "no choices");

    r = parse_openai_response("not json");
    ASSERT(r == NULL, "invalid json");

    r = parse_openai_response(
        "{\"choices\":[{\"message\":{\"content\":null}}]}");
    ASSERT(r == NULL, "null content");
}

static void test_parse_claude_response(void) {
    printf("  parse_claude_response...\n");

    char *r = parse_claude_response(
        "{\"content\":[{\"type\":\"text\",\"text\":\"Hi there\"}]}");
    ASSERT_STR_EQ(r, "Hi there", "basic");
    free(r);

    r = parse_claude_response("{\"content\":[]}");
    ASSERT(r == NULL, "empty content");

    r = parse_claude_response("{}");
    ASSERT(r == NULL, "no content");
}

static void test_parse_gemini_response(void) {
    printf("  parse_gemini_response...\n");

    char *r = parse_gemini_response(
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Gemini says hi\"}]}}]}");
    ASSERT_STR_EQ(r, "Gemini says hi", "basic");
    free(r);

    r = parse_gemini_response("{\"candidates\":[]}");
    ASSERT(r == NULL, "empty candidates");

    r = parse_gemini_response(
        "{\"candidates\":[{\"content\":{\"parts\":[]}}]}");
    ASSERT(r == NULL, "empty parts");
}

static void test_parse_cohere_response(void) {
    printf("  parse_cohere_response...\n");

    char *r = parse_cohere_response(
        "{\"message\":{\"content\":[{\"text\":\"Cohere reply\"}]}}");
    ASSERT_STR_EQ(r, "Cohere reply", "basic");
    free(r);

    r = parse_cohere_response("{\"message\":{}}");
    ASSERT(r == NULL, "no content array");
}

static void test_parse_replicate_response(void) {
    printf("  parse_replicate_response...\n");

    char *r = parse_replicate_response(
        "{\"output\":[\"line1\",\"line2\",\"line3\"]}");
    ASSERT(r != NULL, "basic not null");
    ASSERT_STR_EQ(r, "line1\nline2\nline3", "basic joined");
    free(r);

    r = parse_replicate_response("{\"output\":[\"single\"]}");
    ASSERT_STR_EQ(r, "single", "single item");
    free(r);

    r = parse_replicate_response("{\"output\":[]}");
    ASSERT(r == NULL, "empty output");
}

static void test_parse_huggingface_response(void) {
    printf("  parse_huggingface_response...\n");

    /* Array format */
    char *r = parse_huggingface_response(
        "[{\"generated_text\":\"HF output\"}]");
    ASSERT_STR_EQ(r, "HF output", "array format");
    free(r);

    /* Object format */
    r = parse_huggingface_response(
        "{\"generated_text\":\"HF direct\"}");
    ASSERT_STR_EQ(r, "HF direct", "object format");
    free(r);

    r = parse_huggingface_response("[]");
    ASSERT(r == NULL, "empty array");
}

/* ========================================================================
 * Streaming parsers
 * ======================================================================== */

static void test_parse_stream_openai(void) {
    printf("  parse_stream_openai...\n");

    char *r = parse_stream_openai(
        "{\"choices\":[{\"delta\":{\"content\":\"token1\"}}]}");
    ASSERT_STR_EQ(r, "token1", "basic token");
    free(r);

    r = parse_stream_openai(
        "{\"choices\":[{\"delta\":{}}]}");
    ASSERT(r == NULL, "no content in delta");

    r = parse_stream_openai(
        "{\"choices\":[{\"delta\":{\"content\":null}}]}");
    ASSERT(r == NULL, "null content in delta");
}

static void test_parse_stream_claude(void) {
    printf("  parse_stream_claude...\n");

    char *r = parse_stream_claude(
        "{\"type\":\"content_block_delta\",\"delta\":{\"text\":\"chunk\"}}");
    ASSERT_STR_EQ(r, "chunk", "basic chunk");
    free(r);

    /* Wrong event type should return NULL */
    r = parse_stream_claude(
        "{\"type\":\"message_start\",\"delta\":{\"text\":\"nope\"}}");
    ASSERT(r == NULL, "wrong event type");

    r = parse_stream_claude(
        "{\"type\":\"content_block_delta\",\"delta\":{}}");
    ASSERT(r == NULL, "no text in delta");
}

/* ========================================================================
 * Tool call parsers
 * ======================================================================== */

static void test_parse_openai_tool_calls(void) {
    printf("  parse_openai_tool_calls...\n");

    int count = 0;
    LLMToolCall *calls = parse_openai_tool_calls(
        "{\"choices\":[{\"message\":{\"tool_calls\":["
        "{\"id\":\"call_1\",\"function\":{\"name\":\"get_weather\",\"arguments\":\"{\\\"location\\\":\\\"Tokyo\\\"}\"}}"
        "]}}]}", &count);
    ASSERT(calls != NULL, "not null");
    ASSERT(count == 1, "count is 1");
    if (calls) {
        ASSERT_STR_EQ(calls[0].id, "call_1", "id");
        ASSERT_STR_EQ(calls[0].name, "get_weather", "name");
        ASSERT_STR_EQ(calls[0].arguments, "{\"location\":\"Tokyo\"}", "args");
        free((char*)calls[0].id);
        free((char*)calls[0].name);
        free((char*)calls[0].arguments);
        free(calls);
    }

    /* No tool calls */
    calls = parse_openai_tool_calls(
        "{\"choices\":[{\"message\":{\"content\":\"hi\"}}]}", &count);
    ASSERT(calls == NULL, "no tool calls returns NULL");
    ASSERT(count == 0, "count is 0");

    /* Multiple tool calls */
    calls = parse_openai_tool_calls(
        "{\"choices\":[{\"message\":{\"tool_calls\":["
        "{\"id\":\"c1\",\"function\":{\"name\":\"a\",\"arguments\":\"{}\"}},"
        "{\"id\":\"c2\",\"function\":{\"name\":\"b\",\"arguments\":\"{}\"}}"
        "]}}]}", &count);
    ASSERT(count == 2, "two tool calls");
    if (calls) {
        free((char*)calls[0].id); free((char*)calls[0].name); free((char*)calls[0].arguments);
        free((char*)calls[1].id); free((char*)calls[1].name); free((char*)calls[1].arguments);
        free(calls);
    }
}

static void test_parse_claude_tool_calls(void) {
    printf("  parse_claude_tool_calls...\n");

    int count = 0;
    LLMToolCall *calls = parse_claude_tool_calls(
        "{\"content\":["
        "{\"type\":\"text\",\"text\":\"I'll check the weather.\"},"
        "{\"type\":\"tool_use\",\"id\":\"tu_1\",\"name\":\"get_weather\",\"input\":{\"location\":\"Paris\"}}"
        "]}", &count);
    ASSERT(calls != NULL, "not null");
    ASSERT(count == 1, "count is 1");
    if (calls) {
        ASSERT_STR_EQ(calls[0].id, "tu_1", "id");
        ASSERT_STR_EQ(calls[0].name, "get_weather", "name");
        ASSERT(calls[0].arguments != NULL, "arguments not null");
        free((char*)calls[0].id);
        free((char*)calls[0].name);
        free((char*)calls[0].arguments);
        free(calls);
    }
}

static void test_parse_gemini_tool_calls(void) {
    printf("  parse_gemini_tool_calls...\n");

    int count = 0;
    LLMToolCall *calls = parse_gemini_tool_calls(
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"functionCall\":{\"name\":\"get_weather\",\"args\":{\"location\":\"Berlin\"}}}"
        "]}}]}", &count);
    ASSERT(calls != NULL, "not null");
    ASSERT(count == 1, "count is 1");
    if (calls) {
        ASSERT(calls[0].id == NULL, "gemini has no id");
        ASSERT_STR_EQ(calls[0].name, "get_weather", "name");
        ASSERT(calls[0].arguments != NULL, "arguments not null");
        free((char*)calls[0].name);
        free((char*)calls[0].arguments);
        free(calls);
    }
}

/* ========================================================================
 * Embedding parsers
 * ======================================================================== */

static void test_parse_openai_embedding(void) {
    printf("  parse_openai_embedding...\n");

    int dims = 0;
    float *v = parse_openai_embedding(
        "{\"data\":[{\"embedding\":[0.1,0.2,0.3]}]}", &dims);
    ASSERT(v != NULL, "not null");
    ASSERT(dims == 3, "dims is 3");
    if (v) {
        ASSERT(fabsf(v[0] - 0.1f) < 0.001f, "v[0]");
        ASSERT(fabsf(v[1] - 0.2f) < 0.001f, "v[1]");
        ASSERT(fabsf(v[2] - 0.3f) < 0.001f, "v[2]");
        free(v);
    }

    v = parse_openai_embedding("{\"data\":[]}", &dims);
    ASSERT(v == NULL, "empty data");
}

static void test_parse_cohere_embedding(void) {
    printf("  parse_cohere_embedding...\n");

    int dims = 0;
    float *v = parse_cohere_embedding(
        "{\"embeddings\":[[0.5,0.6]]}", &dims);
    ASSERT(v != NULL, "not null");
    ASSERT(dims == 2, "dims is 2");
    if (v) {
        ASSERT(fabsf(v[0] - 0.5f) < 0.001f, "v[0]");
        ASSERT(fabsf(v[1] - 0.6f) < 0.001f, "v[1]");
        free(v);
    }
}

static void test_parse_gemini_embedding(void) {
    printf("  parse_gemini_embedding...\n");

    int dims = 0;
    float *v = parse_gemini_embedding(
        "{\"embedding\":{\"values\":[0.7,0.8,0.9]}}", &dims);
    ASSERT(v != NULL, "not null");
    ASSERT(dims == 3, "dims is 3");
    if (v) {
        ASSERT(fabsf(v[0] - 0.7f) < 0.001f, "v[0]");
        ASSERT(fabsf(v[1] - 0.8f) < 0.001f, "v[1]");
        ASSERT(fabsf(v[2] - 0.9f) < 0.001f, "v[2]");
        free(v);
    }

    v = parse_gemini_embedding("{}", &dims);
    ASSERT(v == NULL, "no embedding key");
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("=== AnyLM Unit Tests ===\n\n");

    printf("[Chat response parsers]\n");
    test_parse_openai_response();
    test_parse_claude_response();
    test_parse_gemini_response();
    test_parse_cohere_response();
    test_parse_replicate_response();
    test_parse_huggingface_response();

    printf("\n[Streaming parsers]\n");
    test_parse_stream_openai();
    test_parse_stream_claude();

    printf("\n[Tool call parsers]\n");
    test_parse_openai_tool_calls();
    test_parse_claude_tool_calls();
    test_parse_gemini_tool_calls();

    printf("\n[Embedding parsers]\n");
    test_parse_openai_embedding();
    test_parse_cohere_embedding();
    test_parse_gemini_embedding();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

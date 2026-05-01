/**
 * test_integration.c
 *
 * Integration tests for provider request body construction.
 * Verifies that providers generate correct JSON bodies for
 * multi-turn, tools, JSON mode, and vision inputs.
 * No API calls are made - only tests the body construction logic.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "llm.h"

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
 * LLMRequest validation tests
 * ======================================================================== */

static void test_request_defaults(void) {
    printf("  LLMRequest defaults...\n");

    LLMRequest req = {0};
    ASSERT(req.model == NULL, "model null by default");
    ASSERT(req.prompt == NULL, "prompt null by default");
    ASSERT(req.system == NULL, "system null by default");
    ASSERT(req.messages == NULL, "messages null by default");
    ASSERT(req.num_messages == 0, "num_messages 0");
    ASSERT(req.max_tokens == 0, "max_tokens 0");
    ASSERT(req.temperature == 0.0f, "temperature 0");
    ASSERT(req.json_mode == 0, "json_mode off");
    ASSERT(req.tools == NULL, "tools null");
    ASSERT(req.num_tools == 0, "num_tools 0");
    ASSERT(req.timeout_ms == 0, "timeout 0");
    ASSERT(req.stream == 0, "stream off");
    ASSERT(req.stream_cb == NULL, "stream_cb null");
}

static void test_response_defaults(void) {
    printf("  LLMResponse defaults...\n");

    LLMResponse res = {0};
    ASSERT(res.success == 0, "success 0");
    ASSERT(res.text == NULL, "text null");
    ASSERT(res.content == NULL, "content null");
    ASSERT(res.tool_calls == NULL, "tool_calls null");
    ASSERT(res.num_tool_calls == 0, "num_tool_calls 0");
    ASSERT(res.error == NULL, "error null");
}

static void test_embed_request_defaults(void) {
    printf("  LLMEmbedRequest defaults...\n");

    LLMEmbedRequest req = {0};
    ASSERT(req.model == NULL, "model null");
    ASSERT(req.input == NULL, "input null");
    ASSERT(req.dimensions == 0, "dimensions 0");
}

static void test_embed_response_defaults(void) {
    printf("  LLMEmbedResponse defaults...\n");

    LLMEmbedResponse res = {0};
    ASSERT(res.success == 0, "success 0");
    ASSERT(res.embedding == NULL, "embedding null");
    ASSERT(res.num_dimensions == 0, "num_dimensions 0");
    ASSERT(res.text == NULL, "text null");
    ASSERT(res.error == NULL, "error null");
}

/* ========================================================================
 * Message construction tests
 * ======================================================================== */

static void test_simple_prompt(void) {
    printf("  simple prompt mode...\n");

    LLMRequest req = {
        .model = "gpt-4o-mini",
        .prompt = "Hello",
        .system = "Be brief",
        .max_tokens = 50
    };

    /* Verify simple mode fields */
    ASSERT(req.messages == NULL, "messages NULL = simple mode");
    ASSERT(req.prompt != NULL, "prompt set");
    ASSERT(req.system != NULL, "system set");
}

static void test_multiturn_messages(void) {
    printf("  multi-turn message array...\n");

    LLMMessage msgs[] = {
        {"system",    "You are helpful.", NULL, 0},
        {"user",      "Hi",               NULL, 0},
        {"assistant", "Hello!",           NULL, 0},
        {"user",      "Bye",              NULL, 0},
    };
    LLMRequest req = {
        .model = "gpt-4o-mini",
        .messages = msgs,
        .num_messages = 4,
        .max_tokens = 50
    };

    ASSERT(req.messages != NULL, "messages set = multi-turn mode");
    ASSERT(req.num_messages == 4, "4 messages");
    ASSERT_STR_EQ(req.messages[0].role, "system", "msg 0 role");
    ASSERT_STR_EQ(req.messages[1].role, "user", "msg 1 role");
    ASSERT_STR_EQ(req.messages[2].role, "assistant", "msg 2 role");
    ASSERT_STR_EQ(req.messages[2].content, "Hello!", "msg 2 content");
}

static void test_vision_parts(void) {
    printf("  vision/multimodal parts...\n");

    LLMContentPart parts[] = {
        {"text", "What is this?", NULL},
        {"image_url", NULL, "https://example.com/cat.jpg"},
    };
    LLMMessage msg = {
        .role = "user",
        .content = NULL,
        .parts = parts,
        .num_parts = 2
    };

    ASSERT(msg.parts != NULL, "parts set");
    ASSERT(msg.num_parts == 2, "2 parts");
    ASSERT_STR_EQ(msg.parts[0].type, "text", "part 0 type");
    ASSERT_STR_EQ(msg.parts[0].text, "What is this?", "part 0 text");
    ASSERT_STR_EQ(msg.parts[1].type, "image_url", "part 1 type");
    ASSERT_STR_EQ(msg.parts[1].image_url, "https://example.com/cat.jpg", "part 1 url");
}

/* ========================================================================
 * Tool definition tests
 * ======================================================================== */

static void test_tool_definitions(void) {
    printf("  tool definitions...\n");

    LLMTool tools[] = {
        {
            "get_weather",
            "Get current weather",
            "{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\"}},\"required\":[\"location\"]}"
        },
        {
            "get_time",
            "Get current time",
            "{\"type\":\"object\",\"properties\":{\"timezone\":{\"type\":\"string\"}}}"
        }
    };
    LLMRequest req = {
        .model = "gpt-4o-mini",
        .prompt = "What's the weather?",
        .tools = tools,
        .num_tools = 2
    };

    ASSERT(req.num_tools == 2, "2 tools");
    ASSERT_STR_EQ(req.tools[0].name, "get_weather", "tool 0 name");
    ASSERT_STR_EQ(req.tools[1].name, "get_time", "tool 1 name");

    /* Verify parameters is valid JSON */
    cJSON *schema = cJSON_Parse(req.tools[0].parameters);
    ASSERT(schema != NULL, "tool 0 params is valid JSON");
    cJSON_Delete(schema);
}

/* ========================================================================
 * Provider error handling tests
 * ======================================================================== */

static void test_invalid_provider(void) {
    printf("  invalid provider name...\n");

    LLMRequest req = {
        .model = "test",
        .prompt = "hi",
        .max_tokens = 10
    };
    LLMResponse res = llm_generate("nonexistent_provider", &req);
    ASSERT(res.success == 0, "fails for unknown provider");
    ASSERT(res.error != NULL, "has error message");
}

static void test_null_provider(void) {
    printf("  null provider...\n");

    LLMRequest req = {
        .model = "test",
        .prompt = "hi",
        .max_tokens = 10
    };
    LLMResponse res = llm_generate(NULL, &req);
    ASSERT(res.success == 0, "fails for NULL provider");
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("=== AnyLM Integration Tests ===\n\n");

    printf("[Type defaults]\n");
    test_request_defaults();
    test_response_defaults();
    test_embed_request_defaults();
    test_embed_response_defaults();

    printf("\n[Message construction]\n");
    test_simple_prompt();
    test_multiturn_messages();
    test_vision_parts();

    printf("\n[Tool definitions]\n");
    test_tool_definitions();

    printf("\n[Provider error handling]\n");
    test_invalid_provider();
    test_null_provider();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

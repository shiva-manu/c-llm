/**
 * test_json_escape.c
 *
 * Unit tests for json_escape() function.
 * Tests edge cases: special characters, unicode control chars, empty strings.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"

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

static void test_basic(void) {
    printf("  basic strings...\n");

    char *r = json_escape("hello");
    ASSERT_STR_EQ(r, "\"hello\"", "simple word");
    free(r);

    r = json_escape("");
    ASSERT_STR_EQ(r, "\"\"", "empty string");
    free(r);
}

static void test_quotes(void) {
    printf("  quotes...\n");

    char *r = json_escape("say \"hi\"");
    ASSERT_STR_EQ(r, "\"say \\\"hi\\\"\"", "double quotes");
    free(r);
}

static void test_backslash(void) {
    printf("  backslash...\n");

    char *r = json_escape("path\\to\\file");
    ASSERT_STR_EQ(r, "\"path\\\\to\\\\file\"", "backslashes");
    free(r);
}

static void test_newlines(void) {
    printf("  newlines and tabs...\n");

    char *r = json_escape("line1\nline2");
    ASSERT_STR_EQ(r, "\"line1\\nline2\"", "newline");
    free(r);

    r = json_escape("col1\tcol2");
    ASSERT_STR_EQ(r, "\"col1\\tcol2\"", "tab");
    free(r);

    r = json_escape("cr\r");
    ASSERT_STR_EQ(r, "\"cr\\r\"", "carriage return");
    free(r);
}

static void test_control_chars(void) {
    printf("  control characters...\n");

    /* \x01 should become \u0001 */
    char *r = json_escape("\x01");
    ASSERT_STR_EQ(r, "\"\\u0001\"", "control char 0x01");
    free(r);

    /* \x1f should become \u001f */
    r = json_escape("\x1f");
    ASSERT_STR_EQ(r, "\"\\u001f\"", "control char 0x1f");
    free(r);
}

static void test_mixed(void) {
    printf("  mixed content...\n");

    char *r = json_escape("hello \"world\"\n\t\\end");
    ASSERT_STR_EQ(r, "\"hello \\\"world\\\"\\n\\t\\\\end\"",
                  "mixed special chars");
    free(r);
}

static void test_unicode_safe(void) {
    printf("  multi-byte UTF-8 passthrough...\n");

    /* UTF-8 emoji should pass through unchanged */
    char *r = json_escape("hello \xc3\xa9");  /* e-acute */
    ASSERT(r != NULL, "UTF-8 not null");
    ASSERT(strstr(r, "\xc3\xa9") != NULL, "UTF-8 preserved");
    free(r);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(void) {
    printf("=== json_escape Unit Tests ===\n\n");

    test_basic();
    test_quotes();
    test_backslash();
    test_newlines();
    test_control_chars();
    test_mixed();
    test_unicode_safe();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

# Contributing to AnyLM

Thanks for helping improve AnyLM. This guide covers development setup, coding conventions, and how to add new providers.

## Development Setup

### Dependencies

- C compiler with C99 support (gcc, clang, MSVC)
- libcurl development headers and library
- cJSON development headers and library

#### Ubuntu/Debian
```bash
sudo apt install build-essential libcurl4-openssl-dev libcjson-dev
```

#### macOS
```bash
brew install curl cjson
```

#### Fedora/RHEL
```bash
sudo dnf install gcc libcurl-devel cjson-devel
```

#### Arch Linux
```bash
sudo pacman -S base-devel curl cjson
```

### Build

```bash
cmake -S . -B build
cmake --build build -j
```

### Quick Compile (No CMake)

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o llm_example \
  examples/main.c src/core.c src/http.c src/parse.c src/providers/*.c \
  -lcurl -lcjson
```

### Run

```bash
export OPENAI_API_KEY="sk-..."
./build/example
```

## Project Layout

```
include/llm.h          Public API (the only header users include)
src/core.c             Provider dispatch table
src/http.c             HTTP POST + streaming helpers (libcurl)
src/http.h             Internal HTTP header
src/parse.c            JSON response parsing (cJSON)
src/parse.h            Internal parsing header
src/providers/         Provider implementations (17 files)
examples/              Runnable demos
```

### Internal vs Public Headers

- `include/llm.h` is the public API. Users only include this.
- `src/http.h` and `src/parse.h` are internal. Providers include these with relative paths (`#include "../http.h"`).

## Coding Style

- **C99 compatible.** No C11 features, no VLAs, no `_Generic`.
- **Readable, boring C over cleverness.** Clear variable names, straightforward control flow.
- **No global state.** All state is local or passed through function arguments.
- **Error handling through LLMResponse.** On failure: `.success = 0`, `.error = "static string"`. On success: `.success = 1`, `.text` and `.content` are heap-allocated.
- **Memory ownership is explicit.** Document who allocates and who frees.
- **Files end with a newline.**

### Error handling pattern

```c
LLMResponse my_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    // Check preconditions
    const char *key = getenv("MY_API_KEY");
    if (!key) {
        res.error = "Missing MY_API_KEY environment variable";
        return res;
    }

    // Allocate resources
    char *body = malloc(...);
    if (!body) {
        res.error = "Memory allocation failed";
        return res;
    }

    // ... do work ...

    // On success
    res.text = response;
    res.content = parsed_content;
    res.success = 1;
    return res;
}
```

### Memory ownership contract

| Field | On Success | On Failure |
|---|---|---|
| `res.text` | Heap-allocated, caller must `free()` | Undefined |
| `res.content` | Heap-allocated, caller must `free()` | Undefined (usually NULL) |
| `res.error` | Undefined (usually NULL) | Static string literal, caller must NOT `free()` |

## Adding a New Provider

### Step 1: Create the provider file

Create `src/providers/<provider>.c`:

```c
/**
 * <provider>.c
 *
 * <Provider Name> LLM API provider.
 *
 * Environment:
 *   <ENV_VAR> must be set before calling <provider>_generate().
 */

#include "llm.h"
#include "../http.h"
#include "../parse.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

LLMResponse <provider>_generate(const LLMRequest *req) {
    LLMResponse res = {0};

    // 1. Get API key
    const char *api_key = getenv("<ENV_VAR>");
    if (!api_key) {
        res.error = "Missing <ENV_VAR> environment variable";
        return res;
    }

    // 2. Escape strings for JSON
    char *esc_model  = json_escape(req->model);
    char *esc_prompt = json_escape(req->prompt);
    if (!esc_model || !esc_prompt) {
        free(esc_model); free(esc_prompt);
        res.error = "Memory allocation failed";
        return res;
    }

    // 3. Build JSON body (include system prompt if set)
    // 4. Set HTTP headers (Content-Type, Authorization)
    // 5. Call http_post() or http_post_stream()
    // 6. Parse response with appropriate parse_* function
    // 7. Return result

    res.text = response;
    res.content = parse_..._response(response);
    res.success = 1;
    return res;
}
```

### Step 2: If the provider uses OpenAI-compatible format

Instead of a full implementation, create a thin wrapper:

```c
#include "llm.h"

extern LLMResponse openai_compat_generate(const char *url, const char *env_var,
                                          const LLMRequest *req);

LLMResponse myprovider_generate(const LLMRequest *req) {
    return openai_compat_generate(
        "https://api.myprovider.com/v1/chat/completions",
        "MYPROVIDER_API_KEY", req);
}
```

This automatically gets system prompt, streaming, and response parsing support.

### Step 3: Wire into dispatch

Add to `src/core.c`:

1. Add forward declaration:
```c
LLMResponse myprovider_generate(const LLMRequest *req);
```

2. Add to the `providers[]` table:
```c
{ "myprovider", myprovider_generate },
```

### Step 4: Add response parser (if new API format)

If the provider uses a unique JSON response format, add a parser to `src/parse.c`:

```c
char *parse_myprovider_response(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;
    // ... navigate JSON structure ...
    char *result = dup_string(text_node);
    cJSON_Delete(root);
    return result;
}
```

Declare it in `src/parse.h`.

### Step 5: Add streaming support (if provider supports SSE)

If the provider supports server-sent events:

1. Add a streaming chunk parser function
2. Handle `stream=true` in the JSON body
3. Use `http_post_stream()` with an SSE line parser

See `src/providers/openai.c` or `src/providers/claude.c` for reference implementations.

### Step 6: Test

```bash
export MY_API_KEY="..."
gcc -std=c99 -Wall -Wextra -Iinclude -o test \
  examples/main.c src/core.c src/http.c src/parse.c src/providers/*.c \
  -lcurl -lcjson
./test
```

## Provider API Reference

Here are the key API endpoints and formats for each provider, useful when adding or fixing providers:

### OpenAI-compatible format
Used by: OpenAI, Groq, Together, Perplexity, DeepSeek, Fireworks, xAI, Cerebras, SambaNova, OpenRouter, Ollama, Mistral

```
POST /v1/chat/completions
Authorization: Bearer <key>
Content-Type: application/json

{
  "model": "model-name",
  "messages": [
    {"role": "system", "content": "..."},  // optional
    {"role": "user", "content": "..."}
  ],
  "max_tokens": 100,
  "temperature": 0.7,
  "stream": false
}

Response: {"choices":[{"message":{"content":"..."}}]}
Stream:   {"choices":[{"delta":{"content":"..."}}]}
```

### Anthropic Claude
```
POST /v1/messages
x-api-key: <key>
anthropic-version: 2023-06-01
Content-Type: application/json

{
  "model": "claude-3-haiku-20240307",
  "max_tokens": 100,
  "temperature": 0.7,
  "system": "...",  // top-level, not in messages
  "messages": [{"role": "user", "content": "..."}],
  "stream": false
}

Response: {"content":[{"text":"..."}]}
Stream:   {"type":"content_block_delta","delta":{"text":"..."}}
```

### Google Gemini
```
POST /v1beta/models/{model}:generateContent?key=<key>
Content-Type: application/json

{
  "contents": [{"parts": [{"text": "..."}]}],
  "systemInstruction": {"parts": [{"text": "..."}]},  // optional
  "generationConfig": {
    "maxOutputTokens": 100,
    "temperature": 0.7
  }
}

Response: {"candidates":[{"content":{"parts":[{"text":"..."}]}}]}
Stream:   Same format, endpoint: streamGenerateContent?alt=sse
```

### Cohere
```
POST /v2/chat
Authorization: Bearer <key>
Content-Type: application/json

{
  "model": "command-r-plus",
  "messages": [{"role": "user", "content": "..."}],
  "preamble": "...",  // system prompt
  "max_tokens": 100,
  "temperature": 0.7
}

Response: {"message":{"content":[{"text":"..."}]}}
```

## Testing

There is no dedicated test suite yet. For now:

1. Build the library with `cmake --build build`.
2. Run `./build/example` with the relevant API keys set.
3. Uncomment different providers in `examples/main.c` to test each one.
4. Test both streaming and non-streaming modes.

## Questions?

Open an issue on GitHub or check the existing code for patterns.

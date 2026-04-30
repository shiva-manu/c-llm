# AnyLM (C)

A tiny C99 client library that provides a single entrypoint to call different LLM providers through a common interface.

## What’s In Here

- `include/llm.h`: public API (`LLMRequest`, `LLMResponse`, `llm_generate`)
- `src/core.c`: provider dispatch (`openai`, `claude`, ...)
- `src/http.c`: minimal `http_post()` helper implemented with libcurl
- `src/providers/`: provider backends (currently `openai`, `claude` stub)
- `examples/main.c`: a minimal demo program

## Dependencies

- C compiler with C99 support
- `libcurl`
- `cjson` (linked in `CMakeLists.txt`; not fully used yet in the OpenAI backend)

## Build (CMake)

```bash
cmake -S . -B build
cmake --build build -j
```

This produces:

- `build/libllm.a`
- `build/example`

## Run The Example

```bash
export OPENAI_API_KEY="sk-..."
./build/example
```

## Build (Single Command GCC)

If you don’t want to use CMake:

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o llm_example \
  examples/main.c src/core.c src/http.c src/providers/openai.c src/providers/claude.c \
  -lcurl
```

## API Sketch

```c
#include "llm.h"

LLMRequest req = {
  .model = "gpt-4o-mini",
  .prompt = "Hello from C!",
  .max_tokens = 100,
  .temperature = 0.7f,
};

LLMResponse res = llm_generate("openai", &req);
if (res.success) {
  /* res.text is heap-allocated; caller must free() it */
  free(res.text);
} else {
  /* res.error is a static string; do not free */
}
```

## Contributing

See `CONTRIBUTING.md`.


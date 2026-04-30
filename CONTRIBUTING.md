# Contributing

Thanks for helping improve AnyLM.

## Development Setup

### Dependencies

- C compiler with C99 support
- libcurl development headers + library
- cJSON development headers + library (currently linked by CMake)

### Build

```bash
cmake -S . -B build
cmake --build build -j
```

### Quick Compile (No CMake)

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o llm_example \
  examples/main.c src/core.c src/http.c src/providers/openai.c src/providers/claude.c \
  -lcurl
```

## Project Layout

- `include/`: public headers
- `src/`: library sources
- `src/providers/`: provider implementations
- `examples/`: small runnable demos

## Coding Style (Practical)

- Keep the code C99-compatible.
- Prefer readable, boring C over cleverness.
- Avoid global state unless it’s unavoidable for a dependency.
- Return errors through `LLMResponse` (`.success = 0`, `.error = "..."`).
- On success, return heap-allocated `.text` and document ownership.
- Ensure files end with a newline.

## Adding A New Provider

1. Create `src/providers/<provider>.c` implementing:

```c
LLMResponse <provider>_generate(const LLMRequest *req);
```

2. Wire it into the dispatch in `src/core.c`.
3. Add a tiny example in `examples/` if possible.

## Testing

There’s no dedicated test suite yet. For now:

- Build the library.
- Run `examples/main.c` with the relevant API keys set in your environment.


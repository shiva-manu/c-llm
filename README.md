# AnyLM

**One C function. 17 LLM providers. Zero dependencies beyond libcurl.**

AnyLM is a single-header C99 library that gives your application access to every major LLM provider through a unified interface. Switch between OpenAI, Claude, Gemini, Groq, and 13 more providers by changing one string -- no code changes, no vendor lock-in.

```c
LLMRequest req = {
    .model  = "gpt-4o-mini",
    .prompt = "Explain monads in simple terms.",
    .system = "You are a patient teacher.",
    .max_tokens = 200,
    .temperature = 0.7f
};

LLMResponse res = llm_generate("openai", &req);
printf("%s\n", res.content);  // extracted text, ready to use
free(res.content);
free(res.text);
```

Swap to Claude by changing one line:

```c
LLMResponse res = llm_generate("claude", &req);  // that's it
```

---

## Table of Contents

- [Features](#features)
- [Supported Providers](#supported-providers)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Usage Examples](#usage-examples)
- [Use Cases](#use-cases)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

---

## Features

- **17 LLM providers** through a single `llm_generate()` function
- **System prompt support** -- pass a system message to any provider
- **Streaming** -- receive tokens in real-time via callback
- **Response parsing** -- extracted text in `res.content`, raw JSON in `res.text`
- **Pure C99** -- compiles on Linux, macOS, Windows, embedded systems, anywhere
- **Minimal dependencies** -- only libcurl (pre-installed on most systems) and cJSON
- **Static library** -- link `libllm.a` into your project, no runtime dependencies
- **Thread-safe** -- no global state (except curl_global_init which is application-managed)
- **MIT licensed** -- use freely in commercial and open-source projects

---

## Supported Providers

### Cloud APIs

| Provider | String | Env Variable | Endpoint | Notes |
|---|---|---|---|---|
| **OpenAI** | `"openai"` | `OPENAI_API_KEY` | `api.openai.com` | GPT-4o, GPT-4, GPT-3.5-turbo |
| **Anthropic Claude** | `"claude"` | `ANTHROPIC_API_KEY` | `api.anthropic.com` | Claude 3.5, Claude 3 Opus/Sonnet/Haiku |
| **Google Gemini** | `"gemini"` | `GEMINI_API_KEY` | `generativelanguage.googleapis.com` | Gemini 2.0, Gemini 1.5 Pro/Flash |
| **Groq** | `"groq"` | `GROQ_API_KEY` | `api.groq.com` | Ultra-fast inference, Llama/Mixtral |
| **Together AI** | `"together"` | `TOGETHER_API_KEY` | `api.together.xyz` | Open-source model hosting |
| **Perplexity** | `"perplexity"` | `PERPLEXITY_API_KEY` | `api.perplexity.ai` | Search-augmented LLM |
| **DeepSeek** | `"deepseek"` | `DEEPSEEK_API_KEY` | `api.deepseek.com` | DeepSeek-V3, DeepSeek-Coder |
| **Fireworks AI** | `"fireworks"` | `FIREWORKS_API_KEY` | `api.fireworks.ai` | Fast open-source inference |
| **xAI (Grok)** | `"xai"` | `XAI_API_KEY` | `api.x.ai` | Grok-2, Grok-3 |
| **Cerebras** | `"cerebras"` | `CEREBRAS_API_KEY` | `api.cerebras.ai` | Wafer-scale inference |
| **SambaNova** | `"sambanova"` | `SAMBANOVA_API_KEY` | `api.sambanova.ai` | RDU-powered inference |
| **OpenRouter** | `"openrouter"` | `OPENROUTER_API_KEY` | `openrouter.ai` | Multi-provider gateway |
| **Cohere** | `"cohere"` | `COHERE_API_KEY` | `api.cohere.com` | Command R/R+, Embed |
| **Mistral** | `"mistral"` | `MISTRAL_API_KEY` | `api.mistral.ai` | Mistral Large, Medium, Small |
| **Replicate** | `"replicate"` | `REPLICATE_API_TOKEN` | `api.replicate.com` | Any open-source model |
| **Hugging Face** | `"huggingface"` | `HF_API_TOKEN` | `api-inference.huggingface.co` | 500k+ models on HF Hub |

### Local Inference

| Provider | String | Env Variable | Endpoint | Notes |
|---|---|---|---|---|
| **Ollama** | `"ollama"` | `OLLAMA_API_KEY` (optional) | `localhost:11434` | Run models locally, no API key needed |

### Getting API Keys

| Provider | Sign Up URL | Free Tier |
|---|---|---|
| OpenAI | [platform.openai.com](https://platform.openai.com) | $5 credit for new accounts |
| Anthropic | [console.anthropic.com](https://console.anthropic.com) | Free tier available |
| Google Gemini | [aistudio.google.com](https://aistudio.google.com) | Free tier with generous limits |
| Groq | [console.groq.com](https://console.groq.com) | Free tier, very fast |
| Together AI | [api.together.xyz](https://api.together.xyz) | $5 credit for new accounts |
| Perplexity | [perplexity.ai](https://perplexity.ai) | Limited free tier |
| DeepSeek | [platform.deepseek.com](https://platform.deepseek.com) | Free tier available |
| Fireworks AI | [fireworks.ai](https://fireworks.ai) | $1 credit for new accounts |
| xAI | [console.x.ai](https://console.x.ai) | Free tier available |
| Cerebras | [cloud.cerebras.ai](https://cloud.cerebras.ai) | Free tier available |
| SambaNova | [cloud.sambanova.ai](https://cloud.sambanova.ai) | Free tier available |
| OpenRouter | [openrouter.ai](https://openrouter.ai) | $1 credit for new accounts |
| Cohere | [cohere.com](https://cohere.com) | Free tier available |
| Mistral | [console.mistral.ai](https://console.mistral.ai) | Free tier available |
| Replicate | [replicate.com](https://replicate.com) | Pay per use |
| Hugging Face | [huggingface.co](https://huggingface.co) | Free tier available |
| Ollama | [ollama.ai](https://ollama.ai) | Completely free (local) |

---

## Installation

### Dependencies

- **C compiler** with C99 support (gcc, clang, msvc)
- **libcurl** -- HTTP client library (pre-installed on most systems)
- **cJSON** -- lightweight JSON parser

#### Install dependencies on Ubuntu/Debian:
```bash
sudo apt install libcurl4-openssl-dev libcjson-dev
```

#### Install dependencies on macOS:
```bash
brew install curl cjson
```

#### Install dependencies on Fedora/RHEL:
```bash
sudo dnf install libcurl-devel cjson-devel
```

#### Install dependencies on Arch Linux:
```bash
sudo pacman -S curl cjson
```

### Build with CMake

```bash
git clone https://github.com/your-username/anylm.git
cd anylm
cmake -S . -B build
cmake --build build -j
```

This produces:
- `build/libllm.a` -- static library to link into your project
- `build/example` -- demo program

### Build with GCC (single command)

```bash
gcc -std=c99 -Wall -Wextra -Iinclude -o llm_example \
  examples/main.c src/core.c src/http.c src/parse.c src/providers/*.c \
  -lcurl -lcjson
```

### Link into your project

```bash
# Compile your code against the library
gcc -std=c99 -Iinclude -o myapp myapp.c -Lbuild -lllm -lcurl -lcjson
```

Or add to your CMakeLists.txt:
```cmake
add_subdirectory(anylm)
target_link_libraries(myapp llm curl cjson)
```

---

## Quick Start

### 1. Set your API key

```bash
export OPENAI_API_KEY="sk-your-key-here"
```

### 2. Write your code

```c
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "llm.h"

int main(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    LLMRequest req = {
        .model       = "gpt-4o-mini",
        .prompt      = "What is the meaning of life?",
        .system      = "You are a wise philosopher.",
        .max_tokens  = 300,
        .temperature = 0.8f
    };

    LLMResponse res = llm_generate("openai", &req);

    if (res.success) {
        printf("%s\n", res.content);
        free(res.content);
        free(res.text);
    } else {
        fprintf(stderr, "Error: %s\n", res.error);
    }

    curl_global_cleanup();
    return 0;
}
```

### 3. Compile and run

```bash
gcc -std=c99 -Iinclude -o myapp myapp.c -Lbuild -lllm -lcurl -lcjson
./myapp
```

---

## API Reference

### Types

#### `LLMRequest`

Input parameters for a language model call.

```c
typedef struct {
    const char       *model;           // Model identifier (e.g. "gpt-4o-mini")
    const char       *prompt;          // The user message to send
    const char       *system;          // Optional system prompt (NULL for none)
    int               max_tokens;      // Maximum tokens to generate
    float             temperature;     // 0.0 = deterministic, 2.0 = very random
    int               stream;          // Non-zero to enable streaming
    LLMStreamCallback stream_cb;       // Callback for streaming tokens
    void             *stream_user_data; // Opaque pointer passed to stream_cb
} LLMRequest;
```

#### `LLMResponse`

Result returned by a backend.

```c
typedef struct {
    int          success;  // 1 on success, 0 on failure
    char        *text;     // Raw JSON response (caller must free)
    char        *content;  // Extracted text content (caller must free)
    const char  *error;    // Error message on failure (static, do NOT free)
} LLMResponse;
```

**Memory ownership:**
- On success: `text` and `content` are heap-allocated. Caller must `free()` both.
- On failure: `error` is a static string literal. Caller must NOT `free()` it.

#### `LLMStreamCallback`

Callback function for streaming tokens.

```c
typedef void (*LLMStreamCallback)(const char *token, void *user_data);
```

- `token` -- the token text (valid only for the callback duration, do not store)
- `user_data` -- the opaque pointer from `LLMRequest.stream_user_data`

### Functions

#### `llm_generate`

```c
LLMResponse llm_generate(const char *provider, const LLMRequest *req);
```

Send a non-streaming request to the named provider. Returns the full response with both raw JSON (`text`) and extracted content (`content`).

#### `llm_generate_stream`

```c
LLMResponse llm_generate_stream(const char *provider, const LLMRequest *req);
```

Send a streaming request. The request must have `stream=1` and `stream_cb` set. Tokens are delivered via the callback as they arrive. Returns success after the stream completes.

---

## Usage Examples

### Basic request with different providers

```c
// OpenAI
LLMResponse res = llm_generate("openai", &req);

// Claude
LLMResponse res = llm_generate("claude", &req);

// Groq (fastest)
LLMResponse res = llm_generate("groq", &req);

// Local with Ollama (no API key needed)
LLMResponse res = llm_generate("ollama", &req);
```

### Streaming output

```c
void on_token(const char *token, void *ud) {
    printf("%s", token);
    fflush(stdout);
}

LLMRequest req = {
    .model          = "gpt-4o-mini",
    .prompt         = "Write a short story.",
    .max_tokens     = 500,
    .temperature    = 0.9f,
    .stream         = 1,
    .stream_cb      = on_token,
    .stream_user_data = NULL
};

LLMResponse res = llm_generate_stream("openai", &req);
```

### Provider fallback

```c
const char *providers[] = {"groq", "openai", "claude", NULL};
LLMResponse res = {0};

for (int i = 0; providers[i]; i++) {
    res = llm_generate(providers[i], &req);
    if (res.success) break;
    fprintf(stderr, "%s failed: %s, trying next...\n", providers[i], res.error);
}
```

### System prompt for specialized behavior

```c
// Code review assistant
LLMRequest req = {
    .model  = "claude",
    .prompt = "Review this C code for bugs: ...",
    .system = "You are a senior C developer. Focus on memory safety, buffer overflows, and undefined behavior.",
    .max_tokens = 1000
};

// Translation
LLMRequest req = {
    .model  = "deepseek",
    .prompt = "Translate to Japanese: Hello, how are you?",
    .system = "You are a professional translator. Provide natural, colloquial translations.",
    .max_tokens = 100
};
```

### Multi-turn conversation

```c
// Build conversation history manually
char conversation[4096] = "";
strcat(conversation, "User: What is Rust?\n");
// ... get response, append to conversation ...
strcat(conversation, "Assistant: Rust is a systems programming language...\n");
strcat(conversation, "User: How does it compare to C?\n");

LLMRequest req = {
    .model  = "openai",
    .prompt = conversation,
    .system = "You are a programming language expert.",
    .max_tokens = 500
};
```

---

## Use Cases

### Where AnyLM fits

AnyLM is designed for C and C++ applications that need LLM capabilities without the complexity of SDKs, async runtimes, or language-specific dependencies.

#### 1. CLI tools and developer utilities

Build AI-powered command-line tools in C that work on any platform.

```c
// A CLI code review tool
LLMRequest req = {
    .model  = "openai",
    .prompt = source_code,
    .system = "Review this code for bugs and suggest improvements.",
    .max_tokens = 1000
};
LLMResponse res = llm_generate("openai", &req);
```

#### 2. Embedded systems and IoT

Run inference on resource-constrained devices using Ollama or a remote API.

```c
// Use local Ollama on a Raspberry Pi
LLMResponse res = llm_generate("ollama", &req);
```

#### 3. Game NPC dialogue

Generate dynamic NPC conversations in C game engines.

```c
LLMRequest req = {
    .model  = "groq",  // fast inference for real-time
    .prompt = "The player just asked about the ancient ruins.",
    .system = "You are a mysterious merchant in a fantasy RPG. Speak in riddles.",
    .max_tokens = 150,
    .temperature = 1.0f,
    .stream = 1,
    .stream_cb = type_text_animation,
    .stream_user_data = &npc_dialog_box
};
llm_generate_stream("groq", &req);
```

#### 4. Server-side request handling

Embed in C/C++ web servers (nginx modules, FastCGI, custom HTTP servers) for AI-powered endpoints.

```c
// In a request handler
LLMResponse res = llm_generate("openai", &req);
http_response_send(client_fd, res.content);
free(res.content);
free(res.text);
```

#### 5. Database query generation

Convert natural language to SQL or other query languages.

```c
LLMRequest req = {
    .model  = "claude",
    .prompt = "Find all users who signed up in the last 30 days and have made at least 3 purchases.",
    .system = "Generate a PostgreSQL query. Only output the SQL, no explanation.",
    .max_tokens = 200
};
```

#### 6. Text processing pipelines

Build text summarization, classification, extraction, and transformation pipelines.

```c
// Summarize
req.prompt = long_document;
req.system = "Summarize the following text in 3 bullet points.";
LLMResponse summary = llm_generate("openai", &req);

// Classify
req.prompt = customer_email;
req.system = "Classify this email as: billing, technical, sales, or other. Only output the category.";
LLMResponse category = llm_generate("groq", &req);

// Extract
req.prompt = resume_text;
req.system = "Extract the candidate's name, email, and years of experience as JSON.";
LLMResponse data = llm_generate("openai", &req);
```

#### 7. Provider cost optimization

Use the cheapest/fastest provider for each task.

```c
// Simple classification: use cheap, fast model
llm_generate("groq", &simple_req);

// Complex reasoning: use best model
llm_generate("claude", &complex_req);

// Code generation: use code-specialized model
llm_generate("deepseek", &code_req);
```

#### 8. Testing and prototyping

Quickly test prompts across multiple providers without changing code.

```c
const char *providers[] = {"openai", "claude", "gemini", "groq", "mistral"};
for (int i = 0; i < 5; i++) {
    printf("=== %s ===\n", providers[i]);
    LLMResponse res = llm_generate(providers[i], &req);
    if (res.success) printf("%s\n\n", res.content);
    free(res.content); free(res.text);
}
```

#### 9. Chatbot backends

Build chat applications with streaming for real-time token display.

```c
// WebSocket chat handler
void handle_chat_message(const char *user_msg, websocket_t *ws) {
    LLMRequest req = {
        .model = "openai",
        .prompt = user_msg,
        .system = chatbot_system_prompt,
        .max_tokens = 1000,
        .stream = 1,
        .stream_cb = send_token_to_websocket,
        .stream_user_data = ws
    };
    llm_generate_stream("openai", &req);
}
```

#### 10. Document analysis and RAG

Process documents with LLMs for search, Q&A, and knowledge extraction.

```c
// Chunk document, embed with one provider, query with another
LLMRequest embed_req = {
    .model  = "openai",  // use embedding-capable model
    .prompt = document_chunk,
    .max_tokens = 1
};

// Answer questions against the document
LLMRequest qa_req = {
    .model  = "claude",
    .prompt = question,
    .system = "Answer based on this context:\n" + retrieved_chunks,
    .max_tokens = 500
};
```

---

## Project Structure

```
anylm/
  include/
    llm.h                  # Public API header (the only header you need)
  src/
    core.c                 # Provider dispatch table
    http.c                 # HTTP POST + streaming helpers (libcurl)
    http.h                 # Internal HTTP header
    parse.c                # JSON response parsing (cJSON)
    parse.h                # Internal parsing header
    providers/
      openai.c             # OpenAI Chat Completions
      openai_compat.c      # Shared code for OpenAI-compatible APIs
      claude.c             # Anthropic Claude Messages API
      gemini.c             # Google Gemini generateContent
      groq.c               # Groq (thin wrapper over openai_compat)
      together.c           # Together AI (thin wrapper)
      perplexity.c         # Perplexity (thin wrapper)
      deepseek.c           # DeepSeek (thin wrapper)
      fireworks.c          # Fireworks AI (thin wrapper)
      xai.c                # xAI Grok (thin wrapper)
      cerebras.c           # Cerebras (thin wrapper)
      sambanova.c          # SambaNova (thin wrapper)
      openrouter.c         # OpenRouter (thin wrapper)
      ollama.c             # Ollama local inference
      cohere.c             # Cohere Chat API
      mistral.c            # Mistral Chat API
      replicate.c          # Replicate Predictions API
      huggingface.c        # Hugging Face Inference API
  examples/
    main.c                 # Demo program with all providers
  CMakeLists.txt           # CMake build system
  README.md                # This file
  CONTRIBUTING.md          # Contribution guide
  LICENSE                  # MIT License
```

### Architecture

```
                    +------------------+
                    |   llm.h (API)    |
                    +--------+---------+
                             |
                    +--------v---------+
                    |    core.c        |
                    |  (dispatch)      |
                    +--------+---------+
                             |
          +------------------+------------------+
          |         |        |        |         |
    +-----v---+ +---v----+ +-v-----+ +v------+ +v--------+
    | openai  | | claude | | gemini| |cohere | | ...     |
    | .c      | | .c     | | .c    | | .c    | | .c      |
    +---------+ +--------+ +-------+ +-------+ +---------+
          |
    +-----v---------+
    | openai_compat.c|  (shared by: groq, together, perplexity,
    +----------------+   deepseek, fireworks, xai, cerebras,
                         sambanova, openrouter, ollama)
                             |
                    +--------v---------+
                    |  http.c / http.h |
                    |  (libcurl)       |
                    +------------------+
                    |  parse.c/parse.h |
                    |  (cJSON)         |
                    +------------------+
```

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup, coding style, and how to add new providers.

---

## License

MIT License. See [LICENSE](LICENSE) for details.

Copyright (c) 2026 Shivamani

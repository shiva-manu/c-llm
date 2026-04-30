#ifndef LLM_H
#define LLM_H


typedef struct{
    const char *model;
    const char *prompt;
    int max_tokens;
    float temperature;
} LLMRequest;


typedef struct{
    char *text;
    int success;
    char *error;
} LLMResponse;


LLMResponse llm_generate(const char* provider,LLMRequest *req);

#endif
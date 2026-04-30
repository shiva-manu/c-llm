#include <string.h>
#include "llm.h"

// forward declarations
LLMResponse openai_generate(LLMResponse *req);
LLMResponse claude_generate(LLMRequest *req);

LLMResponse llm_generate(const char *provider,LLMRequest *req){
    if(strcmp(provider,"openai")==0){
        return openai_generate(req);
    }else if(strcmp(provider,"claude")==0){
        return claude_generate(req);
    }
    LLMResponse res={0};
    res.success=0;
    res.error="Unkown provider";
    return res;
}
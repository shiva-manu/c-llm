cmake_minimum_required(VERSION 3.10)
project(ANYLM C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

file(GLOB SRC_FILES
     src/*.c
     src/providers/*.c
)

add_library(llm STATIC ${SRC_FILES})

target_link_libraries(llm curl cjson)

add_executable(example examples/main.c)

target_link_libraries(example llm)

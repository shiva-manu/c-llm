cmake_minimum_required(VERSION 3.10)
project(ANYLM C)

set(CMAKE_C_STANDARD 99)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CJSON REQUIRED libcjson)

include_directories(include src ${CJSON_INCLUDE_DIRS})

link_directories(${CJSON_LIBRARY_DIRS})

file(GLOB SRC_FILES
     src/*.c
     src/providers/*.c
)

add_library(llm STATIC ${SRC_FILES})

target_link_libraries(llm curl ${CJSON_LIBRARIES})

add_executable(example examples/main.c)

target_link_libraries(example llm)

# ---- Tests ----

enable_testing()

add_executable(test_parsers tests/test_parsers.c)
target_link_libraries(test_parsers llm)
add_test(NAME parsers COMMAND test_parsers)

add_executable(test_json_escape tests/test_json_escape.c)
target_link_libraries(test_json_escape llm)
add_test(NAME json_escape COMMAND test_json_escape)

add_executable(test_integration tests/test_integration.c)
target_link_libraries(test_integration llm)
add_test(NAME integration COMMAND test_integration)

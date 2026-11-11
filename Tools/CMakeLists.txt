cmake_minimum_required(VERSION 3.20)
project(universal_rag_harness VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rag_harness
    src/analytics.cpp
    src/harness.cpp
    src/json.cpp
    src/hermes.cpp
    src/ico.cpp
)
target_include_directories(rag_harness PUBLIC include)

if(MSVC)
  target_compile_options(rag_harness PRIVATE /W4 /permissive-)
else()
  target_compile_options(rag_harness PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(rag-cli src/main.cpp)
target_link_libraries(rag-cli PRIVATE rag_harness)

enable_testing()
add_executable(rag-tests tests/tests.cpp)
target_link_libraries(rag-tests PRIVATE rag_harness)
target_compile_definitions(rag-tests PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME rag-tests COMMAND rag-tests)

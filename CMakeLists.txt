cmake_minimum_required(VERSION 3.20)
project(SYJLLM VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYJ_BUILD_TESTS "Build SYJ tests" ON)
option(SYJ_BUILD_CLI "Build SYJ CLI" ON)
option(SYJ_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(SYJ_ENABLE_ASAN "Enable AddressSanitizer/UndefinedBehaviorSanitizer" OFF)

set(SYJ_LLAMA_REVISION "391fac16460f15233a7740550d858ac96df3419d" CACHE STRING "Pinned llama.cpp upstream commit")
set(SYJ_LLAMA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp" CACHE PATH "Vendored llama.cpp source directory")

if(NOT EXISTS "${SYJ_LLAMA_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Pinned llama.cpp source is missing. Populate third_party/llama.cpp at exact commit ${SYJ_LLAMA_REVISION} before configuring.")
endif()

# Keep llama.cpp build surface minimal for SYJ's low-RAM CPU-first runtime.
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_METAL OFF CACHE BOOL "" FORCE)
set(GGML_CUDA OFF CACHE BOOL "" FORCE)
set(GGML_VULKAN OFF CACHE BOOL "" FORCE)
set(GGML_OPENCL OFF CACHE BOOL "" FORCE)

add_subdirectory("${SYJ_LLAMA_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/llama.cpp" EXCLUDE_FROM_ALL)

add_library(syj_core STATIC
    src/core/version.cpp
    src/core/memory_budget.cpp
    src/core/runtime.cpp
)

target_include_directories(syj_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(syj_core PRIVATE llama)
target_compile_features(syj_core PUBLIC cxx_std_17)

if(WIN32)
    target_link_libraries(syj_core PRIVATE psapi)
endif()

if(SYJ_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(syj_core PRIVATE /W4 /permissive-)
    else()
        target_compile_options(syj_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
endif()

if(SYJ_ENABLE_ASAN AND NOT MSVC)
    target_compile_options(syj_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(syj_core PRIVATE -fsanitize=address,undefined)
endif()

if(SYJ_BUILD_CLI)
    add_executable(syj src/cli/main.cpp)
    target_link_libraries(syj PRIVATE syj_core)
    if(SYJ_ENABLE_WARNINGS AND NOT MSVC)
        target_compile_options(syj PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
    if(SYJ_ENABLE_ASAN AND NOT MSVC)
        target_compile_options(syj PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(syj PRIVATE -fsanitize=address,undefined)
    endif()
endif()

if(SYJ_BUILD_TESTS)
    enable_testing()

    add_executable(syj_core_tests tests/core_tests.cpp)
    target_link_libraries(syj_core_tests PRIVATE syj_core)
    add_test(NAME syj_core_tests COMMAND syj_core_tests)

    add_executable(syj_memory_budget_tests tests/memory_budget_tests.cpp)
    target_link_libraries(syj_memory_budget_tests PRIVATE syj_core)
    add_test(NAME syj_memory_budget_tests COMMAND syj_memory_budget_tests)
endif()

include(GNUInstallDirs)
install(TARGETS syj_core ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SYJ_BUILD_CLI)
    install(TARGETS syj RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

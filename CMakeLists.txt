cmake_minimum_required(VERSION 3.20)
project(SYJLLM VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYJ_BUILD_TESTS "Build SYJ tests" ON)
option(SYJ_BUILD_CLI "Build SYJ CLI" ON)
option(SYJ_BUILD_HTTP_API "Build optional embedded local HTTP API add-on" ON)
option(SYJ_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(SYJ_ENABLE_ASAN "Enable AddressSanitizer/UndefinedBehaviorSanitizer" OFF)

set(SYJ_LLAMA_REVISION "391fac16460f15233a7740550d858ac96df3419d" CACHE STRING "Pinned llama.cpp upstream commit")
set(SYJ_LLAMA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp" CACHE PATH "Vendored llama.cpp source directory")

if(NOT EXISTS "${SYJ_LLAMA_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Pinned llama.cpp source is missing. Populate third_party/llama.cpp at exact commit ${SYJ_LLAMA_REVISION} before configuring.")
endif()

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
    src/core/model_registry.cpp
    src/core/runtime.cpp
)
target_include_directories(syj_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
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

if(SYJ_BUILD_HTTP_API)
    find_package(Threads REQUIRED)
    add_library(syj_http_api STATIC
        src/api/local_api.cpp
        third_party/llama.cpp/vendor/cpp-httplib/httplib.cpp
    )
    target_include_directories(syj_http_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SYJ_LLAMA_SOURCE_DIR}/vendor/cpp-httplib
        ${SYJ_LLAMA_SOURCE_DIR}/vendor
    )
    target_link_libraries(syj_http_api PUBLIC syj_core Threads::Threads)
    target_compile_features(syj_http_api PUBLIC cxx_std_17)
    target_compile_definitions(syj_http_api PRIVATE
        CPPHTTPLIB_FORM_URL_ENCODED_PAYLOAD_MAX_LENGTH=1048576
        CPPHTTPLIB_LISTEN_BACKLOG=512
        CPPHTTPLIB_REQUEST_URI_MAX_LENGTH=32768
        CPPHTTPLIB_TCP_NODELAY=1
    )
    if(SYJ_ENABLE_WARNINGS AND NOT MSVC)
        target_compile_options(syj_http_api PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        set_source_files_properties(third_party/llama.cpp/vendor/cpp-httplib/httplib.cpp PROPERTIES COMPILE_OPTIONS "-w")
    endif()
    if(SYJ_ENABLE_ASAN AND NOT MSVC)
        target_compile_options(syj_http_api PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(syj_http_api PRIVATE -fsanitize=address,undefined)
    endif()
endif()

if(SYJ_BUILD_CLI)
    add_executable(syj
        src/cli/main.cpp
        src/cli/commands.cpp
    )
    target_link_libraries(syj PRIVATE syj_core)
    if(SYJ_BUILD_HTTP_API)
        target_link_libraries(syj PRIVATE syj_http_api)
    endif()
    target_include_directories(syj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
    add_executable(syj_model_registry_tests tests/model_registry_tests.cpp)
    target_link_libraries(syj_model_registry_tests PRIVATE syj_core)
    add_test(NAME syj_model_registry_tests COMMAND syj_model_registry_tests)
    add_executable(syj_cli_tests tests/cli_tests.cpp src/cli/commands.cpp)
    target_link_libraries(syj_cli_tests PRIVATE syj_core)
    target_include_directories(syj_cli_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    add_test(NAME syj_cli_tests COMMAND syj_cli_tests)
    if(SYJ_BUILD_HTTP_API)
        add_executable(syj_api_tests tests/local_api_tests.cpp)
        target_link_libraries(syj_api_tests PRIVATE syj_http_api)
        target_include_directories(syj_api_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${SYJ_LLAMA_SOURCE_DIR}/vendor/cpp-httplib
        )
        add_test(NAME syj_api_tests COMMAND syj_api_tests)
    endif()

    add_executable(syj_phase5_pipeline_tests tests/phase5_pipeline_tests.cpp)
    target_compile_features(syj_phase5_pipeline_tests PRIVATE cxx_std_17)
    target_compile_definitions(syj_phase5_pipeline_tests PRIVATE
        SYJ_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME syj_phase5_pipeline_tests COMMAND syj_phase5_pipeline_tests)
endif()

include(GNUInstallDirs)
install(TARGETS syj_core ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SYJ_BUILD_HTTP_API)
    install(TARGETS syj_http_api ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
if(SYJ_BUILD_CLI)
    install(TARGETS syj RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

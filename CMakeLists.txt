cmake_minimum_required(VERSION 3.20)
project(SYJLLM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYJ_BUILD_TESTS "Build SYJ tests" ON)
option(SYJ_BUILD_CLI "Build SYJ CLI" ON)
option(SYJ_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

add_library(syj_core STATIC
    src/core/version.cpp
)

target_include_directories(syj_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(syj_core PUBLIC cxx_std_17)

if(SYJ_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(syj_core PRIVATE /W4 /permissive-)
    else()
        target_compile_options(syj_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
endif()

if(SYJ_BUILD_CLI)
    add_executable(syj src/cli/main.cpp)
    target_link_libraries(syj PRIVATE syj_core)
    if(SYJ_ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(syj PRIVATE /W4 /permissive-)
        else()
            target_compile_options(syj PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
        endif()
    endif()
endif()

if(SYJ_BUILD_TESTS)
    enable_testing()
    add_executable(syj_core_tests tests/core_tests.cpp)
    target_link_libraries(syj_core_tests PRIVATE syj_core)
    add_test(NAME syj_core_tests COMMAND syj_core_tests)
endif()

include(GNUInstallDirs)
install(TARGETS syj_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(SYJ_BUILD_CLI)
    install(TARGETS syj RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

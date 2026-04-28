cmake_minimum_required(VERSION 3.20)

project(JobPostCounter LANGUAGES CXX)

add_executable(JobPostCounter
    src/main.cpp
)

target_compile_features(JobPostCounter PRIVATE cxx_std_17)
target_compile_definitions(JobPostCounter PRIVATE UNICODE _UNICODE NOMINMAX _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)

if (MSVC)
    target_compile_options(JobPostCounter PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(JobPostCounter PRIVATE -Wall -Wextra -Wpedantic -finput-charset=UTF-8)
endif()

target_link_libraries(JobPostCounter PRIVATE winhttp)

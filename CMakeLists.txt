cmake_minimum_required(VERSION 3.20)
project(ControllsFix C)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(WINVER=0x0501 _WIN32_WINNT=0x0501 _CRT_SECURE_NO_WARNINGS)

add_library(ControllsFix SHARED controlls_fix.c padmap.c)
target_link_options(ControllsFix PRIVATE /MANIFEST:NO)

enable_testing()
add_executable(test_padmap tests/test_padmap.c padmap.c)
add_test(NAME padmap COMMAND test_padmap)

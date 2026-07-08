cmake_minimum_required(VERSION 3.20)
project(zazaki_git VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
pkg_check_modules(DOCTEST REQUIRED IMPORTED_TARGET doctest)
pkg_check_modules(LIBGIT2 REQUIRED IMPORTED_TARGET libgit2)
pkg_check_modules(FTXUI REQUIRED IMPORTED_TARGET ftxui)

add_subdirectory(src)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

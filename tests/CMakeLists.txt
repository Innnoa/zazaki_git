add_executable(zazaki_git_tests
    test_main.cpp
    test_git_core.cpp
    test_git_extensions.cpp
    $<TARGET_OBJECTS:zazaki_git_lib>
)

target_include_directories(zazaki_git_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(zazaki_git_tests PRIVATE
    PkgConfig::DOCTEST
    PkgConfig::LIBGIT2
)

add_test(NAME zazaki_git_tests COMMAND zazaki_git_tests --success)

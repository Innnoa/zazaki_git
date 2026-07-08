add_library(zazaki_git_lib OBJECT
    git/repo.cpp
)

target_include_directories(zazaki_git_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zazaki_git_lib PUBLIC
    PkgConfig::FMT
    PkgConfig::FTXUI
    PkgConfig::LIBGIT2
)

add_executable(zazaki_git
    main.cpp
    $<TARGET_OBJECTS:zazaki_git_lib>
)

target_include_directories(zazaki_git PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zazaki_git PRIVATE
    PkgConfig::FMT
    PkgConfig::FTXUI
    PkgConfig::LIBGIT2
)

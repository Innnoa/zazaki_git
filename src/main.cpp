#include <ftxui/component/screen_interactive.hpp>

#include <filesystem>
#include <iostream>
#include <string>

#include "app/app.h"
#include "git/repo.h"

int main(int argc, char* argv[]) {
    using namespace ftxui;
    using namespace zazaki_git;

    std::string repo_path;
    if (argc > 1) {
        repo_path = argv[1];
    } else {
        std::error_code ec;
        repo_path = std::filesystem::current_path(ec).string();
    }

    GitRepo repo;
    auto result = repo.open(repo_path);
    if (result.is_err()) {
        std::cerr << "Failed to open repository: " << result.error().message()
                  << std::endl;
        return 1;
    }

    auto app_component = CreateApp(&repo);
    auto screen = ScreenInteractive::Fullscreen();
    screen.Loop(app_component);

    return 0;
}

#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>

#include <string>

namespace zazaki_git {

inline ftxui::Component TextInput(std::string* content,
                                  const std::string& placeholder = "") {
    using namespace ftxui;

    InputOption option;
    option.multiline = false;
    if (!placeholder.empty()) {
        return Input(content, placeholder, option);
    }
    return Input(content, option);
}

inline ftxui::Component TextInputMultiline(std::string* content,
                                           const std::string& placeholder = "") {
    using namespace ftxui;

    InputOption option;
    option.multiline = true;
    if (!placeholder.empty()) {
        return Input(content, placeholder, option);
    }
    return Input(content, option);
}

}  // namespace zazaki_git

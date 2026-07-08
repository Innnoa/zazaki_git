#pragma once

#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

#include "ui/colors.h"

namespace zazaki_git {

inline ftxui::Element TabBar(int tab_index) {
    using namespace ftxui;

    std::vector<std::string> tabs = {
        "Status", "Staging", "Branches", "Log", "Stash", "Remote"
    };

    Elements items;
    for (size_t i = 0; i < tabs.size(); i++) {
        auto num = std::to_string(i + 1);
        auto label = " " + num + ": " + tabs[i] + " ";
        Element el;
        if (static_cast<int>(i) == tab_index) {
            el = text(label) | bold | color(colors::kBlue) | inverted;
        } else {
            el = text(label) | color(colors::kOverlay0);
        }
        items.push_back(el);
        if (i < tabs.size() - 1) {
            items.push_back(separator());
        }
    }

    return hbox(std::move(items));
}

}  // namespace zazaki_git

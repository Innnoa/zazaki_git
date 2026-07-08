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
    items.push_back(text(" zazaki_git ") | bold | color(colors::kText));
    items.push_back(separator());

    for (size_t i = 0; i < tabs.size(); i++) {
        auto label = " " + tabs[i] + " ";
        Element el = text(label) | color(colors::kSubtext0);
        if (static_cast<int>(i) == tab_index) {
            el = text(label) | bold | inverted | color(colors::kBlue);
        }
        items.push_back(el);
    }

    items.push_back(separator());
    items.push_back(text(" h/l: switch  ?: help  q: quit ")
                    | color(colors::kOverlay0));

    return hbox(std::move(items)) | border;
}

}  // namespace zazaki_git

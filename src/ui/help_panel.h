#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

#include "ui/colors.h"

namespace zazaki_git {

inline ftxui::Element HelpContent() {
    using namespace ftxui;

    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> sections = {
        {"Navigation", {
            {"h",         "Previous tab"},
            {"l",         "Next tab"},
            {"Tab",       "Toggle focus between sidebar and main panel"},
            {"Shift+Tab", "Toggle focus (reverse)"},
        }},
        {"List Controls", {
            {"j / Down",  "Move cursor down"},
            {"k / Up",    "Move cursor up"},
            {"g",         "Jump to top"},
            {"G",         "Jump to bottom"},
        }},
        {"File Operations", {
            {"Space",     "Select/deselect file (Staging tab)"},
            {"s",         "Stage selected files"},
            {"u",         "Unstage selected files"},
            {"a",         "Stage all files"},
            {"c",         "Create commit (Staging tab)"},
        }},
        {"Branch Operations", {
            {"Enter",     "Switch to selected branch"},
            {"n",         "Create new branch"},
            {"d",         "Delete selected branch"},
        }},
        {"Log Operations", {
            {"Enter",     "View commit details"},
            {"n",         "Next page"},
            {"p",         "Previous page"},
        }},
        {"Stash Operations", {
            {"s",         "Save stash"},
            {"p",         "Pop stash"},
            {"d",         "Drop stash"},
        }},
        {"Remote Operations", {
            {"f",         "Fetch from remote"},
            {"P",         "Push to remote"},
        }},
        {"Commit", {
            {"Ctrl+D",    "Confirm commit"},
            {"Esc",       "Cancel commit"},
        }},
        {"Global", {
            {"q",         "Quit"},
            {"?",         "Show/hide this help"},
            {"Ctrl+R",    "Refresh current view"},
        }},
    };

    Elements all;
    all.push_back(text(" zazaki_git Help ") | bold | inverted | center);
    all.push_back(separator());

    for (auto& section : sections) {
        all.push_back(text(" " + section.first) | bold | color(colors::kBlue));
            all.push_back(separator() | dim);
        for (auto& binding : section.second) {
            auto key = text(" " + binding.first) | bold | color(colors::kYellow);
            auto desc = text("  " + binding.second) | dim;
            all.push_back(hbox({key, desc}));
        }
        all.push_back(separatorEmpty());
    }

    return vbox(std::move(all)) | border | vscroll_indicator;
}

inline ftxui::Component HelpPanel() {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        return HelpContent();
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        if (event == Event::Character('q') || event == Event::Escape) {
            return true;
        }
        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

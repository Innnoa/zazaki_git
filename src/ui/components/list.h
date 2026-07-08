#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace zazaki_git {

inline ftxui::Component ScrollableList(std::vector<std::string>* entries,
                                       int* selected,
                                       int* scroll_offset,
                                       int visible_lines = 10) {
    using namespace ftxui;

    auto menu = Menu(entries, selected, MenuOption::Vertical());

    return Renderer(menu, [=] {
        int n = static_cast<int>(entries->size());

        if (*selected < 0) *selected = 0;
        if (*selected >= n) *selected = n - 1;

        if (*scroll_offset < 0) *scroll_offset = 0;
        int max_offset = std::max(0, n - visible_lines);
        if (*scroll_offset > max_offset) *scroll_offset = max_offset;

        if (*selected < *scroll_offset)
            *scroll_offset = *selected;
        if (*selected >= *scroll_offset + visible_lines)
            *scroll_offset = *selected - visible_lines + 1;

        Elements items;
        int end = std::min(*scroll_offset + visible_lines, n);

        if (n == 0) {
            return text(" (empty) ") | dim | center;
        }

        for (int i = *scroll_offset; i < end; i++) {
            auto label = (*entries)[i];
            if (label.empty()) label = " ";
            auto element = text(label);
            if (i == *selected) {
                element = element | bold | inverted;
            }
            items.push_back(element);
        }

        return vbox(std::move(items));
    });
}

}  // namespace zazaki_git

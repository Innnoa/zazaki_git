#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "git/diff.h"
#include "ui/colors.h"

namespace zazaki_git {

struct DiffPanelState {
    int scroll_offset = 0;
    int visible_lines = 20;
};

inline ftxui::Component DiffPanel(GitRepo* repo,
                                   DiffPanelState* state,
                                   std::string* title,
                                   std::vector<DiffLine>* lines) {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        Elements elements;

        if (!title->empty()) {
            elements.push_back(text(" " + *title + " ") | bold | inverted);
            elements.push_back(separator());
        }

        int n = static_cast<int>(lines->size());
        if (n == 0) {
            elements.push_back(text(" No diff content ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        if (state->scroll_offset < 0) state->scroll_offset = 0;
        int max_offset = std::max(0, n - state->visible_lines);
        if (state->scroll_offset > max_offset)
            state->scroll_offset = max_offset;

        int render_end =
            std::min(state->scroll_offset + state->visible_lines, n);

        auto green = color(colors::kGreen);
        auto red = color(colors::kRed);
        auto cyan = color(colors::kSky);
        auto yellow = color(colors::kPeach);
        auto dim_ctx = color(colors::kOverlay0);

        for (int i = state->scroll_offset; i < render_end; i++) {
            auto& line = (*lines)[i];
            Element el;

            switch (line.origin) {
                case DiffLine::Origin::Addition:
                    el = text("+ " + line.content) | green;
                    break;
                case DiffLine::Origin::Deletion:
                    el = text("- " + line.content) | red;
                    break;
                case DiffLine::Origin::FileHeader:
                    el = text(line.content) | yellow | bold;
                    break;
                case DiffLine::Origin::HunkHeader:
                    el = text(line.content) | cyan;
                    break;
                case DiffLine::Origin::Binary:
                    el = text("[binary] " + line.content) | dim;
                    break;
                default:
                    el = text("  " + line.content) | dim_ctx;
                    break;
            }

            elements.push_back(el);
        }

        for (int i = render_end;
             i < state->scroll_offset + state->visible_lines; i++) {
            elements.push_back(text(" "));
        }

        std::string scroll_info = " " +
            std::to_string(state->scroll_offset + 1) + "-" +
            std::to_string(std::min(state->scroll_offset + state->visible_lines, n)) +
            " / " + std::to_string(n) + " ";
        elements.push_back(separator());
        elements.push_back(text(scroll_info) | dim | center);
        elements.push_back(
            text(" j/k: scroll  PgUp/PgDn: page  q: back ") | dim);

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        int n = static_cast<int>(lines->size());

        if (event == Event::Character('q') || event == Event::Escape) {
            return true;
        }

        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (state->scroll_offset < std::max(0, n - 1))
                state->scroll_offset++;
            return true;
        }

        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (state->scroll_offset > 0)
                state->scroll_offset--;
            return true;
        }

        if (event == Event::PageDown) {
            state->scroll_offset = std::min(
                state->scroll_offset + state->visible_lines,
                std::max(0, n - 1));
            return true;
        }

        if (event == Event::PageUp) {
            state->scroll_offset = std::max(
                state->scroll_offset - state->visible_lines, 0);
            return true;
        }

        if (event == Event::Character('g')) {
            state->scroll_offset = 0;
            return true;
        }

        if (event == Event::Character('G')) {
            state->scroll_offset = std::max(0, n - 1);
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

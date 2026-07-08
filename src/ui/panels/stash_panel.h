#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "git/stash.h"
#include "ui/colors.h"

namespace zazaki_git {

struct StashPanelState {
    int highlight = 0;
    int scroll_offset = 0;
    std::string status_text;
};

inline ftxui::Component StashPanel(GitRepo* repo,
                                   StashPanelState* state,
                                   int visible_lines = 20) {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        Elements elements;
        elements.push_back(text(" Stash ") | bold | inverted);
        elements.push_back(separator());

        if (!repo->is_open()) {
            elements.push_back(text(" Repository not open ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        auto result = stash_list(repo->raw());
        if (result.is_err()) {
            elements.push_back(
                text(" Error: " + result.error().message()) |
                color(colors::kRed));
            return vbox(std::move(elements)) | border;
        }

        auto& stashes = result.value();

        if (stashes.empty()) {
            elements.push_back(text(" No stashes ") | dim | center);
        } else {
            int n = static_cast<int>(stashes.size());
            if (state->highlight < 0) state->highlight = 0;
            if (state->highlight >= n) state->highlight = n - 1;

            if (state->scroll_offset < 0) state->scroll_offset = 0;
            int max_offset = std::max(0, n - visible_lines);
            if (state->scroll_offset > max_offset)
                state->scroll_offset = max_offset;
            if (state->highlight < state->scroll_offset)
                state->scroll_offset = state->highlight;
            if (state->highlight >= state->scroll_offset + visible_lines)
                state->scroll_offset = state->highlight - visible_lines + 1;

            int render_end =
                std::min(state->scroll_offset + visible_lines, n);

            for (int i = state->scroll_offset; i < render_end; i++) {
                auto& s = stashes[i];
                std::string label =
                    "stash@{" + std::to_string(s.index) + "}: " + s.message;

                auto line = text(label);
                if (i == state->highlight) {
                    line = line | inverted;
                }
                elements.push_back(line);
            }

            for (int i = render_end;
                 i < state->scroll_offset + visible_lines; i++) {
                elements.push_back(text(" "));
            }
        }

        if (state->status_text.empty()) return vbox(std::move(elements)) | border | vscroll_indicator;

        elements.push_back(separator());
        elements.push_back(
            text(" " + state->status_text) | dim);

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        if (!repo->is_open()) return false;

        auto result = stash_list(repo->raw());
        if (result.is_err()) return false;

        auto& stashes = result.value();
        int n = static_cast<int>(stashes.size());

        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (state->highlight < n - 1) state->highlight++;
            state->status_text.clear();
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (state->highlight > 0) state->highlight--;
            state->status_text.clear();
            return true;
        }

        if (event == Event::Character('s')) {
            auto r = stash_push(repo->raw());
            if (r.is_err()) {
                state->status_text = r.error().message();
            } else {
                state->status_text = "Changes stashed";
            }
            return true;
        }

        if (event == Event::Character('p')) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& s = stashes[state->highlight];
                auto r = stash_pop(repo->raw(), s.index);
                if (r.is_err()) {
                    state->status_text = r.error().message();
                } else {
                    state->status_text =
                        "Applied stash@{" + std::to_string(s.index) + "}";
                }
            }
            return true;
        }

        if (event == Event::Character('d')) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& s = stashes[state->highlight];
                auto r = stash_drop(repo->raw(), s.index);
                if (r.is_err()) {
                    state->status_text = r.error().message();
                } else {
                    state->status_text =
                        "Dropped stash@{" + std::to_string(s.index) + "}";
                }
            }
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

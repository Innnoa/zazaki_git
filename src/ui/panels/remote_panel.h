#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "git/remote.h"
#include "ui/colors.h"

namespace zazaki_git {

struct RemotePanelState {
    int highlight = 0;
    int scroll_offset = 0;
    std::string status_text;
};

inline ftxui::Component RemotePanel(GitRepo* repo,
                                    RemotePanelState* state,
                                    int visible_lines = 20) {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        Elements elements;
        elements.push_back(text(" Remote ") | bold | inverted);
        elements.push_back(separator());

        if (!repo->is_open()) {
            elements.push_back(text(" Repository not open ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        auto result = remote_list(repo->raw());
        if (result.is_err()) {
            elements.push_back(
                text(" Error: " + result.error().message()) |
                color(colors::kRed));
            return vbox(std::move(elements)) | border;
        }

        auto& remotes = result.value();

        if (remotes.empty()) {
            elements.push_back(text(" No remotes ") | dim | center);
        } else {
            int n = static_cast<int>(remotes.size());
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
                auto& r = remotes[i];

                auto name_line = text(" " + r.name) | bold;
                if (i == state->highlight) {
                    name_line = name_line | inverted;
                }
                elements.push_back(name_line);

                auto url_line = text("     " + r.url) | dim;
                if (i == state->highlight) {
                    url_line = url_line | inverted;
                }
                elements.push_back(url_line);
            }

            for (int i = render_end;
                 i < state->scroll_offset + visible_lines; i++) {
                elements.push_back(text(" "));
            }
        }

        if (!state->status_text.empty()) {
            elements.push_back(separator());
            elements.push_back(
                text(" " + state->status_text) | dim);
        }

        elements.push_back(separator());
        elements.push_back(
            text(" f: fetch  P: push ") | dim);

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        if (!repo->is_open()) return false;

        auto result = remote_list(repo->raw());
        if (result.is_err()) return false;

        auto& remotes = result.value();
        int n = static_cast<int>(remotes.size());

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

        if (event == Event::Character('f')) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& r = remotes[state->highlight];
                auto fetch_result = remote_fetch(repo->raw(), r.name);
                if (fetch_result.is_err()) {
                    state->status_text = fetch_result.error().message();
                } else {
                    state->status_text = "Fetched " + r.name;
                }
            }
            return true;
        }

        if (event == Event::Character('P')) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& r = remotes[state->highlight];
                auto push_result = remote_push(repo->raw(), r.name);
                if (push_result.is_err()) {
                    state->status_text = push_result.error().message();
                } else {
                    state->status_text = "Pushed to " + r.name;
                }
            }
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "git/log.h"
#include "git/diff.h"
#include "ui/colors.h"

namespace zazaki_git {

struct LogPanelState {
    int highlight = 0;
    int scroll_offset = 0;
    size_t page = 0;
    static constexpr size_t kPageSize = 50;
    std::string detail_message;
    bool show_detail = false;
};

inline ftxui::Component LogPanel(GitRepo* repo,
                                  LogPanelState* state,
                                  int visible_lines = 20,
                                  std::function<void(const std::string&, std::vector<DiffLine>)> show_diff_fn = {}) {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        Elements elements;
        elements.push_back(text(" Log ") | bold | inverted);
        elements.push_back(separator());

        if (!repo->is_open()) {
            elements.push_back(text(" Repository not open ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        if (state->show_detail && !state->detail_message.empty()) {
            elements.push_back(text(" Commit Details ") | bold);
            elements.push_back(separator());
            elements.push_back(paragraph(state->detail_message));
            elements.push_back(separator());
            elements.push_back(
                text(" [Esc] to close ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        size_t skip = state->page * LogPanelState::kPageSize;
        auto result = log_list(repo->raw(), LogPanelState::kPageSize, skip);
        if (result.is_err()) {
            elements.push_back(
                text(" Error: " + result.error().message()) |
                color(colors::kRed));
            return vbox(std::move(elements)) | border;
        }

        auto& commits = result.value();

        if (commits.empty()) {
            elements.push_back(text(" No commits ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        int n = static_cast<int>(commits.size());
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
            auto& c = commits[i];
            auto short_hash = c.hash.substr(0, 7);
            auto label_text =
                short_hash + "  " + c.date + "  " + c.author;

            auto line = text(label_text) | dim;
            if (i == state->highlight) {
                line = line | inverted;
            }
            elements.push_back(line);

            auto msg = c.message;
            if (msg.size() > 60) msg = msg.substr(0, 57) + "...";
            auto msg_line = text("          " + msg);
            if (i == state->highlight) {
                msg_line = msg_line | inverted;
            }
            elements.push_back(msg_line);
        }

        for (int i = render_end; i < state->scroll_offset + visible_lines; i++) {
            elements.push_back(text(" "));
        }

        elements.push_back(separator());
        std::string page_info;
        if (state->page > 0) {
            page_info = " p: prev  ";
        }
        page_info += "page " + std::to_string(state->page + 1);
        if (n == static_cast<int>(LogPanelState::kPageSize)) {
            page_info += "  n: next";
        }
        elements.push_back(separator());
        if (n > 0) {
            elements.push_back(text(" " + page_info) | dim);
        }

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        if (!repo->is_open()) return false;

        if (state->show_detail) {
            if (event == Event::Escape ||
                event == Event::Character('q')) {
                state->show_detail = false;
                state->detail_message.clear();
                return true;
            }
            return false;
        }

        auto result = log_list(repo->raw(), LogPanelState::kPageSize,
                                state->page * LogPanelState::kPageSize);
        if (result.is_err()) return false;

        auto& commits = result.value();
        int n = static_cast<int>(commits.size());

        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (state->highlight < n - 1) state->highlight++;
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (state->highlight > 0) state->highlight--;
            return true;
        }

        if (event == Event::Character('n')) {
            if (n == static_cast<int>(LogPanelState::kPageSize)) {
                state->page++;
                state->highlight = 0;
                state->scroll_offset = 0;
            }
            return true;
        }

        if (event == Event::Character('p')) {
            if (state->page > 0) {
                state->page--;
                state->highlight = 0;
                state->scroll_offset = 0;
            }
            return true;
        }

        if (event == Event::Return) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& c = commits[state->highlight];
                state->detail_message =
                    "Commit: " + c.hash + "\n"
                    "Author: " + c.author + " <" + c.email + ">\n"
                    "Date:   " + c.date + "\n\n" +
                    c.message;
                state->show_detail = true;
            }
            return true;
        }

        if (event == Event::Character('d') && show_diff_fn) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& c = commits[state->highlight];
                auto r = diff_commit(repo->raw(), c.hash);
                if (r.is_ok()) {
                    show_diff_fn("Commit: " + c.hash.substr(0, 7) + " " + c.message,
                                 r.value());
                }
            }
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

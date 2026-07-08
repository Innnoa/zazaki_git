#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "git/repo.h"
#include "ui/colors.h"

namespace zazaki_git {

struct StagingState {
    std::set<int> selected_items;
    int highlight = 0;
    int scroll_offset = 0;
    bool commit_mode = false;
    std::string commit_message;
};

inline ftxui::Component StagingPanel(GitRepo* repo,
                                     StagingState* state,
                                     int visible_lines = 20) {
    using namespace ftxui;

    auto green = color(colors::kGreen);
    auto yellow = color(colors::kYellow);
    auto red = color(colors::kRed);

    auto renderer = Renderer([=] {
        auto result = repo->status();

        Elements elements;
        elements.push_back(text(" Staging ") | bold | inverted);
        elements.push_back(separator());

        if (result.is_err()) {
            elements.push_back(text(" Error: " + result.error().message()) | red);
            return vbox(std::move(elements)) | border;
        }

        auto& info = result.value();

        int total = static_cast<int>(
            info.unstaged.size() + info.untracked.size() +
            info.staged.size());

        if (total == 0) {
            elements.push_back(text(" Nothing to stage ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        if (state->highlight < 0)
            state->highlight = 0;
        if (state->highlight >= total)
            state->highlight = total - 1;

        if (state->scroll_offset < 0) state->scroll_offset = 0;
        int max_offset = std::max(0, total - visible_lines);
        if (state->scroll_offset > max_offset) state->scroll_offset = max_offset;
        if (state->highlight < state->scroll_offset)
            state->scroll_offset = state->highlight;
        if (state->highlight >= state->scroll_offset + visible_lines)
            state->scroll_offset = state->highlight - visible_lines + 1;

        int idx = 0;
        int render_end = state->scroll_offset + visible_lines;

        auto render_section = [&](const std::vector<FileStatus>& files,
                                  const std::string& label,
                                  Color c,
                                  const std::string& indicator) {
            if (files.empty()) return;

            if (idx < render_end && idx >= state->scroll_offset) {
                if (idx > state->scroll_offset)
                    elements.push_back(separatorEmpty());
                elements.push_back(
                    text(" " + label + ":") | bold | color(c));
            }

            for (size_t i = 0; i < files.size(); i++) {
                if (idx >= render_end) { idx++; continue; }
                if (idx < state->scroll_offset) { idx++; continue; }

                bool sel = state->selected_items.count(idx) > 0;
                auto marker = sel ? "[" + indicator + "]" : "[ ]";
                auto line = text(" " + marker + " " + files[i].path);

                if (idx == state->highlight) line = line | bold | inverted;
                line = line | color(c);
                elements.push_back(line);
                idx++;
            }
        };

        render_section(info.unstaged, "Unstaged", colors::kYellow, "*");
        render_section(info.untracked, "Untracked", colors::kRed, "+");
        render_section(info.staged, "Staged", colors::kGreen, "s");

        while (idx < render_end) {
            elements.push_back(text(" "));
            idx++;
        }

        if (!info.staged.empty()) {
            elements.push_back(separator());
            auto staged_count = std::to_string(info.staged.size());
            elements.push_back(
                text(" " + staged_count + " file(s) staged. [c] commit ") | dim);
        }

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        auto result = repo->status();
        if (result.is_err()) return false;

        auto& info = result.value();
        int total = static_cast<int>(
            info.unstaged.size() + info.untracked.size() +
            info.staged.size());

        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (state->highlight < total - 1) state->highlight++;
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (state->highlight > 0) state->highlight--;
            return true;
        }

        if (event == Event::Character(' ')) {
            if (state->selected_items.count(state->highlight)) {
                state->selected_items.erase(state->highlight);
            } else {
                state->selected_items.insert(state->highlight);
            }
            return true;
        }

        if (event == Event::Character('a')) {
            state->selected_items.clear();
            for (int i = 0; i < static_cast<int>(info.unstaged.size() + info.untracked.size()); i++) {
                int global_idx = i;
                state->selected_items.insert(global_idx);
            }
            repo->stage_all();
            state->selected_items.clear();
            return true;
        }

        if (event == Event::Character('s')) {
            if (state->selected_items.empty()) {
                int idx = state->highlight;
                if (idx < static_cast<int>(info.unstaged.size() + info.untracked.size())) {
                    std::string path;
                    if (idx < static_cast<int>(info.unstaged.size())) {
                        path = info.unstaged[idx].path;
                    } else {
                        path = info.untracked[idx - static_cast<int>(info.unstaged.size())].path;
                    }
                    repo->stage(path);
                }
            } else {
                for (auto sel_idx : state->selected_items) {
                    if (sel_idx < static_cast<int>(info.unstaged.size() + info.untracked.size())) {
                        std::string path;
                        if (sel_idx < static_cast<int>(info.unstaged.size())) {
                            path = info.unstaged[sel_idx].path;
                        } else {
                            path = info.untracked[sel_idx - static_cast<int>(info.unstaged.size())].path;
                        }
                        repo->stage(path);
                    }
                }
                state->selected_items.clear();
            }
            return true;
        }

        if (event == Event::Character('u')) {
            if (state->selected_items.empty()) {
                int staged_start = static_cast<int>(
                    info.unstaged.size() + info.untracked.size());
                int idx = state->highlight - staged_start;
                if (idx >= 0 && idx < static_cast<int>(info.staged.size())) {
                    repo->unstage(info.staged[idx].path);
                }
            } else {
                int staged_start = static_cast<int>(
                    info.unstaged.size() + info.untracked.size());
                for (auto sel_idx : state->selected_items) {
                    int idx = sel_idx - staged_start;
                    if (idx >= 0 && idx < static_cast<int>(info.staged.size())) {
                        repo->unstage(info.staged[idx].path);
                    }
                }
                state->selected_items.clear();
            }
            return true;
        }

        if (event == Event::Character('c')) {
            state->commit_mode = true;
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

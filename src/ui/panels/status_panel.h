#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "git/repo.h"
#include "git/diff.h"
#include "ui/colors.h"

namespace zazaki_git {

inline ftxui::Component StatusPanel(GitRepo* repo,
                                    int* selected,
                                    int* scroll_offset,
                                    int visible_lines = 20,
                                    std::function<void(const std::string&, std::vector<DiffLine>)> show_diff_fn = {}) {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        auto result = repo->status();

        auto green = color(colors::kGreen);
        auto yellow = color(colors::kYellow);
        auto red = color(colors::kRed);
        auto cyan = color(colors::kTeal);

        Elements elements;
        elements.push_back(text(" Status ") | bold | inverted);
        elements.push_back(separator());

        if (result.is_err()) {
            elements.push_back(text(" Error: " + result.error().message()) | red);
            return vbox(std::move(elements)) | border;
        }

        auto& info = result.value();

        if (!info.has_changes()) {
            elements.push_back(text(" Working tree clean ") | green | center);
            return vbox(std::move(elements)) | border;
        }

        int total_items = static_cast<int>(
            info.staged.size() + info.unstaged.size() + info.untracked.size());

        if (total_items > 0) {
            if (*selected < 0) *selected = 0;
            if (*selected >= total_items) *selected = total_items - 1;

            if (*scroll_offset < 0) *scroll_offset = 0;
            int max_offset = std::max(0, total_items - visible_lines);
            if (*scroll_offset > max_offset) *scroll_offset = max_offset;
            if (*selected < *scroll_offset) *scroll_offset = *selected;
            if (*selected >= *scroll_offset + visible_lines)
                *scroll_offset = *selected - visible_lines + 1;
        }

        int idx = 0;
        int render_end = *scroll_offset + visible_lines;

        auto render_list = [&](const std::vector<FileStatus>& files,
                               const std::string& label,
                               Color c) {
            if (files.empty()) return;

            if (idx < render_end && idx >= *scroll_offset) {
                if (idx > *scroll_offset) elements.push_back(separatorEmpty());
                elements.push_back(text(" " + label + ":") | bold | color(c));
            }

            for (auto& f : files) {
                if (idx >= render_end) { idx++; continue; }
                if (idx < *scroll_offset) { idx++; continue; }

                auto line = text("  " + f.path);
                if (idx == *selected) line = line | bold | inverted;
                line = line | color(c);
                elements.push_back(line);
                idx++;
            }
        };

        render_list(info.staged, "Staged", colors::kGreen);
        render_list(info.unstaged, "Unstaged", colors::kYellow);
        render_list(info.untracked, "Untracked", colors::kRed);

        while (idx < render_end) {
            elements.push_back(text(" "));
            idx++;
        }

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        auto result = repo->status();
        if (result.is_err()) return false;

        int total = static_cast<int>(
            result->staged.size() + result->unstaged.size() + result->untracked.size());

        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (*selected < total - 1) (*selected)++;
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (*selected > 0) (*selected)--;
            return true;
        }
        if (event == Event::Character('g')) {
            *selected = 0;
            return true;
        }
        if (event == Event::Character('G')) {
            *selected = std::max(0, total - 1);
            return true;
        }
        if (event == Event::Character('d') && show_diff_fn && total > 0) {
            int idx = 0;
            auto match = [&](const std::vector<FileStatus>& files,
                             const std::string& label) -> bool {
                for (auto& f : files) {
                    if (idx == *selected) {
                        auto r = diff_index_to_workdir(repo->raw(), f.path);
                        if (r.is_ok()) {
                            show_diff_fn(label + ": " + f.path, r.value());
                        }
                        return true;
                    }
                    idx++;
                }
                return false;
            };
            if (match(result->staged, "Staged")) return true;
            if (match(result->unstaged, "Unstaged")) return true;
            if (match(result->untracked, "Untracked")) return true;
            return true;
        }
        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

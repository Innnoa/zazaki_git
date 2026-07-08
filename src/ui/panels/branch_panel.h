#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "git/branch.h"
#include "ui/colors.h"

namespace zazaki_git {

struct BranchPanelState {
    int highlight = 0;
    int scroll_offset = 0;
    bool create_mode = false;
    std::string new_branch_name;
    std::string status_text;
};

inline ftxui::Component BranchPanel(GitRepo* repo,
                                    BranchPanelState* state,
                                    int visible_lines = 20) {
    using namespace ftxui;

    auto renderer = Renderer([=] {
        Elements elements;
        elements.push_back(text(" Branches ") | bold | inverted);
        elements.push_back(separator());

        if (!repo->is_open()) {
            elements.push_back(text(" Repository not open ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        auto result = branch_list(repo->raw());
        if (result.is_err()) {
            elements.push_back(
                text(" Error: " + result.error().message()) |
                color(colors::kRed));
            return vbox(std::move(elements)) | border;
        }

        auto& branches = result.value();

        if (branches.empty()) {
            elements.push_back(text(" No branches ") | dim | center);
            return vbox(std::move(elements)) | border;
        }

        int n = static_cast<int>(branches.size());
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
            auto& b = branches[i];
            std::string prefix = b.is_current ? "* " : "  ";
            auto label = prefix + b.name;

            auto line = text(label);
            if (b.is_current) {
                line = line | color(colors::kGreen) | bold;
            }
            if (i == state->highlight) {
                line = line | inverted;
            }
            elements.push_back(line);
        }

        for (int i = render_end; i < state->scroll_offset + visible_lines; i++) {
            elements.push_back(text(" "));
        }

        if (state->status_text.empty()) return vbox(std::move(elements)) | border;

        elements.push_back(separator());
        elements.push_back(
            text(" " + state->status_text) | dim);

        if (state->create_mode) {
            elements.push_back(separator());
            elements.push_back(
                text(" New branch: " + state->new_branch_name) | bold);
        }

        return vbox(std::move(elements)) | border | vscroll_indicator;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        if (!repo->is_open()) return false;

        if (state->create_mode) {
            if (event == Event::Escape) {
                state->create_mode = false;
                state->new_branch_name.clear();
                return true;
            }
            if (event == Event::Return) {
                if (state->new_branch_name.empty()) {
                    state->status_text = "Branch name cannot be empty";
                } else {
                    auto r = branch_create(repo->raw(), state->new_branch_name);
                    if (r.is_err()) {
                        state->status_text = r.error().message();
                    } else {
                        state->status_text =
                            "Branch '" + state->new_branch_name + "' created";
                    }
                }
                state->create_mode = false;
                state->new_branch_name.clear();
                return true;
            }
            if (event == Event::Backspace) {
                if (!state->new_branch_name.empty())
                    state->new_branch_name.pop_back();
                return true;
            }
            if (event.is_character()) {
                state->new_branch_name += event.character();
                return true;
            }
            return false;
        }

        auto result = branch_list(repo->raw());
        if (result.is_err()) return false;

        auto& branches = result.value();
        int n = static_cast<int>(branches.size());

        if (event == Event::Character('j') || event == Event::ArrowDown) {
            if (state->highlight < n - 1) state->highlight++;
            return true;
        }
        if (event == Event::Character('k') || event == Event::ArrowUp) {
            if (state->highlight > 0) state->highlight--;
            return true;
        }

        if (event == Event::Return) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& b = branches[state->highlight];
                if (!b.is_current) {
                    auto r = branch_switch_to(repo->raw(), b.name);
                    if (r.is_err()) {
                        state->status_text = r.error().message();
                    } else {
                        state->status_text = "Switched to " + b.name;
                    }
                }
            }
            return true;
        }

        if (event == Event::Character('n')) {
            state->create_mode = true;
            state->new_branch_name.clear();
            return true;
        }

        if (event == Event::Character('d')) {
            if (state->highlight >= 0 && state->highlight < n) {
                auto& b = branches[state->highlight];
                if (b.is_current) {
                    state->status_text = "Cannot delete current branch";
                } else {
                    auto r = branch_delete(repo->raw(), b.name);
                    if (r.is_err()) {
                        state->status_text = r.error().message();
                    } else {
                        state->status_text = "Deleted " + b.name;
                    }
                }
            }
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

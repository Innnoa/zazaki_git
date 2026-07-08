#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>

#include "git/repo.h"
#include "ui/colors.h"

namespace zazaki_git {

struct CommitState {
    std::string message;
    std::string status_text;
    bool active = false;
    bool success = false;
};

inline ftxui::Component CommitPanel(GitRepo* repo, CommitState* state) {
    using namespace ftxui;

    InputOption option;
    option.multiline = true;
    option.transform = [](InputState state) {
        state.element |= borderEmpty;
        if (state.is_placeholder) {
            state.element |= dim;
        }
        return state.element;
    };

    auto input = Input(&state->message, "Commit message", option);

    auto renderer = Renderer(input, [=] {
        if (!state->active && !state->success) {
            return text("") | size(ftxui::HEIGHT, ftxui::EQUAL, 0);
        }

        Elements elements;

        if (state->success) {
            elements.push_back(
                text(" Commit created successfully! ") |
                color(colors::kGreen) | bold | center);
            elements.push_back(separatorEmpty());
            elements.push_back(text(" " + state->status_text) | dim);
            elements.push_back(separator());
            elements.push_back(text(" [Esc] to close ") | dim | center);
        } else {
            elements.push_back(text(" Commit ") | bold | inverted);
            elements.push_back(separator());
            elements.push_back(text(" Message:") | bold);

            auto input_box = input->Render();
            elements.push_back(input_box);

            elements.push_back(separator());
            elements.push_back(
                text(" Ctrl+D: confirm  |  Esc: cancel ")
                | dim | center);
        }

        if (!state->status_text.empty()) {
            elements.push_back(
                text(" " + state->status_text) | color(colors::kRed));
        }

        return vbox(std::move(elements)) | border;
    });

    renderer |= CatchEvent([=](Event event) -> bool {
        if (!state->active && !state->success) return false;

        if (state->success) {
            if (event == Event::Escape ||
                event == Event::Character('q')) {
                state->success = false;
                state->message.clear();
                state->status_text.clear();
                return true;
            }
            return false;
        }

        if (event == Event::Escape) {
            state->active = false;
            state->message.clear();
            state->status_text.clear();
            return true;
        }

        if (event == Event::CtrlD) {
            if (state->message.empty()) {
                state->status_text = "Commit message cannot be empty";
                return true;
            }

            auto result = repo->commit(state->message);
            if (result.is_err()) {
                state->status_text = result.error().message();
            } else {
                state->status_text = "Commit: " + result.value();
                state->success = true;
                state->message.clear();
            }
            return true;
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

#include "git/repo.h"
#include "git/diff.h"
#include "ui/panels/status_panel.h"
#include "ui/panels/staging_panel.h"
#include "ui/panels/branch_panel.h"
#include "ui/panels/log_panel.h"
#include "ui/panels/stash_panel.h"
#include "ui/panels/remote_panel.h"
#include "ui/panels/commit_panel.h"
#include "ui/diff_panel.h"
#include "ui/help_panel.h"
#include "ui/sidebar.h"
#include "ui/main_panel.h"

namespace zazaki_git {

struct App {
    GitRepo* repo;

    int tab_index = 0;
    int sidebar_index = 0;

    bool show_help = false;
    bool show_diff = false;
    std::string diff_title;
    std::vector<DiffLine> diff_lines;
    DiffPanelState diff_state;

    int status_selected = 0;
    int status_scroll = 0;

    StagingState staging_state;
    BranchPanelState branch_state;
    LogPanelState log_state;
    StashPanelState stash_state;
    RemotePanelState remote_state;
    CommitState commit_state;

    bool running = true;
    bool sidebar_focused = true;
    std::string status_line;
};

inline ftxui::Element StatusBar(const std::string& repo_path,
                                 int tab_index,
                                 const std::string& status_line) {
    using namespace ftxui;

    std::vector<std::string> tab_names = {
        "Status", "Staging", "Branches", "Log", "Stash", "Remote"
    };

    std::string tab_label = tab_index >= 0 && tab_index < 6
        ? tab_names[tab_index] : "";

    std::string left = " " + tab_label + "  |  " + repo_path + "  ";
    std::string right = " " + status_line + " ";

    auto left_el = text(left) | dim;
    auto right_el = text(right) | dim;

    int left_size = static_cast<int>(left.size());
    int right_size = static_cast<int>(right.size());
    int total = std::max(left_size, right_size) * 2 + 2;

    auto filler = text(std::string(total > left_size + right_size ? total - left_size - right_size : 1, ' ')) | dim;

    return hbox({left_el, filler, right_el}) | border;
}

inline ftxui::Component CreateApp(GitRepo* repo) {
    using namespace ftxui;

    auto app = std::make_shared<App>();
    app->repo = repo;

    auto status_panel = StatusPanel(repo, &app->status_selected, &app->status_scroll, 20,
        [=](const std::string& title, std::vector<DiffLine> lines) {
            app->diff_title = title;
            app->diff_lines = std::move(lines);
            app->show_diff = true;
        });
    auto staging_panel = StagingPanel(repo, &app->staging_state);
    auto branch_panel = BranchPanel(repo, &app->branch_state);
    auto log_panel = LogPanel(repo, &app->log_state, 20,
        [=](const std::string& title, std::vector<DiffLine> lines) {
            app->diff_title = title;
            app->diff_lines = std::move(lines);
            app->show_diff = true;
        });
    auto stash_panel = StashPanel(repo, &app->stash_state);
    auto remote_panel = RemotePanel(repo, &app->remote_state);
    auto commit_panel = CommitPanel(repo, &app->commit_state);

    auto help_panel = HelpPanel();
    auto diff_panel = DiffPanel(repo, &app->diff_state, &app->diff_title, &app->diff_lines);

    std::vector<Component> panels = {
        status_panel,
        staging_panel,
        branch_panel,
        log_panel,
        stash_panel,
        remote_panel,
    };

    auto renderer = Renderer([=] {
        if (app->show_help) {
            return hbox({
                help_panel->Render() | flex,
            }) | border;
        }

        if (app->show_diff) {
            return hbox({
                diff_panel->Render() | flex,
            }) | border;
        }

        if (app->commit_state.active || app->commit_state.success) {
            return vbox({
                commit_panel->Render(),
            });
        }

        auto active_render = (app->tab_index >= 0 && static_cast<size_t>(app->tab_index) < panels.size())
            ? panels[app->tab_index]->Render()
            : text("") | center;

        return vbox({
            TabBar(app->tab_index),
            active_render | flex,
            separator(),
            StatusBar(repo->path(), app->tab_index, app->status_line),
        });
    });

    renderer |= CatchEvent([=](Event event) -> bool {

        if (app->show_help) {
            if (event == Event::Character('q') || event == Event::Escape ||
                event == Event::Character('?')) {
                app->show_help = false;
                return true;
            }
            return true;
        }

        if (app->show_diff) {
            if (event == Event::Character('q') || event == Event::Escape) {
                app->show_diff = false;
                app->diff_lines.clear();
                app->diff_title.clear();
                return true;
            }

            int n = static_cast<int>(app->diff_lines.size());
            if (event == Event::Character('j') || event == Event::ArrowDown) {
                if (app->diff_state.scroll_offset < std::max(0, n - 1))
                    app->diff_state.scroll_offset++;
                return true;
            }
            if (event == Event::Character('k') || event == Event::ArrowUp) {
                if (app->diff_state.scroll_offset > 0)
                    app->diff_state.scroll_offset--;
                return true;
            }
            if (event == Event::PageDown) {
                app->diff_state.scroll_offset = std::min(
                    app->diff_state.scroll_offset + app->diff_state.visible_lines,
                    std::max(0, n - 1));
                return true;
            }
            if (event == Event::PageUp) {
                app->diff_state.scroll_offset = std::max(
                    app->diff_state.scroll_offset - app->diff_state.visible_lines, 0);
                return true;
            }
            if (event == Event::Character('g')) {
                app->diff_state.scroll_offset = 0;
                return true;
            }
            if (event == Event::Character('G')) {
                app->diff_state.scroll_offset = std::max(0, n - 1);
                return true;
            }
            return true;
        }

        if (app->commit_state.active || app->commit_state.success) {
            if (event == Event::Escape) {
                if (app->commit_state.success) {
                    app->commit_state.success = false;
                    app->commit_state.message.clear();
                    app->commit_state.status_text.clear();
                } else {
                    app->commit_state.active = false;
                    app->commit_state.message.clear();
                    app->commit_state.status_text.clear();
                }
                return true;
            }
            return commit_panel->OnEvent(event);
        }

        if (event == Event::Character('q')) {
            if (app->commit_state.active) {
                app->commit_state.active = false;
                app->commit_state.message.clear();
                return true;
            }
            ftxui::App::Active()->Exit();
            return true;
        }

        if (event == Event::Character('?')) {
            app->show_help = true;
            return true;
        }

        if (event == Event::CtrlR) {
            return true;
        }

        if (event == Event::Character('c')) {
            if (app->tab_index == 1) {
                app->commit_state.active = true;
                app->commit_state.message.clear();
                app->commit_state.status_text.clear();
                return true;
            }
        }

        if (event == Event::Character('h')) {
            app->tab_index = (app->tab_index + 5) % 6;
            return true;
        }

        if (event == Event::Character('l')) {
            app->tab_index = (app->tab_index + 1) % 6;
            return true;
        }

        if (event == Event::Tab) {
            app->sidebar_focused = !app->sidebar_focused;
            return true;
        }

        if (event == Event::TabReverse) {
            app->sidebar_focused = !app->sidebar_focused;
            return true;
        }

        if (app->tab_index >= 0 && static_cast<size_t>(app->tab_index) < panels.size()) {
            return panels[app->tab_index]->OnEvent(event);
        }

        return false;
    });

    return renderer;
}

}  // namespace zazaki_git

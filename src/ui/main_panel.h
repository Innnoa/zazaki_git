#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

#include "ui/panels/status_panel.h"
#include "ui/panels/staging_panel.h"
#include "ui/panels/branch_panel.h"
#include "ui/panels/log_panel.h"
#include "ui/panels/stash_panel.h"
#include "ui/panels/remote_panel.h"

namespace zazaki_git {

inline ftxui::Component MainPanel(GitRepo* repo,
                                   int tab_index,
                                   int* status_selected,
                                   int* status_scroll,
                                   StagingState* staging_state,
                                   BranchPanelState* branch_state,
                                   LogPanelState* log_state,
                                   StashPanelState* stash_state,
                                   RemotePanelState* remote_state,
                                   CommitState* commit_state) {
    using namespace ftxui;

    auto status_panel = StatusPanel(repo, status_selected, status_scroll);
    auto staging_panel = StagingPanel(repo, staging_state);
    auto branch_panel = BranchPanel(repo, branch_state);
    auto log_panel = LogPanel(repo, log_state);
    auto stash_panel = StashPanel(repo, stash_state);
    auto remote_panel = RemotePanel(repo, remote_state);

    auto container = Container::Tab({
        status_panel,
        staging_panel,
        branch_panel,
        log_panel,
        stash_panel,
        remote_panel,
    }, &tab_index);

    return container;
}

}  // namespace zazaki_git

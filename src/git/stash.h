#pragma once

#include <git2.h>
#include <string>
#include <vector>

#include "git/repo.h"

namespace zazaki_git {

struct StashInfo {
    size_t index = 0;
    std::string message;
    std::string oid;
};

inline Result<std::vector<StashInfo>> stash_list(git_repository* repo) {
    if (!repo) return Result<std::vector<StashInfo>>::err("Repository not open");

    std::vector<StashInfo> result;

    auto stash_cb = [](size_t index, const char* message, const git_oid* /*stash_id*/, void* payload) -> int {
        auto* vec = static_cast<std::vector<StashInfo>*>(payload);
        StashInfo info;
        info.index = index;
        info.message = message ? message : "";
        vec->push_back(std::move(info));
        return 0;
    };

    int err = git_stash_foreach(repo, stash_cb, &result);
    if (err != 0) {
        return Result<std::vector<StashInfo>>::from_git_error(err, "Failed to list stashes");
    }

    return Result<std::vector<StashInfo>>::ok(std::move(result));
}

inline Result<void> stash_push(git_repository* repo, const std::string& message = "") {
    if (!repo) return Result<void>::err("Repository not open");

    git_signature* sig = nullptr;
    int err = git_signature_default(&sig, repo);
    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to get default signature");
    }

    const char* msg = message.empty() ? nullptr : message.c_str();
    git_oid stash_oid;
    err = git_stash_save(&stash_oid, repo, sig, msg, GIT_STASH_DEFAULT);

    git_signature_free(sig);

    if (err != 0) {
        if (err == GIT_ENOTFOUND) {
            return Result<void>::err("Nothing to stash");
        }
        return Result<void>::from_git_error(err, "Failed to save stash");
    }
    return Result<void>::ok();
}

inline Result<void> stash_pop(git_repository* repo, size_t index = 0) {
    if (!repo) return Result<void>::err("Repository not open");

    git_stash_apply_options apply_opts = GIT_STASH_APPLY_OPTIONS_INIT;
    int err = git_stash_apply(repo, index, &apply_opts);
    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to apply stash at index " + std::to_string(index));
    }

    err = git_stash_drop(repo, index);
    if (err != 0) {
        return Result<void>::from_git_error(err, "Stash applied but failed to drop at index " + std::to_string(index));
    }
    return Result<void>::ok();
}

inline Result<void> stash_drop(git_repository* repo, size_t index = 0) {
    if (!repo) return Result<void>::err("Repository not open");

    int err = git_stash_drop(repo, index);
    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to drop stash at index " + std::to_string(index));
    }
    return Result<void>::ok();
}

}  // namespace zazaki_git

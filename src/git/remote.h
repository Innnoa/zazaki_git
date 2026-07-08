#pragma once

#include <git2.h>
#include <string>
#include <vector>

#include "git/repo.h"

namespace zazaki_git {

struct RemoteInfo {
    std::string name;
    std::string url;
    std::string push_url;
};

inline Result<std::vector<RemoteInfo>> remote_list(git_repository* repo) {
    if (!repo) return Result<std::vector<RemoteInfo>>::err("Repository not open");

    git_strarray remotes = {nullptr, 0};
    int err = git_remote_list(&remotes, repo);
    if (err != 0) {
        return Result<std::vector<RemoteInfo>>::from_git_error(err, "Failed to list remotes");
    }

    std::vector<RemoteInfo> result;
    for (size_t i = 0; i < remotes.count; i++) {
        RemoteInfo info;
        info.name = remotes.strings[i];

        git_remote* remote = nullptr;
        err = git_remote_lookup(&remote, repo, remotes.strings[i]);
        if (err == 0) {
            const char* url = git_remote_url(remote);
            if (url) info.url = url;

            const char* push_url = git_remote_pushurl(remote);
            if (push_url) info.push_url = push_url;
            else if (url) info.push_url = url;

            git_remote_free(remote);
        }

        result.push_back(std::move(info));
    }

    git_strarray_dispose(&remotes);
    return Result<std::vector<RemoteInfo>>::ok(std::move(result));
}

inline Result<void> remote_fetch(git_repository* repo, const std::string& name) {
    if (!repo) return Result<void>::err("Repository not open");

    git_remote* remote = nullptr;
    int err = git_remote_lookup(&remote, repo, name.c_str());
    if (err != 0) {
        return Result<void>::from_git_error(err, "Remote not found: " + name);
    }

    git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
    err = git_remote_fetch(remote, nullptr, &opts, nullptr);
    git_remote_free(remote);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Fetch failed for: " + name);
    }
    return Result<void>::ok();
}

inline Result<void> remote_push(git_repository* repo, const std::string& name) {
    if (!repo) return Result<void>::err("Repository not open");

    git_remote* remote = nullptr;
    int err = git_remote_lookup(&remote, repo, name.c_str());
    if (err != 0) {
        return Result<void>::from_git_error(err, "Remote not found: " + name);
    }

    git_push_options opts = GIT_PUSH_OPTIONS_INIT;
    const char* refspecs[] = {"refs/heads/*:refs/heads/*"};
    git_strarray arr = {const_cast<char**>(refspecs), 1};

    err = git_remote_push(remote, &arr, &opts);
    git_remote_free(remote);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Push failed for: " + name);
    }
    return Result<void>::ok();
}

}  // namespace zazaki_git

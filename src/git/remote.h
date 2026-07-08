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

inline Result<void> remote_exec(const std::string& repo_path,
                                 const std::vector<std::string>& args) {
    std::string cmd = "timeout 30 env GIT_TERMINAL_PROMPT=0 git -C \"" + repo_path + "\"";
    for (auto& a : args) cmd += " " + a;
    cmd += " 2>&1 </dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return Result<void>::err("Failed to execute: git");
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    int rc = pclose(pipe);
    if (rc != 0) {
        if (!output.empty() && output.back() == '\n') output.pop_back();
        return Result<void>::err(output.empty() ? "git command failed" : output);
    }
    return Result<void>::ok();
}

inline Result<void> remote_fetch(GitRepo* repo, const std::string& name) {
    if (!repo || !repo->is_open()) return Result<void>::err("Repository not open");
    return remote_exec(repo->path(), {"fetch", name});
}

inline Result<void> remote_push(GitRepo* repo, const std::string& name) {
    if (!repo || !repo->is_open()) return Result<void>::err("Repository not open");
    return remote_exec(repo->path(), {"push", name});
}

}  // namespace zazaki_git

#pragma once

#include <git2.h>
#include <ctime>
#include <string>
#include <vector>

#include "git/repo.h"

namespace zazaki_git {

inline std::string format_git_time(const git_time* gt) {
    char buf[64] = {0};
    time_t t = static_cast<time_t>(gt->time);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return std::string(buf);
}

inline Result<std::vector<CommitInfo>> log_list(git_repository* repo, size_t count = 50, size_t skip = 0) {
    if (!repo) return Result<std::vector<CommitInfo>>::err("Repository not open");

    git_revwalk* walker = nullptr;
    int err = git_revwalk_new(&walker, repo);
    if (err != 0) {
        return Result<std::vector<CommitInfo>>::from_git_error(err, "Failed to create revwalk");
    }

    err = git_revwalk_push_head(walker);
    if (err != 0) {
        git_revwalk_free(walker);
        return Result<std::vector<CommitInfo>>::from_git_error(err, "Failed to push HEAD to revwalk");
    }

    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    std::vector<CommitInfo> commits;
    git_oid oid;
    size_t idx = 0;

    while (git_revwalk_next(&oid, walker) == 0 && commits.size() < count) {
        if (idx++ < skip) continue;

        git_commit* commit = nullptr;
        err = git_commit_lookup(&commit, repo, &oid);
        if (err != 0) continue;

        CommitInfo info;
        char buf[GIT_OID_HEXSZ + 1] = {0};
        git_oid_tostr(buf, sizeof(buf), &oid);
        info.hash = buf;

        const git_signature* author = git_commit_author(commit);
        if (author) {
            info.author = author->name ? author->name : "";
            info.email = author->email ? author->email : "";
            info.date = format_git_time(&author->when);
        }

        const char* msg = git_commit_message(commit);
        if (msg) {
            info.message = msg;
            if (!info.message.empty() && info.message.back() == '\n') {
                info.message.pop_back();
            }
        }

        commits.push_back(std::move(info));
        git_commit_free(commit);
    }

    git_revwalk_free(walker);
    return Result<std::vector<CommitInfo>>::ok(std::move(commits));
}

inline Result<std::string> log_commit_message(git_repository* repo, const std::string& hash) {
    if (!repo) return Result<std::string>::err("Repository not open");

    git_oid oid;
    int err = git_oid_fromstr(&oid, hash.c_str());
    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Invalid hash: " + hash);
    }

    git_commit* commit = nullptr;
    err = git_commit_lookup(&commit, repo, &oid);
    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Commit not found: " + hash);
    }

    const char* msg = git_commit_message(commit);
    std::string result = msg ? std::string(msg) : "";
    git_commit_free(commit);

    return Result<std::string>::ok(std::move(result));
}

}  // namespace zazaki_git

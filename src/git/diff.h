#pragma once

#include <git2.h>
#include <cstring>
#include <string>
#include <vector>

#include "git/repo.h"

namespace zazaki_git {

struct DiffLine {
    enum class Origin {
        Context,
        Addition,
        Deletion,
        FileHeader,
        HunkHeader,
        Binary,
    };

    Origin origin;
    std::string content;
};

struct DiffFile {
    std::string old_path;
    std::string new_path;
    std::vector<DiffLine> lines;
};

inline DiffLine::Origin classify_diff_origin(char origin) {
    switch (origin) {
        case GIT_DIFF_LINE_CONTEXT:      return DiffLine::Origin::Context;
        case GIT_DIFF_LINE_ADDITION:     return DiffLine::Origin::Addition;
        case GIT_DIFF_LINE_DELETION:     return DiffLine::Origin::Deletion;
        case GIT_DIFF_LINE_FILE_HDR:     return DiffLine::Origin::FileHeader;
        case GIT_DIFF_LINE_HUNK_HDR:     return DiffLine::Origin::HunkHeader;
        case GIT_DIFF_LINE_BINARY:       return DiffLine::Origin::Binary;
        default:                          return DiffLine::Origin::Context;
    }
}

inline int diff_line_callback(const git_diff_delta* /*delta*/, const git_diff_hunk* /*hunk*/,
                              const git_diff_line* line, void* payload) {
    auto* lines = static_cast<std::vector<DiffLine>*>(payload);
    std::string content;
    if (line->content_len > 0) {
        content.assign(line->content, line->content_len);
        if (!content.empty() && content.back() == '\n') {
            content.pop_back();
        }
    }
    lines->push_back({classify_diff_origin(line->origin), std::move(content)});
    return 0;
}

inline int diff_file_callback(const git_diff_delta* delta, float /*progress*/, void* payload) {
    auto* files = static_cast<std::vector<DiffFile>*>(payload);
    DiffFile df;
    df.old_path = delta->old_file.path ? delta->old_file.path : "";
    df.new_path = delta->new_file.path ? delta->new_file.path : "";
    files->push_back(std::move(df));
    return 0;
}

inline Result<std::vector<DiffLine>> diff_index_to_workdir(git_repository* repo, const std::string& path = "") {
    if (!repo) return Result<std::vector<DiffLine>>::err("Repository not open");

    git_diff* diff = nullptr;
    git_index* index = nullptr;
    int err = git_repository_index(&index, repo);
    if (err != 0) {
        return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to get index");
    }

    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
    if (!path.empty()) {
        char* pathspec[1];
        char buf[4096];
        strncpy(buf, path.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        pathspec[0] = buf;
        diff_opts.pathspec.strings = pathspec;
        diff_opts.pathspec.count = 1;
    }

    err = git_diff_index_to_workdir(&diff, repo, index, &diff_opts);
    git_index_free(index);

    if (err != 0) {
        return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to create diff");
    }

    std::vector<DiffLine> lines;
    git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, diff_line_callback, &lines);
    git_diff_free(diff);

    return Result<std::vector<DiffLine>>::ok(std::move(lines));
}

inline Result<std::vector<DiffLine>> diff_head_to_index(git_repository* repo) {
    if (!repo) return Result<std::vector<DiffLine>>::err("Repository not open");

    git_diff* diff = nullptr;
    git_index* index = nullptr;
    int err = git_repository_index(&index, repo);
    if (err != 0) {
        return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to get index");
    }

    git_object* head_obj = nullptr;
    git_tree* head_tree = nullptr;
    err = git_revparse_single(&head_obj, repo, "HEAD^{tree}");
    if (err != 0) {
        git_index_free(index);
        git_tree* empty_tree = nullptr;
        git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
        err = git_diff_tree_to_index(&diff, repo, empty_tree, index, &opts);
        git_index_free(index);

        if (err != 0) {
            return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to create diff");
        }
    } else {
        err = git_tree_lookup(&head_tree, repo, git_object_id(head_obj));
        git_object_free(head_obj);

        if (err != 0) {
            git_index_free(index);
            return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to lookup HEAD tree");
        }

        git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
        err = git_diff_tree_to_index(&diff, repo, head_tree, index, &opts);
        git_tree_free(head_tree);
        git_index_free(index);

        if (err != 0) {
            return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to create diff");
        }
    }

    std::vector<DiffLine> lines;
    git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, diff_line_callback, &lines);
    git_diff_free(diff);

    return Result<std::vector<DiffLine>>::ok(std::move(lines));
}

inline Result<std::vector<DiffLine>> diff_commit(git_repository* repo, const std::string& hash) {
    if (!repo) return Result<std::vector<DiffLine>>::err("Repository not open");

    git_oid oid;
    int err = git_oid_fromstr(&oid, hash.c_str());
    if (err != 0) {
        return Result<std::vector<DiffLine>>::from_git_error(err, "Invalid hash: " + hash);
    }

    git_commit* commit = nullptr;
    err = git_commit_lookup(&commit, repo, &oid);
    if (err != 0) {
        return Result<std::vector<DiffLine>>::from_git_error(err, "Commit not found: " + hash);
    }

    git_tree* commit_tree = nullptr;
    err = git_commit_tree(&commit_tree, commit);
    if (err != 0) {
        git_commit_free(commit);
        return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to get commit tree");
    }

    git_diff* diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;

    unsigned int parent_count = git_commit_parentcount(commit);
    if (parent_count > 0) {
        git_commit* parent_commit = nullptr;
        git_commit_parent(&parent_commit, commit, 0);
        git_tree* parent_tree = nullptr;
        git_commit_tree(&parent_tree, parent_commit);
        err = git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, &opts);
        git_tree_free(parent_tree);
        git_commit_free(parent_commit);
    } else {
        err = git_diff_tree_to_tree(&diff, repo, nullptr, commit_tree, &opts);
    }

    git_tree_free(commit_tree);
    git_commit_free(commit);

    if (err != 0) {
        return Result<std::vector<DiffLine>>::from_git_error(err, "Failed to create diff");
    }

    std::vector<DiffLine> lines;
    git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, diff_line_callback, &lines);
    git_diff_free(diff);

    return Result<std::vector<DiffLine>>::ok(std::move(lines));
}

inline Result<std::string> diff_stat(git_repository* repo) {
    if (!repo) return Result<std::string>::err("Repository not open");

    git_diff* diff = nullptr;
    git_index* index = nullptr;
    int err = git_repository_index(&index, repo);
    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to get index");
    }

    // Diff index to workdir for stat information
    err = git_diff_index_to_workdir(&diff, repo, index, nullptr);
    git_index_free(index);

    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to create diff");
    }

    git_diff_stats* stats = nullptr;
    err = git_diff_get_stats(&stats, diff);
    git_diff_free(diff);

    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to get diff stats");
    }

    git_buf buf = GIT_BUF_INIT;
    err = git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL, 80);
    git_diff_stats_free(stats);

    if (err != 0) {
        git_buf_dispose(&buf);
        return Result<std::string>::from_git_error(err, "Failed to format diff stats");
    }

    std::string result(buf.ptr, buf.size);
    git_buf_dispose(&buf);
    return Result<std::string>::ok(std::move(result));
}

}  // namespace zazaki_git

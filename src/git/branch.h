#pragma once

#include <git2.h>
#include <string>
#include <vector>

#include "git/repo.h"

namespace zazaki_git {

struct BranchInfo {
    std::string name;
    std::string oid;
    bool is_current = false;
    bool is_local = true;
};

inline Result<std::vector<BranchInfo>> branch_list(git_repository* repo) {
    if (!repo) return Result<std::vector<BranchInfo>>::err("Repository not open");

    git_branch_iterator* iter = nullptr;
    int err = git_branch_iterator_new(&iter, repo, GIT_BRANCH_LOCAL);
    if (err != 0) {
        return Result<std::vector<BranchInfo>>::from_git_error(err, "Failed to create branch iterator");
    }

    std::vector<BranchInfo> branches;
    git_reference* ref = nullptr;
    git_branch_t type;

    while (git_branch_next(&ref, &type, iter) == 0) {
        BranchInfo info;
        const char* name = git_reference_shorthand(ref);
        if (name) info.name = name;

        const git_oid* oid = git_reference_target(ref);
        if (oid) {
            char buf[GIT_OID_HEXSZ + 1] = {0};
            git_oid_tostr(buf, sizeof(buf), oid);
            info.oid = buf;
        }

        info.is_current = git_branch_is_head(ref) == 1;
        info.is_local = (type == GIT_BRANCH_LOCAL);
        branches.push_back(std::move(info));
        git_reference_free(ref);
    }

    git_branch_iterator_free(iter);
    return Result<std::vector<BranchInfo>>::ok(std::move(branches));
}

inline Result<void> branch_create(git_repository* repo, const std::string& name) {
    if (!repo) return Result<void>::err("Repository not open");

    git_commit* head_commit = nullptr;
    git_object* head_obj = nullptr;
    int err = git_revparse_single(&head_obj, repo, "HEAD");
    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to resolve HEAD");
    }

    git_reference* new_ref = nullptr;
    err = git_branch_create(&new_ref, repo, name.c_str(),
                            reinterpret_cast<git_commit*>(head_obj), 0);
    git_object_free(head_obj);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to create branch: " + name);
    }
    git_reference_free(new_ref);
    return Result<void>::ok();
}

inline Result<void> branch_switch_to(git_repository* repo, const std::string& name) {
    if (!repo) return Result<void>::err("Repository not open");

    git_object* target = nullptr;
    std::string refname = "refs/heads/" + name;
    int err = git_revparse_single(&target, repo, refname.c_str());
    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to resolve branch: " + name);
    }

    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    err = git_checkout_tree(repo, target, &opts);
    if (err != 0) {
        git_object_free(target);
        return Result<void>::from_git_error(err, "Failed to checkout branch: " + name);
    }

    err = git_repository_set_head(repo, refname.c_str());
    git_object_free(target);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to set HEAD to branch: " + name);
    }
    return Result<void>::ok();
}

inline Result<void> branch_delete(git_repository* repo, const std::string& name) {
    if (!repo) return Result<void>::err("Repository not open");

    git_reference* ref = nullptr;
    std::string refname = "refs/heads/" + name;
    int err = git_reference_lookup(&ref, repo, refname.c_str());
    if (err != 0) {
        return Result<void>::from_git_error(err, "Branch not found: " + name);
    }

    if (git_branch_is_head(ref) == 1) {
        git_reference_free(ref);
        return Result<void>::err("Cannot delete current branch: " + name);
    }

    err = git_branch_delete(ref);
    git_reference_free(ref);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to delete branch: " + name);
    }
    return Result<void>::ok();
}

}  // namespace zazaki_git

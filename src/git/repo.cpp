#include "git/repo.h"

#include <cstring>
#include <ctime>

namespace zazaki_git {

GitRepo::~GitRepo() {
    close();
}

GitRepo::GitRepo(GitRepo&& other) noexcept
    : repo_(other.repo_), path_(std::move(other.path_)) {
    other.repo_ = nullptr;
}

GitRepo& GitRepo::operator=(GitRepo&& other) noexcept {
    if (this != &other) {
        close();
        repo_ = other.repo_;
        path_ = std::move(other.path_);
        other.repo_ = nullptr;
    }
    return *this;
}

Result<void> GitRepo::open(const std::string& path) {
    close();

    git_libgit2_init();

    path_ = path;
    int err = git_repository_open(&repo_, path.c_str());
    if (err != 0) {
        repo_ = nullptr;
        return Result<void>::from_git_error(err, "Failed to open repository");
    }
    return Result<void>::ok();
}

void GitRepo::close() {
    if (repo_) {
        git_repository_free(repo_);
        repo_ = nullptr;
    }
    path_.clear();
}

void GitRepo::init_index() {
    git_libgit2_init();
}

Result<git_index*> GitRepo::ensure_index() {
    git_index* index = nullptr;
    int err = git_repository_index(&index, repo_);
    if (err != 0) {
        return Result<git_index*>::from_git_error(err, "Failed to get index");
    }
    return Result<git_index*>::ok(index);
}

FileStatus::Status GitRepo::classify_delta(git_delta_t delta) {
    switch (delta) {
        case GIT_DELTA_UNTRACKED: return FileStatus::Status::NewUntracked;
        case GIT_DELTA_ADDED:     return FileStatus::Status::NewInIndex;
        case GIT_DELTA_MODIFIED:  return FileStatus::Status::ModifiedInIndex;
        case GIT_DELTA_DELETED:   return FileStatus::Status::DeletedInIndex;
        case GIT_DELTA_RENAMED:   return FileStatus::Status::RenamedInIndex;
        case GIT_DELTA_TYPECHANGE: return FileStatus::Status::TypeChangeInIndex;
        case GIT_DELTA_CONFLICTED: return FileStatus::Status::Conflicted;
        case GIT_DELTA_IGNORED:   return FileStatus::Status::Ignored;
        default:                  return FileStatus::Status::ModifiedInWorkdir;
    }
}

Result<StatusInfo> GitRepo::status() {
    if (!repo_) return Result<StatusInfo>::err("Repository not open");

    git_status_options opts = GIT_STATUS_OPTIONS_INIT;
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                 GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                 GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

    git_status_list* status_list = nullptr;
    int err = git_status_list_new(&status_list, repo_, &opts);
    if (err != 0) {
        return Result<StatusInfo>::from_git_error(err, "Failed to get status");
    }

    StatusInfo info;
    size_t count = git_status_list_entrycount(status_list);

    for (size_t i = 0; i < count; i++) {
        const git_status_entry* entry = git_status_byindex(status_list, i);

        FileStatus fs;
        if (entry->index_to_workdir) {
            fs.path = entry->index_to_workdir->new_file.path;
            fs.status = FileStatus::Status::NewUntracked;
        } else if (entry->head_to_index) {
            fs.path = entry->head_to_index->new_file.path;
            fs.status = classify_delta(entry->head_to_index->status);
            fs.is_staged = true;
        }

        if (entry->status & GIT_STATUS_INDEX_NEW ||
            entry->status & GIT_STATUS_INDEX_MODIFIED ||
            entry->status & GIT_STATUS_INDEX_DELETED ||
            entry->status & GIT_STATUS_INDEX_RENAMED ||
            entry->status & GIT_STATUS_INDEX_TYPECHANGE) {
            fs.is_staged = true;
        }

        if (entry->status & GIT_STATUS_WT_NEW) {
            fs.path = entry->index_to_workdir ?
                entry->index_to_workdir->new_file.path : fs.path;
            fs.status = FileStatus::Status::NewUntracked;
            info.untracked.push_back(fs);
        } else if (entry->status & GIT_STATUS_INDEX_NEW ||
                   entry->status & GIT_STATUS_INDEX_MODIFIED ||
                   entry->status & GIT_STATUS_INDEX_DELETED ||
                   entry->status & GIT_STATUS_INDEX_RENAMED ||
                   entry->status & GIT_STATUS_INDEX_TYPECHANGE) {
            fs.is_staged = true;
            if (entry->status & GIT_STATUS_WT_MODIFIED ||
                entry->status & GIT_STATUS_WT_DELETED) {
                info.staged.push_back(fs);
                FileStatus fs_wt = fs;
                fs_wt.is_staged = false;
                if (entry->status & GIT_STATUS_WT_MODIFIED) {
                    fs_wt.status = FileStatus::Status::ModifiedInWorkdir;
                } else if (entry->status & GIT_STATUS_WT_DELETED) {
                    fs_wt.status = FileStatus::Status::DeletedInWorkdir;
                }
                info.unstaged.push_back(fs_wt);
            } else {
                info.staged.push_back(fs);
            }
        } else if (entry->status & GIT_STATUS_WT_MODIFIED ||
                   entry->status & GIT_STATUS_WT_DELETED ||
                   entry->status & GIT_STATUS_WT_TYPECHANGE ||
                   entry->status & GIT_STATUS_WT_RENAMED) {
            fs.is_staged = false;
            info.unstaged.push_back(fs);
        } else if (entry->status & GIT_STATUS_IGNORED) {
            continue;
        }
    }

    git_status_list_free(status_list);
    return Result<StatusInfo>::ok(std::move(info));
}

Result<void> GitRepo::stage(const std::string& path) {
    if (!repo_) return Result<void>::err("Repository not open");

    auto idx_result = ensure_index();
    if (idx_result.is_err()) return Result<void>::err(idx_result.error().message());

    git_index* index = idx_result.value();
    int err = git_index_add_bypath(index, path.c_str());
    if (err == 0) {
        err = git_index_write(index);
    }
    git_index_free(index);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to stage file: " + path);
    }
    return Result<void>::ok();
}

Result<void> GitRepo::stage_all() {
    if (!repo_) return Result<void>::err("Repository not open");

    auto idx_result = ensure_index();
    if (idx_result.is_err()) return Result<void>::err(idx_result.error().message());

    git_index* index = idx_result.value();

    git_strarray arr = {nullptr, 0};
    int err = git_index_add_all(index, &arr, GIT_INDEX_ADD_DEFAULT, nullptr, nullptr);
    git_strarray_dispose(&arr);
    if (err == 0) {
        err = git_index_write(index);
    }
    git_index_free(index);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to stage all");
    }
    return Result<void>::ok();
}

Result<void> GitRepo::unstage(const std::string& path) {
    if (!repo_) return Result<void>::err("Repository not open");

    git_object* head_obj = nullptr;
    int err = git_revparse_single(&head_obj, repo_, "HEAD");
    if (err != 0) {
        auto idx_result = ensure_index();
        if (idx_result.is_err()) return Result<void>::err(idx_result.error().message());
        git_index* index = idx_result.value();
        err = git_index_remove_bypath(index, path.c_str());
        git_index_write(index);
        git_index_free(index);
        if (err != 0) {
            return Result<void>::from_git_error(err, "Failed to unstage file: " + path);
        }
        return Result<void>::ok();
    }

    git_tree* head_tree = nullptr;
    err = git_commit_tree(&head_tree, reinterpret_cast<git_commit*>(head_obj));
    git_object_free(head_obj);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to get HEAD tree");
    }

    auto idx_result = ensure_index();
    if (idx_result.is_err()) return Result<void>::err(idx_result.error().message());

    git_index* index = idx_result.value();
    err = git_index_read_tree(index, head_tree);
    git_tree_free(head_tree);

    if (err == 0) {
        err = git_index_write(index);
    }
    git_index_free(index);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to unstage file: " + path);
    }
    return Result<void>::ok();
}

Result<void> GitRepo::unstage_all() {
    if (!repo_) return Result<void>::err("Repository not open");

    auto idx_result = ensure_index();
    if (idx_result.is_err()) return Result<void>::err(idx_result.error().message());

    git_index* index = idx_result.value();

    git_object* head_obj = nullptr;
    int err = git_revparse_single(&head_obj, repo_, "HEAD");

    if (err == 0) {
        git_tree* head_tree = nullptr;
        err = git_commit_tree(&head_tree, reinterpret_cast<git_commit*>(head_obj));
        git_object_free(head_obj);

        if (err == 0) {
            err = git_index_read_tree(index, head_tree);
            git_tree_free(head_tree);
        }
    } else {
        err = git_index_clear(index);
    }

    if (err == 0) {
        err = git_index_write(index);
    }
    git_index_free(index);

    if (err != 0) {
        return Result<void>::from_git_error(err, "Failed to unstage all");
    }
    return Result<void>::ok();
}

Result<std::string> GitRepo::commit(const std::string& message) {
    if (!repo_) return Result<std::string>::err("Repository not open");

    auto idx_result = ensure_index();
    if (idx_result.is_err()) return Result<std::string>::err(idx_result.error().message());

    git_index* index = idx_result.value();

    git_oid tree_oid;
    int err = git_index_write(index);
    if (err != 0) {
        git_index_free(index);
        return Result<std::string>::from_git_error(err, "Failed to write index");
    }

    err = git_index_write_tree(&tree_oid, index);
    git_index_free(index);

    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to write tree");
    }

    git_tree* tree = nullptr;
    err = git_tree_lookup(&tree, repo_, &tree_oid);
    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to lookup tree");
    }

    git_signature* sig = nullptr;
    err = git_signature_default(&sig, repo_);
    if (err != 0) {
        git_tree_free(tree);
        return Result<std::string>::from_git_error(err, "Failed to create signature");
    }

    git_oid parent_oid;
    git_commit* parent = nullptr;
    err = git_reference_name_to_id(&parent_oid, repo_, "HEAD");
    if (err == 0) {
        git_commit_lookup(&parent, repo_, &parent_oid);
    }

    git_oid commit_oid;
    const char* ref_target = "HEAD";
    const git_commit* parents_arr[1] = {parent};
    int parent_count = parent ? 1 : 0;

    err = git_commit_create(
        &commit_oid, repo_, ref_target,
        sig, sig, "UTF-8",
        message.c_str(), tree,
        parent_count, parent_count > 0 ? parents_arr : nullptr);

    git_signature_free(sig);
    git_tree_free(tree);
    if (parent) git_commit_free(parent);

    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to create commit");
    }

    char oid_str[GIT_OID_HEXSZ + 1] = {0};
    git_oid_tostr(oid_str, sizeof(oid_str), &commit_oid);
    return Result<std::string>::ok(std::string(oid_str));
}

Result<HeadInfo> GitRepo::head() {
    if (!repo_) return Result<HeadInfo>::err("Repository not open");

    HeadInfo info;
    int detached = git_repository_head_detached(repo_);
    info.detached = (detached == 1);

    if (detached == 1) {
        git_oid head_oid;
        int err = git_reference_name_to_id(&head_oid, repo_, "HEAD");
        if (err != 0) {
            return Result<HeadInfo>::from_git_error(err, "Failed to get HEAD oid");
        }
        char oid_str[GIT_OID_HEXSZ + 1] = {0};
        git_oid_tostr(oid_str, sizeof(oid_str), &head_oid);
        info.oid = oid_str;
        info.branch = "(detached)";
        return Result<HeadInfo>::ok(std::move(info));
    }

    git_reference* head_ref = nullptr;
    int err = git_repository_head(&head_ref, repo_);
    if (err == GIT_EUNBORNBRANCH) {
        const char* ref_name = git_reference_name(head_ref);
        if (ref_name) {
            info.branch = ref_name;
            size_t pos = info.branch.rfind('/');
            if (pos != std::string::npos) {
                info.branch = info.branch.substr(pos + 1);
            }
        }
        git_reference_free(head_ref);
        info.oid = "";
        return Result<HeadInfo>::ok(std::move(info));
    }
    if (err != 0) {
        return Result<HeadInfo>::from_git_error(err, "Failed to get HEAD");
    }

    const char* branch_name = git_reference_shorthand(head_ref);
    if (branch_name) {
        info.branch = branch_name;
    }

    const git_oid* target_oid = git_reference_target(head_ref);
    if (target_oid) {
        char oid_str[GIT_OID_HEXSZ + 1] = {0};
        git_oid_tostr(oid_str, sizeof(oid_str), target_oid);
        info.oid = oid_str;
    }

    git_reference_free(head_ref);
    return Result<HeadInfo>::ok(std::move(info));
}

Result<std::string> GitRepo::config_string(const std::string& key) {
    if (!repo_) return Result<std::string>::err("Repository not open");

    git_config* cfg = nullptr;
    int err = git_repository_config(&cfg, repo_);
    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Failed to get config");
    }

    const char* value = nullptr;
    err = git_config_get_string(&value, cfg, key.c_str());
    git_config_free(cfg);

    if (err != 0) {
        return Result<std::string>::from_git_error(err, "Config key not found: " + key);
    }

    return Result<std::string>::ok(std::string(value));
}

}  // namespace zazaki_git

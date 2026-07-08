#pragma once

#include <git2.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace zazaki_git {

class GitError {
public:
    explicit GitError(std::string msg) : msg_(std::move(msg)) {}
    const std::string& message() const { return msg_; }

private:
    std::string msg_;
};

template <typename T>
class Result {
public:
    static Result<T> ok(T value) {
        Result<T> r;
        r.value_ = std::move(value);
        return r;
    }

    static Result<T> err(std::string msg) {
        Result<T> r;
        r.error_ = GitError(std::move(msg));
        return r;
    }

    static Result<T> from_git_error(int error_code, const std::string& context = {}) {
        if (error_code == 0) {
            return err("from_git_error called with success code");
        }
        const git_error* ge = git_error_last();
        std::string msg = context.empty() ? "" : context + ": ";
        if (ge) {
            msg += ge->message;
        } else {
            msg += "git error code " + std::to_string(error_code);
        }
        return err(std::move(msg));
    }

    bool is_ok() const { return value_.has_value(); }
    bool is_err() const { return error_.has_value(); }

    T& value() { return *value_; }
    const T& value() const { return *value_; }

    T& operator*() { return *value_; }
    const T& operator*() const { return *value_; }

    T* operator->() { return &*value_; }
    const T* operator->() const { return &*value_; }

    const GitError& error() const { return *error_; }

private:
    Result() = default;
    std::optional<T> value_;
    std::optional<GitError> error_;
};

template <>
class Result<void> {
public:
    static Result<void> ok() {
        Result<void> r;
        r.ok_ = true;
        return r;
    }

    static Result<void> err(std::string msg) {
        Result<void> r;
        r.ok_ = false;
        r.error_ = GitError(std::move(msg));
        return r;
    }

    static Result<void> from_git_error(int error_code, const std::string& context = {}) {
        const git_error* ge = git_error_last();
        std::string msg = context.empty() ? "" : context + ": ";
        if (ge) {
            msg += ge->message;
        } else {
            msg += "git error code " + std::to_string(error_code);
        }
        return err(std::move(msg));
    }

    bool is_ok() const { return ok_; }
    bool is_err() const { return !ok_; }
    const GitError& error() const { return error_; }

private:
    Result() = default;
    bool ok_ = false;
    GitError error_{""};
};

struct FileStatus {
    enum class Status {
        NewUntracked,
        NewInIndex,
        ModifiedInIndex,
        DeletedInIndex,
        RenamedInIndex,
        TypeChangeInIndex,
        ModifiedInWorkdir,
        DeletedInWorkdir,
        TypeChangeInWorkdir,
        RenamedInWorkdir,
        Conflicted,
        Ignored,
    };

    std::string path;
    std::string old_path;
    Status status;
    bool is_staged = false;
};

struct StatusInfo {
    std::vector<FileStatus> staged;
    std::vector<FileStatus> unstaged;
    std::vector<FileStatus> untracked;

    [[nodiscard]] bool has_changes() const {
        return !staged.empty() || !unstaged.empty() || !untracked.empty();
    }
};

struct HeadInfo {
    std::string branch;
    bool detached = false;
    std::string tag;
    std::string oid;
};

struct CommitInfo {
    std::string hash;
    std::string author;
    std::string email;
    std::string date;
    std::string message;
    std::vector<std::string> refs;
};

class GitRepo {
public:
    GitRepo() = default;
    ~GitRepo();

    GitRepo(const GitRepo&) = delete;
    GitRepo& operator=(const GitRepo&) = delete;
    GitRepo(GitRepo&&) noexcept;
    GitRepo& operator=(GitRepo&&) noexcept;

    Result<void> open(const std::string& path);
    void close();
    [[nodiscard]] bool is_open() const { return repo_ != nullptr; }
    [[nodiscard]] std::string path() const { return path_; }

    Result<StatusInfo> status();

    Result<void> stage(const std::string& path);
    Result<void> stage_all();
    Result<void> unstage(const std::string& path);
    Result<void> unstage_all();

    Result<std::string> commit(const std::string& message);

    Result<HeadInfo> head();

    Result<std::string> config_string(const std::string& key);

    git_repository* raw() { return repo_; }

private:
    static FileStatus::Status classify_delta(git_delta_t delta);
    void init_index();
    Result<git_index*> ensure_index();

    git_repository* repo_ = nullptr;
    std::string path_;
};

}  // namespace zazaki_git

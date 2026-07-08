#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "git/branch.h"
#include "git/diff.h"
#include "git/log.h"
#include "git/remote.h"
#include "git/repo.h"
#include "git/stash.h"

namespace fs = std::filesystem;
using namespace zazaki_git;

struct ExtRepo {
    fs::path dir;
    GitRepo repo;
    static inline int counter = 0;

    ExtRepo() {
        git_libgit2_init();
        dir = fs::temp_directory_path() / ("zazaki_ext_test_" + std::to_string(counter++));
        fs::remove_all(dir);
        fs::create_directories(dir);

        auto abs = fs::absolute(dir).string();
        git_repository* raw = nullptr;
        int err = git_repository_init(&raw, abs.c_str(), false);
        INFO("repo_init at ", abs, " returned ", err);
        REQUIRE(err == 0);
        git_repository_free(raw);

        auto result = repo.open(abs);
        if (!result.is_ok()) {
            INFO("repo.open failed: ", result.error().message());
        }
        REQUIRE(result.is_ok());

        {
            fs::path f = dir / "tracked.txt";
            std::ofstream of(f);
            of << "initial content\n";
        }

        REQUIRE(repo.stage_all().is_ok());
        auto commit_result = repo.commit("initial commit");
        if (!commit_result.is_ok()) {
            INFO("commit failed: ", commit_result.error().message());
        }
        REQUIRE(commit_result.is_ok());
    }

    void write_file(const std::string& name, const std::string& content) {
        fs::path f = dir / name;
        std::ofstream of(f);
        of << content;
    }

    void append_to_file(const std::string& name, const std::string& content) {
        fs::path f = dir / name;
        std::ofstream of(f, std::ios::app);
        of << content;
    }

    ~ExtRepo() {
        repo.close();
        fs::remove_all(dir);
    }
};

TEST_CASE("branch: list branches with initial commit") {
    ExtRepo t;
    auto result = branch_list(t.repo.raw());
    REQUIRE(result.is_ok());
    REQUIRE(result->size() >= 1);
    bool found_current = false;
    for (const auto& b : *result) {
        if (b.is_current) { found_current = true; break; }
    }
    CHECK(found_current);
}

TEST_CASE("branch: create and switch branch") {
    ExtRepo t;
    auto create_result = branch_create(t.repo.raw(), "feature/test");
    REQUIRE(create_result.is_ok());

    auto list_after = branch_list(t.repo.raw());
    bool found_new = false;
    for (const auto& b : *list_after) {
        if (b.name == "feature/test") { found_new = true; break; }
    }
    CHECK(found_new);

    auto switch_result = branch_switch_to(t.repo.raw(), "feature/test");
    if (!switch_result.is_ok()) {
        MESSAGE("switch failed: ", switch_result.error().message());
    }
    CHECK(switch_result.is_ok());

    if (switch_result.is_ok()) {
        auto list_switched = branch_list(t.repo.raw());
        for (const auto& b : *list_switched) {
            if (b.name == "feature/test") {
                CHECK(b.is_current);
            }
        }
    }
}

TEST_CASE("branch: fail to delete current branch") {
    ExtRepo t;
    auto result = branch_delete(t.repo.raw(), "main");
    CHECK(result.is_err());
}

TEST_CASE("branch: create and delete non-current branch") {
    ExtRepo t;
    REQUIRE(branch_create(t.repo.raw(), "temp-branch").is_ok());

    auto del_result = branch_delete(t.repo.raw(), "temp-branch");
    CHECK(del_result.is_ok());

    auto list_after = branch_list(t.repo.raw());
    for (const auto& b : *list_after) {
        CHECK(b.name != "temp-branch");
    }
}

TEST_CASE("log: list commits after one commit") {
    ExtRepo t;
    auto result = log_list(t.repo.raw());
    REQUIRE(result.is_ok());
    REQUIRE(result->size() >= 1);
    CHECK((*result)[0].message == "initial commit");
}

TEST_CASE("log: log_commit_message returns correct message") {
    ExtRepo t;
    auto log_result = log_list(t.repo.raw(), 1);
    REQUIRE(log_result.is_ok());
    REQUIRE(log_result->size() == 1);

    auto msg = log_commit_message(t.repo.raw(), (*log_result)[0].hash);
    REQUIRE(msg.is_ok());
    CHECK(*msg == "initial commit");
}

TEST_CASE("log: multiple commits show in order") {
    ExtRepo t;
    t.write_file("new_file.txt", "new content\n");
    REQUIRE(t.repo.stage_all().is_ok());
    REQUIRE(t.repo.commit("second commit").is_ok());

    auto result = log_list(t.repo.raw(), 10);
    REQUIRE(result.is_ok());
    REQUIRE(result->size() >= 2);
    CHECK((*result)[0].message == "second commit");
    CHECK((*result)[1].message == "initial commit");
}

TEST_CASE("diff: index_to_workdir shows unstaged changes") {
    ExtRepo t;
    t.append_to_file("tracked.txt", "added line\n");

    auto result = diff_index_to_workdir(t.repo.raw());
    REQUIRE(result.is_ok());
    CHECK(result->size() > 0);

    bool found_addition = false;
    for (const auto& line : *result) {
        if (line.origin == DiffLine::Origin::Addition) {
            found_addition = true;
            break;
        }
    }
    CHECK(found_addition);
}

TEST_CASE("diff: index_to_workdir on clean repo returns minimal diff") {
    ExtRepo t;
    auto result = diff_index_to_workdir(t.repo.raw());
    REQUIRE(result.is_ok());
}

TEST_CASE("diff: head_to_index after stage shows staged changes") {
    ExtRepo t;
    t.append_to_file("tracked.txt", "staged change\n");
    REQUIRE(t.repo.stage("tracked.txt").is_ok());

    auto result = diff_head_to_index(t.repo.raw());
    REQUIRE(result.is_ok());

    bool found_addition = false;
    for (const auto& line : *result) {
        if (line.origin == DiffLine::Origin::Addition) {
            found_addition = true;
            break;
        }
    }
    CHECK(found_addition);
}

TEST_CASE("diff: diff_commit shows full diff") {
    ExtRepo t;
    t.write_file("another.txt", "another content\n");
    REQUIRE(t.repo.stage_all().is_ok());
    auto commit_result = t.repo.commit("second commit");
    REQUIRE(commit_result.is_ok());

    auto result = diff_commit(t.repo.raw(), *commit_result);
    REQUIRE(result.is_ok());
    CHECK(result->size() > 0);
}

TEST_CASE("diff: diff_stat returns statistics") {
    ExtRepo t;
    t.append_to_file("tracked.txt", "a new line\n");

    auto result = diff_stat(t.repo.raw());
    REQUIRE(result.is_ok());
    CHECK(!result->empty());
}

TEST_CASE("remote: list remotes initially empty") {
    ExtRepo t;
    auto result = remote_list(t.repo.raw());
    REQUIRE(result.is_ok());
    CHECK(result->size() == 0);
}

TEST_CASE("remote: fetch non-existent remote fails") {
    ExtRepo t;
    auto result = remote_fetch(t.repo.raw(), "nonexistent");
    CHECK(result.is_err());
}

TEST_CASE("remote: push without remote fails") {
    ExtRepo t;
    auto result = remote_push(t.repo.raw(), "nonexistent");
    CHECK(result.is_err());
}

TEST_CASE("stash: empty stash list") {
    ExtRepo t;
    auto result = stash_list(t.repo.raw());
    REQUIRE(result.is_ok());
    CHECK(result->size() == 0);
}

TEST_CASE("stash: push tracked modification and list") {
    ExtRepo t;
    t.append_to_file("tracked.txt", "modification to stash\n");

    auto push_result = stash_push(t.repo.raw());
    REQUIRE(push_result.is_ok());

    auto list_result = stash_list(t.repo.raw());
    REQUIRE(list_result.is_ok());
    REQUIRE(list_result->size() == 1);
    CHECK((*list_result)[0].index == 0);

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    CHECK(!status->has_changes());
}

TEST_CASE("stash: drop removes stash entry") {
    ExtRepo t;
    t.append_to_file("tracked.txt", "mod to drop\n");
    REQUIRE(stash_push(t.repo.raw()).is_ok());

    auto drop_result = stash_drop(t.repo.raw(), 0);
    REQUIRE(drop_result.is_ok());

    auto list_after = stash_list(t.repo.raw());
    REQUIRE(list_after.is_ok());
    CHECK(list_after->size() == 0);
}

TEST_CASE("stash: push with custom message") {
    ExtRepo t;
    t.append_to_file("tracked.txt", "custom message stash\n");

    auto push_result = stash_push(t.repo.raw(), "my stash message");
    REQUIRE(push_result.is_ok());

    auto list_result = stash_list(t.repo.raw());
    REQUIRE(list_result.is_ok());
    REQUIRE(list_result->size() == 1);
    CHECK((*list_result)[0].message.find("my stash message") != std::string::npos);
}

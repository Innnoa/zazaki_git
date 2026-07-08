#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "git/repo.h"

namespace fs = std::filesystem;
using namespace zazaki_git;

struct TempRepo {
    fs::path dir;
    GitRepo repo;
    static inline int counter = 0;

    TempRepo() {
        git_libgit2_init();
        dir = fs::temp_directory_path() / ("zazaki_test_" + std::to_string(counter++));
        fs::remove_all(dir);
        fs::create_directories(dir);

        auto abs = fs::absolute(dir).string();
        git_repository* raw = nullptr;
        int err = git_repository_init(&raw, abs.c_str(), false);
        INFO("repo_init at ", abs, " returned ", err);
        REQUIRE(err == 0);
        git_repository_free(raw);

        auto result = repo.open(abs);
        INFO("repo.open failed: ", result.error().message());
        REQUIRE(result.is_ok());

        {
            fs::path f = dir / "test.txt";
            std::ofstream of(f);
            of << "hello world\n";
        }
        {
            fs::path f = dir / "test2.txt";
            std::ofstream of(f);
            of << "second file\n";
        }
    }

    ~TempRepo() {
        repo.close();
        fs::remove_all(dir);
    }
};

TEST_CASE("GitRepo open and head") {
    TempRepo t;

    auto head = t.repo.head();
    REQUIRE(head.is_ok());
    REQUIRE(!head->detached);
}

TEST_CASE("GitRepo status shows untracked files") {
    TempRepo t;

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    REQUIRE(status->untracked.size() == 2);
    REQUIRE(status->untracked[0].status == FileStatus::Status::NewUntracked);
}

TEST_CASE("GitRepo stage single file") {
    TempRepo t;

    auto r = t.repo.stage("test.txt");
    REQUIRE(r.is_ok());

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    REQUIRE(status->staged.size() == 1);
    REQUIRE(status->staged[0].path.find("test.txt") != std::string::npos);
    REQUIRE(status->staged[0].is_staged);
    REQUIRE(status->untracked.size() == 1);
}

TEST_CASE("GitRepo stage all and commit") {
    TempRepo t;

    auto r = t.repo.stage_all();
    REQUIRE(r.is_ok());

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    REQUIRE(status->staged.size() == 2);
    REQUIRE(status->untracked.empty());

    auto commit_result = t.repo.commit("initial commit");
    REQUIRE(commit_result.is_ok());
    REQUIRE(!commit_result->empty());

    auto status2 = t.repo.status();
    REQUIRE(status2.is_ok());
    REQUIRE(status2->staged.empty());
    REQUIRE(status2->untracked.empty());
}

TEST_CASE("GitRepo unstage single file") {
    TempRepo t;

    REQUIRE(t.repo.stage_all().is_ok());
    auto r = t.repo.unstage("test.txt");
    REQUIRE(r.is_ok());

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    REQUIRE(status->untracked.size() == 1);
}

TEST_CASE("GitRepo unstage all") {
    TempRepo t;

    REQUIRE(t.repo.stage_all().is_ok());
    auto r = t.repo.unstage_all();
    REQUIRE(r.is_ok());

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    REQUIRE(status->staged.empty());
}

TEST_CASE("GitRepo status after commit shows clean") {
    TempRepo t;

    REQUIRE(t.repo.stage_all().is_ok());
    auto commit = t.repo.commit("test commit");
    REQUIRE(commit.is_ok());

    auto status = t.repo.status();
    REQUIRE(status.is_ok());
    REQUIRE(!status->has_changes());
}

TEST_CASE("GitRepo commit with empty index creates root commit") {
    TempRepo t;

    auto result = t.repo.commit("initial root commit");
    REQUIRE(result.is_ok());
    REQUIRE(!result->empty());
}

TEST_CASE("GitRepo config string") {
    TempRepo t;

    auto name = t.repo.config_string("user.name");
    if (name.is_ok()) {
        REQUIRE(!name->empty());
    }
}

TEST_CASE("GitRepo Result type basic") {
    auto ok_result = Result<int>::ok(42);
    REQUIRE(ok_result.is_ok());
    REQUIRE(!ok_result.is_err());
    REQUIRE(*ok_result == 42);

    auto err_result = Result<int>::err("something went wrong");
    REQUIRE(err_result.is_err());
    REQUIRE(!err_result.is_ok());
    REQUIRE(err_result.error().message() == "something went wrong");

    auto void_ok = Result<void>::ok();
    REQUIRE(void_ok.is_ok());

    auto void_err = Result<void>::err("void error");
    REQUIRE(void_err.is_err());
    REQUIRE(void_err.error().message() == "void error");
}

TEST_CASE("GitRepo unopened") {
    GitRepo repo;
    REQUIRE(!repo.is_open());

    auto status = repo.status();
    REQUIRE(status.is_err());

    auto stage = repo.stage("file.txt");
    REQUIRE(stage.is_err());

    auto commit = repo.commit("msg");
    REQUIRE(commit.is_err());
}

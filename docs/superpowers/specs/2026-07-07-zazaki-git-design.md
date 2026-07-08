<!-- ai-generated: true | do-not-version-control -->

# zazaki_git 设计文档

## 概述

一个类 lazygit 的个人定制化终端 TUI git 工具，用 C++ 实现，基于 FTXUI + libgit2。

## 架构：面板组件 + 共享状态（方案 C）

```
         ┌──────────────────────┐
         │     App (事件循环)     │
         ├──────────┬───────────┤
         │ Sidebar  │ MainPanel │  ... 各面板组件
         │ + 各Tab  │ + 各面板   │
         └──────────┴───────────┘
                  │
         ┌────────┴────────┐
         │  GitRepo (状态)  │
         │  libgit2 封装    │
         └─────────────────┘
```

## 面板布局

```
┌───────────────┬──────────────────────────────┐
│   Sidebar     │                              │
│               │       Main Panel             │
│  1. Status    │    (根据选中的 tab 切换)       │
│  2. Staging   │                              │
│  3. Branches  │    - Status → 文件状态列表     │
│  4. Log       │    - Staging → stage/unstage  │
│  5. Stash     │    - Branches → 分支列表       │
│  6. Remote    │    - Log → 提交历史            │
│               │    - Stash → stash 列表       │
│               │    - Remote → 远程操作        │
├───────────────┴──────────────────────────────┤
│              Bottom Bar (提示/命令/帮助)        │
└──────────────────────────────────────────────┘
```

## 核心组件

| 组件 | 职责 |
|------|------|
| `App` | 事件循环、键盘路由、全局状态 |
| `GitRepo` | libgit2 封装（状态读取、操作执行） |
| `Sidebar` | 左侧导航面板，6 个 tab |
| `MainPanel` | 根据选中 tab 切换内容 |
| `StatusPanel` | 工作区/暂存区文件状态 |
| `StagingPanel` | 文件级 + 行级 stage/unstage |
| `BranchPanel` | 分支列表/切换/创建/删除 |
| `LogPanel` | 提交历史、diff 查看 |
| `StashPanel` | stash 列表、apply/pop/drop |
| `RemotePanel` | fetch/pull/push |

## 键盘快捷键

### 全局键位

| 按键 | 动作 |
|------|------|
| `Tab` / `Shift+Tab` | 切换面板（Sidebar ↔ Main） |
| `1-6` | 快速跳转到对应 tab |
| `q` / `Esc` | 退出 / 返回上一层 |
| `?` | 显示帮助面板 |
| `Ctrl+R` | 刷新当前视图 |

### 通用

| 按键 | 动作 |
|------|------|
| `j/k` 或 `↑/↓` | 上下移动 |
| `Enter` | 确认/进入 |
| `Space` | 选择/切换 |

### 面板专有

| 面板 | 按键 | 动作 |
|------|------|------|
| Status/Staging | `a` | stage 当前文件/全部 |
| Status/Staging | `u` | unstage |
| Status/Staging | `d` | 查看 diff |
| Staging | `c` | 打开 commit 编辑 |
| Branches | `n` | 新建分支 |
| Branches | `d` | 删除分支 |
| Branches | `Enter` | 切换分支 |
| Branches | `m` | merge 到当前分支 |
| Log | `Enter` | 查看 commit 详情/diff |
| Log | `r` | 交互式 rebase |
| Log | `s` | squash |
| Remote | `p` | push |
| Remote | `f` | fetch |
| Remote | `P` | pull |
| Stash | `s` | stash 当前改动 |
| Stash | `g` | stash pop |
| Stash | `d` | stash drop |

## 数据流

```
键盘事件 → App(路由分发) → Sidebar / MainPanel
                │
                ▼
          GitRepo (唯一状态源)
                │
                ▼
            libgit2
```

- GitRepo 是唯一状态源，面板不持有数据副本
- 每次操作后 GitRepo 自动刷新受影响的状态缓存
- 面板渲染时从 GitRepo 读取最新状态
- 异步操作（fetch/push/pull）在后台线程执行

### GitRepo 状态结构

```
GitRepo
├── status() → { staged[], unstaged[], untracked[] }
├── branches() → { local[], remote[], current }
├── log(n) → [{ hash, author, date, message, refs }]
├── stash_list() → [{ index, message, branch }]
├── diff(file) → hunks[]
├── head() → { branch, detached, tag }
└── 操作: stage(), unstage(), commit(), checkout(), merge(),
          rebase(), fetch(), push(), pull(), stash_*()
```

## 技术栈

| 组件 | 选择 |
|------|------|
| 语言 | C++17 |
| 构建 | CMake |
| TUI | FTXUI |
| Git 操作 | libgit2 |
| 格式化 | fmtlib/fmt |
| 测试 | doctest（header-only） |
| 配置 | TOML |

## 项目结构

```
zazaki_git/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── app.h / app.cpp
│   │   └── keybindings.h
│   ├── git/
│   │   ├── repo.h / repo.cpp
│   │   ├── status.h / status.cpp
│   │   ├── branch.h / branch.cpp
│   │   ├── log.h / log.cpp
│   │   ├── diff.h / diff.cpp
│   │   ├── remote.h / remote.cpp
│   │   └── stash.h / stash.cpp
│   ├── ui/
│   │   ├── sidebar.h / sidebar.cpp
│   │   ├── main_panel.h / main_panel.cpp
│   │   ├── status_panel.h / status_panel.cpp
│   │   ├── staging_panel.h / staging_panel.cpp
│   │   ├── branch_panel.h / branch_panel.cpp
│   │   ├── log_panel.h / log_panel.cpp
│   │   ├── stash_panel.h / stash_panel.cpp
│   │   ├── remote_panel.h / remote_panel.cpp
│   │   ├── commit_panel.h / commit_panel.cpp
│   │   ├── diff_panel.h / diff_panel.cpp
│   │   ├── help_panel.h / help_panel.cpp
│   │   └── components/
│   │       ├── list.h
│   │       └── input.h
│   └── util/
│       ├── logger.h
│       └── config.h
├── tests/
│   ├── test_git_status.cpp
│   └── ...
└── config/
    └── default.toml
```

## 错误处理

| 层级 | 策略 |
|------|------|
| libgit2 调用 | 返回 `Result<T>`，包含成功值或错误信息 |
| UI 层 | 底部状态栏显示最近错误/提示 |
| 异步操作 | 超时 30s，失败提示可重试 |
| 致命错误 | 直接退出并报错 |

## 测试策略

| 类型 | 范围 | 方式 |
|------|------|------|
| 单元测试 | `git/` 各模块 | 创建临时 git 仓库，验证操作结果 |
| 集成测试 | `app/` 路由 | 模拟键盘事件 |
| 手工验证 | UI | 真实仓库走查流程 |

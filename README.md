# ClipLoom

ClipLoom is a Windows-native clipboard manager focused on fast search, reliable history, and keyboard-first workflows.

中文文档请看：[README-CN.md](README-CN.md)

## Overview

ClipLoom runs in the Windows notification area, captures clipboard content, and lets you reopen history with a global shortcut or configured trigger.

Current core capabilities:

- clipboard monitoring with persistence
- text, HTML, RTF, image, and file-list capture
- exact, fuzzy, regexp, and mixed search
- highlighted search matches
- tray icon and tray quick actions
- configurable global open hotkey
- optional double-click modifier trigger
- preview pane
- pin/unpin, rename pin, edit pinned plain text
- ignore rules and startup registration
- single-instance behavior

Known gaps still on roadmap:

- image preview zoom
- OCR and updater
- scripting interface and diagnostics improvements

## Quick Start

1. Download the latest release asset: `cliploom.zip`.
2. Extract it to a local folder.
3. Run `ClipLoom.exe`.
4. Open history with your configured hotkey (default `Ctrl+Shift+C`) or tray icon.

## Build From Source

### Prerequisites

- Windows 10/11 x64
- Visual Studio 2022 (Desktop development with C++)
- CMake 3.25+

### Configure, Build, Test

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMACCY_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

### Package

```powershell
cmake --install build --config Release --prefix package
Compress-Archive -Path package\* -DestinationPath cliploom.zip
```

## Usage Basics

- `Up` / `Down`: move selection
- `Enter`: activate selected history item
- `Esc`: close popup
- `Delete`: remove selected item
- `Ctrl+P`: pin / unpin
- `Ctrl+R`: rename pinned item
- `Ctrl+E`: edit pinned plain text
- `Ctrl+1` to `Ctrl+9`: jump to top visible items

## Configuration

Open settings from tray:

1. Right-click tray icon.
2. Click `Settings...`.

Settings include:

- open trigger (hotkey / double-click modifier)
- capture behavior (auto paste, plain text paste)
- storage policy (history limit, sort order, capture formats)
- appearance options (search visibility, preview pane, popup position)
- ignore rules (apps, patterns, formats)
- advanced cleanup options

## Data and Migration

Default data location:

- `%LOCALAPPDATA%\ClipLoom`

Migration compatibility:

- if `%LOCALAPPDATA%\MaccyWindows` exists and `ClipLoom` does not, ClipLoom will continue using the legacy folder

Persisted data includes:

- history items
- pin metadata and pinned edits
- popup placement and UI preferences
- capture and ignore settings
- startup preference

## Architecture

- `src/core`: platform-agnostic domain logic
- `src/platform/win32`: Win32 integration layer (clipboard, input, startup, source app)
- `src/app`: Win32 app shell, windows, tray, settings, popup
- `tests`: unit tests for core behavior

## Development Workflow

1. Create a feature branch from `main`.
2. Implement changes with small commits.
3. Run local build and tests.
4. Open a pull request to `main`.
5. Ensure `build-windows` workflow is green before merge.

Recommended local checks:

```powershell
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Release Workflow

Release is tag-driven:

1. Merge release-ready code into `main`.
2. Create and push a version tag: `v*` (for example `v0.2.17`).
3. `release-windows.yml` builds and publishes `cliploom.zip` to GitHub Releases.

CI workflows:

- `.github/workflows/build-windows.yml`
- `.github/workflows/release-windows.yml`

## Documentation

- `README-CN.md`
- `docs/windows-maccy-clone-todolist.md`
- `docs/tech-stack-decision.md`
- `docs/ui-upgrade-options.md`
- `docs/win32-app-split-todolist.md`
- `docs/github-actions-windows-build-notes.md`

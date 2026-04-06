# Maccy Windows Clone

This is a standalone project placeholder for building a Windows version of Maccy.

## Goal

Create a Windows clipboard manager that matches the current Maccy feature set as closely as possible, with Windows-native behavior where the platforms differ.

## Chosen Stack

The primary target is now:

- `C++20 + Win32 API + CMake + SQLite`

Reason:

- smallest packaged size
- no .NET runtime dependency
- no Windows App SDK runtime dependency
- direct access to clipboard, tray, hotkeys, `SendInput`, and window management APIs

## Layout

- `docs/`: planning and design notes
- `src/`: application source code
- `tests/`: unit and integration tests

## Docs

- `docs/windows-maccy-clone-todolist.md`
- `docs/tech-stack-decision.md`
- `docs/github-actions-windows-build-notes.md`

## CI

GitHub Actions workflow:

- `.github/workflows/build-windows.yml`

It builds on `windows-2022`, runs tests, installs the packaged layout, and uploads a `maccy-windows-x64.zip` artifact.

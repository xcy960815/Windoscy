# Maccy Windows Clone

A standalone Windows clipboard manager inspired by the current Maccy feature set.

## Goal

Create a Windows clipboard manager that matches the current Maccy feature set as closely as possible, with Windows-native behavior where the platforms differ.

## Current Status

The current build is already usable on Windows and includes:

- clipboard monitoring and history persistence
- text, HTML, RTF, image, and file list capture
- search with exact, fuzzy, regexp, and mixed modes
- search result highlight
- tray icon, tray menu, and global hotkey popup
- preview pane
- pinning, pin rename, and pinned text editing
- auto paste and plain text paste options
- ignore rules, startup registration, history limit, and sorting
- single-instance startup behavior

Still missing or incomplete:

- configurable global hotkey UI
- image preview zoom
- double-tap modifier activation
- OCR, updater, scripting interface, and richer diagnostics

## How To Use

1. Launch `maccy_win.exe`.
2. Look for the app in the Windows notification area tray.
3. Open clipboard history with `Ctrl+Shift+C`.
4. If that hotkey is busy, click the tray icon instead.
5. Copy text, images, files, or rich text as usual with `Ctrl+C`.
6. Open the popup and search or browse your saved history.
7. Press `Enter` on the selected item to restore it to the clipboard.

By default, selecting an item also auto-pastes it back into the previous window.

## Keyboard Shortcuts

- `Ctrl+Shift+C`: open the history popup
- `Up` / `Down`: move through the list
- `Enter`: activate the selected item
- `Esc`: close the popup
- `Delete`: remove the selected history item
- `Ctrl+P`: pin or unpin the selected item
- `Ctrl+R`: rename a pinned item
- `Ctrl+E`: edit pinned plain text
- `Ctrl+1` to `Ctrl+9`: jump to the first 9 visible items

## Settings

Settings are available from both a dedicated settings window and the tray menu.

To open settings:

1. Right-click the tray icon.
2. Click `Settings...`.

You can still use the grouped tray menu sections for quick toggles.

Available settings today:

- `General`
  - `Enable clipboard capture`
  - `Auto Paste`
  - `Paste Plain Text`
  - `Start on Login`
  - `Show Search`
  - `Show Preview`
  - `Remember Position`
  - `Show startup guide`
- `Search Mode`
  - `Mixed`
  - `Exact`
  - `Fuzzy`
  - `Regexp`
- `Sort By`
  - `Last Copied`
  - `First Copied`
  - `Copy Count`
- `Pins`
  - `Pins on Top`
  - `Pins on Bottom`
- `History Limit`
  - `50`
  - `100`
  - `200`
  - `500`
- `Capture Types`
  - `Ignore All`
  - `Text`
  - `HTML`
  - `Rich Text`
  - `Images`
  - `Files`
- ignore rules editors
  - ignored applications
  - allowed applications
  - ignored text patterns
  - ignored content formats
- direct actions
  - `Show History`
  - `Pause Capture`
  - `Ignore Next Copy`
  - `Clear History`
  - `Clear All History`
  - `Exit`

## Data Storage

The app currently stores its data under the current user's local app data directory in a `MaccyWindows` folder.

It persists:

- history items
- pin state and pinned edits
- popup size and last position
- search mode and sort mode
- capture and ignore settings
- startup preference

## Chosen Stack

The current implementation uses:

- `C++20 + Win32 API + CMake`

The original planning notes mentioned SQLite, but the current build uses a lightweight custom persistence format instead.

## Layout

- `docs/`: planning and design notes
- `src/`: application source code
- `tests/`: unit tests

## Docs

- `docs/windows-maccy-clone-todolist.md`
- `docs/tech-stack-decision.md`
- `docs/github-actions-windows-build-notes.md`

## CI

GitHub Actions workflows:

- `.github/workflows/build-windows.yml`
- `.github/workflows/release-windows.yml`

They build on `windows-2022`, run tests, package the app, and publish release zip artifacts for tagged versions.

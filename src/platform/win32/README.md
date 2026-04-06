# Win32 Platform Layer

This folder is reserved for Windows-only integrations:

- clipboard monitoring and read/write
- tray icon and menu
- global hotkeys
- popup positioning
- source application detection
- `SendInput` paste behavior
- startup registration
- updater integration

Keep this layer thin. Business rules should stay in `src/core`.

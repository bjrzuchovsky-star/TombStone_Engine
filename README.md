# Tombstone Engine

Tombstone Engine is a fully custom C++ game engine built for high performance, low-level control, and a streamlined development workflow.  
It is designed around a modular architecture, a custom build system, and a custom rendering backend to ensure predictable behavior and maximum efficiency across all engine operations.

This repository contains the core engine source code, internal tools, and supporting systems used to build and run Tombstone Engine.

---

## Features

### Custom Build System
A dedicated build pipeline designed specifically for the engine's architecture.  
Provides fast compilation, predictable output, and full control over how engine modules are built and linked.

### Custom Rendering Backend
A low-level rendering system focused on speed, stability, and minimal overhead.  
Built to support modern graphics APIs and optimized for high-framerate gameplay and editor performance.

### Modular Engine Architecture
Core systems are separated into clean, independent modules, allowing for:
- Efficient runtime behavior  
- Clear engine structure  
- Easy internal maintenance  
- High scalability for future features  

### Admin / Client / Game Builds
Tombstone Engine is structured into three distinct build types:

- **Admin Build**  
  Full engine + editor tools. Used internally for development, debugging, and engine management.

- **Client Build**  
  Lightweight engine runtime for developers building games on top of Tombstone Engine.

- **Game Build**  
  Stripped-down runtime containing only what shipped games need for execution and performance.

### Performance-Focused Design
Every system is built with performance as the primary goal:
- Minimal abstraction overhead  
- Tight memory usage  
- Predictable update loops  
- High-efficiency rendering path  
- Optimized runtime modules  

---

## Project Goals

- Deliver a fast, stable, and fully custom C++ engine  
- Provide a clean development workflow through custom tooling  
- Maintain strict control over engine behavior and architecture  
- Support high-framerate gameplay and editor performance  
- Offer a clear separation between engine development and game development

---

## Build Requirements

- C++20 compatible compiler  
- Supported graphics API (Vulkan/DirectX/OpenGL depending on configuration)  
- Platform-specific SDKs as required  
- Custom build system included in this repository  

---

## License

This project is **proprietary** and **all rights reserved**.  
No usage, copying, modification, or distribution is permitted without explicit written permission.

See the `LICENSE` file for full details.

---

## Status

Active development.  
Major systems are being rebuilt and optimized as part of the current engine iteration.

---

## Repository layout

```
CMakeLists.txt          # Root: TombStoneEngine, C++20, TS_BUILD_* options
cmake/                  # Shared CMake helpers (TombStoneOptions.cmake)
src/core/               # Engine core (types, assert, Engine)
src/platform/           # Platform / window stubs
src/render/             # Renderer stubs
src/input/              # Input stubs
src/net/                # Networking stubs (4-player MMO target later)
src/physics/            # 2D physics stubs
src/scene/              # Scene stubs
src/editor/             # Admin UI (AppFlow, settings, projects, workspace, ui, screens)
cmake/FetchImGuiDeps.cmake  # FetchContent GLFW + Dear ImGui (admin only)
apps/admin|client|game/ # Build targets controlled by TS_BUILD_*
```

### Admin flow (ImGui)

`ts_admin` opens a GLFW + OpenGL3 window and drives ImGui screens owned by `src/editor/`:

1. **Loading** -- splash title + progress bar; auto-advances to Login  
2. **Login** -- username/password, Login, Dev login, Settings  
3. **ProjectManager** -- list projects, Open / double-click, New 2D (validated name), Settings, Logout; errors shown in-UI  
4. **Settings** -- edit `projects_root` (Browse opens an in-app ImGui folder picker), theme, `auto_login_dev`, username; Apply/Save still validates, writes settings, and reloads projects  
5. **Editor2D** -- live Hierarchy (create/rename/delete/select under Scene), Viewport2D canvas (rects, pan MMB/Alt-drag, wheel zoom, optional grid), Inspector (name, transform, color/tint, layer/z) + Back  

Namespace: `ts::tombstone::editor`.

**Editor2D workspace** keeps an in-memory entity list (seeded with Camera2D / Player / TileMap placeholders; not persisted yet). Hierarchy edits and Inspector fields update the viewport immediately. **Settings → Browse...** uses a portable ImGui directory browser (no extra native deps); Apply remains the step that validates/writes `projects_root` and reloads the project list.


### Building admin with ImGui

Admin pulls **GLFW 3.4** and **Dear ImGui (docking branch)** via CMake `FetchContent` (`cmake/FetchImGuiDeps.cmake`). Downloaded sources live under the build directory (`build/_deps/...`) and are gitignored — they are **not** committed.

Requirements (in addition to C++20):

- OpenGL development libraries  
- On Linux: X11 (or Wayland) development packages for GLFW (`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libgl1-mesa-dev`)  
- On Windows/MSVC: a working desktop OpenGL driver (Visual Studio 2019+ recommended)

```bash
cmake -S . -B build -DTS_BUILD_ADMIN=ON -DTS_BUILD_CLIENT=ON -DTS_BUILD_GAME=ON
cmake --build build --target ts_admin
./build/apps/admin/ts_admin          # GUI
./build/apps/admin/ts_admin --smoke  # headless flow smoke (no window)
```

Client/Game do **not** fetch or link ImGui/GLFW. Building with `-DTS_BUILD_ADMIN=OFF` skips `src/editor` and the FetchContent deps entirely.

### Projects & settings (on disk)

Defaults are **cwd-relative** (portable; no machine-absolute paths):

| Path | Purpose |
|------|---------|
| `./TombStoneConfig/settings.json` | User settings store |
| `./TombStoneProjects/` | Default `projects_root` (saves/projects folder) |

`settings.json` fields: `projects_root`, `username` (last-used), `auto_login_dev` (bool), `theme` (`dark`/`light` stub), `last_project_path`.

Each project is a folder under `projects_root` with a `project.json`:

```json
{
  "name": "Sample 2D Platformer",
  "dimension": "2d",
  "created": "2026-09-30T00:00:00Z",
  "last_opened": "2026-09-30T00:00:00Z"
}
```

On first run (empty `projects_root`), Admin seeds one sample 2D project. Changing `projects_root` in Settings reloads the project list from the new folder. Local `TombStoneProjects/` and `TombStoneConfig/` are gitignored.

Configure with CMake 3.20+ and a C++20 toolchain. Toggle `TS_BUILD_ADMIN`, `TS_BUILD_CLIENT`, and `TS_BUILD_GAME` as needed. See **Building admin with ImGui** above when enabling Admin.

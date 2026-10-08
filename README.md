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
src/editor/             # Admin UI (AppFlow, settings, projects, workspace/SceneIO + TileMap, assets/TextureCache, ui, screens)
cmake/FetchImGuiDeps.cmake  # FetchContent GLFW + Dear ImGui (admin only)
apps/admin|client|game/ # Build targets controlled by TS_BUILD_*
```

### Admin flow (ImGui)

`ts_admin` opens a GLFW + OpenGL3 window and drives ImGui screens owned by `src/editor/`:

1. **Loading** -- splash title + progress bar; auto-advances to Login  
2. **Login** -- username/password, Login, Dev login, Settings  
3. **ProjectManager** -- list projects, Open / double-click, New 2D (validated name), Settings, Logout; errors shown in-UI  
4. **Settings** -- edit `projects_root` (Browse opens an in-app ImGui folder picker), theme, `auto_login_dev`, username; Apply/Save still validates, writes settings, and reloads projects  
5. **Editor2D** -- live Hierarchy (create/duplicate/rename/delete, multi-select), Viewport2D canvas (drag-to-move, box select, snap-to-grid, pan MMB/RMB/Alt-drag, wheel zoom), Inspector (name, snapped transform, color/tint, layer/z, TileMap / Sprite sections) + Back, **Tile Palette** (tilemap painting) and **Supply Wagon** (project images); File → Save Scene  

Namespace: `ts::tombstone::editor`.

**Editor2D workspace** loads/saves `<project>/scene.json` (entities: id/name, x/y/w/h, rgba tint, layer/z, optional `tilemap` / `sprite` objects, plus pan/zoom/grid, `grid_size`, `snap`, `selection`; older files without the new keys still load). Opening a 2D project loads scene.json when present, otherwise seeds Camera2D / Player / TileMap and writes an initial scene.json. Hierarchy/Inspector edits autosave; leaving the editor (Back / Quit) also saves. Hierarchy edits and Inspector fields update the viewport immediately. **Settings → Browse...** uses a portable ImGui directory browser (no extra native deps); Apply remains the step that validates/writes `projects_root` and reloads the project list.


### UI identity (Admin shell)

Admin has its own look, kept in `src/editor/ui/Theme` (palette, metrics, custom-drawn chrome) and `src/editor/ui/Brand.h` (copy). The tone is frontier/western with a gothic edge, not cartoonish:

- **Dusk** (default): deep charcoal base, warm stone/sand neutrals, bone text, **amber/gold** primary accent, **rust/copper** secondary. **Parchment** is the light variant (warm paper, ink-brown text, darker gold). Pick either in Settings → Lamplight.
- Sharp 2-3 px corners with 1 px inked frame borders. Custom-drawn section headers (amber diamond, label, caption, fading copper rule), ornamental dividers, a headstone mark, title strips, and a branded progress bar. Hover and active states go copper and amber.
- Copy keeps one short, dry voice: splash "TOMBSTONE / Four riders. One frontier. Build it right.", login "Sign the ledger. Ride in.", Project Manager "Claims" / "Break Ground" / "Bury Project?", and Settings sections "Territory / Rider / Lamplight / Trail". The editor status bar carries the brand, and the window title follows the open project.

Editor2D runs in an ImGui **Docking** layout (Hierarchy over Supply Wagon / Viewport2D / Inspector over Tile Palette) with a tool strip (Undo, Redo, Select/Brush/Erase/Fill/Rect/Pick tools + brush swatch, Grid, Snap, cell size, Duplicate, Delete, Reset Cam, Save) and a status bar (fps, zoom, grid/snap, selection, recent action, path). Docking and multi-viewport flags are enabled in `apps/admin/main.cpp`.

### Editor2D tools

| Action | Input |
|---|---|
| Undo / Redo | Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z (also Edit menu and toolbar) |
| Move selection | LMB drag in Viewport (Esc cancels mid-drag) |
| Multi-select | Ctrl+click (toggle) / Shift+click (add; Hierarchy: range) |
| Box select | LMB drag on empty canvas (Ctrl/Shift adds) |
| Duplicate / Delete | Ctrl+D / Del (also toolbar, Hierarchy, Edit menu) |
| Nudge | Arrows (1 px, or 1 cell with snap); Shift+Arrows (10 px, or 4 cells) |
| Snap / grid | Shift+G snap, G grid, `[` `]` halve/double cell size, or toolbar cell field |
| Select all / clear / rename | Ctrl+A / Esc / F2 |

When snap is on, drag moves, nudges, duplicates, and Inspector X/Y/W/H edits all land on the grid. In a group drag the primary snaps and the others keep their spacing. The Inspector edits the primary selection (amber outline) and shows the selection count. Every tool autosaves `scene.json`. `--smoke` covers snap, nudge, multi-select, box select, duplicate, and delete, plus autosave and persistence.

**Undo / redo** covers every workspace edit: create, rename, delete (single or multi), duplicate, drag-move, arrow nudge, snap-selection-to-grid, Reset Scene, and Inspector edits (name, X/Y/W/H, tint, layer). History is snapshot based (entity list, including each entity's tile grid and sprite, + selection), capped at 200 steps, kept in memory only, and cleared when a project is opened or closed. Continuous edits coalesce into one step: a whole viewport drag is one "Move", an Inspector field is one step from activation to release, and held or rapid arrow nudges (within 0.6 s) are one "Nudge". The Edit menu names the step ("Undo Move 3"), and the status bar reports "Undid: Move 3" / "Redid: Duplicate 2". Any new edit clears the redo stack. Undo and redo restore the selection from that step and autosave `scene.json`. Grid, snap, and camera are view settings and are not tracked. While a text field is active, Ctrl+Z belongs to the text field. `--smoke` runs six edits, undoes all of them back to the initial state, redoes them, checks that a new edit clears redo, and confirms `scene.json` after each step.

### TileMaps (painting)

A TileMap entity owns a grid of tile ids: width x height in cells, tile size in px, origin at the entity's x/y (its w/h always follow the grid). `0` is empty. Any number of TileMaps can live in a scene (seeded scenes get an 8x2 painted "TileMap"; **Edit → Create TileMap** or the Tile Palette's **+ TileMap** stakes a 16x8 one).

Tiles come from the TileMap's **tileset** image when one is assigned (sliced left-to-right, top-to-bottom by tile size; id 1 = top-left slice) or, with no image, from a built-in 16-tile frontier palette (Dirt, Sand, Stone, Grass, Water, Wood, Clay, Adobe, Dry Brush, Mud, Brick, Cactus, Iron Rail, Coal, Gold Ore, Bone), so painting works with zero assets. A missing tileset falls back to the built-in colours.

| Tool | Key | Use |
|---|---|---|
| Select | V | Original pick / drag / box-select behaviour |
| Brush | B | LMB click or drag on the selected TileMap; a whole stroke (mouse down → up) is one undo step |
| Erase | E | Same as Brush, clears cells |
| Fill | F | Bucket-fill the 4-connected region matching the clicked tile |
| Rect | R | Drag a box of tiles (hold Shift on release to erase the box) |
| Eyedropper | I or Alt+click | Pick the tile under the cursor as the brush (I then returns to Brush) |
| Brush size | 1 / 2 / 3 | 1x1, 2x2, 3x3 (also in the Tile Palette) |
| Cancel stroke | Esc | Mid-stroke: restores the tiles, no undo step |

The viewport shows the brush footprint / fill cell / rect box under the cursor and cell lines on the TileMap being painted; the status bar shows the tool, hovered cell and tile id. Picking a tile tool with no TileMap selected selects the first one; clicking another TileMap retargets the brush. In tile tools Alt+LMB over the TileMap picks a tile, so pan with MMB/RMB there. The Inspector's TileMap section edits Cols / Rows (resizing keeps existing tiles; Enter or +/- applies), Tile px, and the tileset (picker or drop an image from the Supply Wagon), plus Clear Tiles. Every paint stroke, fill, rect, resize, tile-size and tileset change is one undo step and autosaves `scene.json`.

### Sprites and the Supply Wagon (assets)

Images live in `<project>/assets/` (created when a project is made or opened). The **Supply Wagon** panel lists every image there (png/jpg/bmp/tga/gif/psd/pnm) with a thumbnail and size, refreshing every couple of seconds. **Import...** opens the in-app file picker and copies the image in (never overwrites: `name_2.png`). Drag an image onto the viewport to stake a sprite entity centred on the drop point and sized to the image; double-click or right-click assigns it to the selected entity, or as the selected TileMap's tileset.

Any non-TileMap entity can carry a sprite: project-relative image path, optional source rect (x/y/w/h, 0 = to the edge), flip X/Y, tinted by the entity colour (**White tint** shows the image as drawn). The viewport draws the textured quad (nearest filtering) instead of the rect; a missing or unreadable file falls back to the tinted rect with an amber "!" marker and a tooltip. Sprite assignment, removal, flips, source rect, new sprite entities and "Fit to image" are all undoable.

Images are decoded with [stb_image](https://github.com/nothings/stb) (public domain / MIT, fetched at configure time for admin builds, pinned by commit + SHA-256). Decoding and caching (`src/editor/assets/TextureCache`) are renderer-agnostic: the admin app registers an OpenGL uploader, while `--smoke` runs headless and only reads image sizes. The cache keys by path and reloads a file when it changes on disk.

### scene.json format

`"version": 2` adds two optional per-entity objects (version 1 files still load; a v1 entity named `TileMap*` becomes an empty 32 px grid covering its old rect):

```json
"tilemap": {"cols": 8, "rows": 2, "tile_size": 32, "tileset": "", "encoding": "rle", "data": "8*4,8*1"},
"sprite": {"path": "assets/rider.png", "flip_x": false, "flip_y": false, "use_src": false, "src_x": 0, "src_y": 0, "src_w": 0, "src_h": 0}
```

`data` is row-major run-length text: comma-separated `count*id` runs (a single cell is just `id`).

`--smoke` (headless, no GL) paints a multi-cell stroke and checks it is one undo step, a 2x2 brush, erase, a cancelled stroke, bucket fill, rect fill/erase, eyedropper, resize (keeps tiles; undo restores cut cells), tile size, undo-all / redo-all with `scene.json` in sync, the v2 RLE roundtrip, the v1 upgrade, sprite drop-create / assign / flip / source-rect roundtrip with undo/redo, missing and corrupt image fallbacks, tileset slicing, and import.

### Building admin with ImGui

Admin pulls **GLFW 3.4**, **Dear ImGui (docking branch)** and the single **stb_image.h** header via CMake `FetchContent` (`cmake/FetchImGuiDeps.cmake`). Downloaded sources live under the build directory (`build/_deps/...`) and are gitignored — they are **not** committed.

Requirements (in addition to C++20):

- OpenGL development libraries  
- On Linux: X11 (or Wayland) development packages for GLFW (`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libgl1-mesa-dev`)  
- On Windows/MSVC: a working desktop OpenGL driver (Visual Studio 2019+ recommended)

```bash
cmake -S . -B build -DTS_BUILD_ADMIN=ON -DTS_BUILD_CLIENT=ON -DTS_BUILD_GAME=ON
cmake --build build --target ts_admin
./build/apps/admin/ts_admin          # GUI
./build/apps/admin/ts_admin --smoke  # headless flow smoke (no window, no GL)
```

Client/Game do **not** fetch or link ImGui/GLFW. Building with `-DTS_BUILD_ADMIN=OFF` skips `src/editor` and the FetchContent deps entirely.

### Projects & settings (on disk)

Defaults are **cwd-relative** (portable; no machine-absolute paths):

| Path | Purpose |
|------|---------|
| `./TombStoneConfig/settings.json` | User settings store |
| `./TombStoneProjects/` | Default `projects_root` (saves/projects folder) |

`settings.json` fields: `projects_root`, `username` (last-used), `auto_login_dev` (bool), `theme` (`dark`/`light` stub), `last_project_path`.

Each project is a folder under `projects_root` with a `project.json` (and, for 2D projects after first Editor2D open, a `scene.json`):

```json
{
  "name": "Sample 2D Platformer",
  "dimension": "2d",
  "created": "2026-09-30T00:00:00Z",
  "last_opened": "2026-09-30T00:00:00Z"
}
```

`scene.json` stores the Editor2D workspace (entity list + view). On first run (empty `projects_root`), Admin seeds one sample 2D project. Changing `projects_root` in Settings reloads the project list from the new folder. Local `TombStoneProjects/` and `TombStoneConfig/` are gitignored.

Configure with CMake 3.20+ and a C++20 toolchain. Toggle `TS_BUILD_ADMIN`, `TS_BUILD_CLIENT`, and `TS_BUILD_GAME` as needed. See **Building admin with ImGui** above when enabling Admin.

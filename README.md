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
src/core/               # Engine core (types, assert, Engine, JsonMini)
src/platform/           # Platform / window stubs
src/render/             # Renderer stubs
src/input/              # Input stubs
src/net/                # Networking stubs (4-player MMO target later)
src/physics/            # 2D physics stubs
src/scene/              # Scene data: Entity2D + components, TileMap, scene.json read/write (no UI, no GL)
src/runtime/            # Game runtime: World, PlaySession (60 Hz fixed step), abstract input (no UI, no GL)
src/gfx/                # stb_image TextureCache + OpenGL texture uploader (admin + game)
src/editor/             # Admin UI (AppFlow, settings, projects, workspace, Play mode, launcher, ui, screens)
cmake/FetchImGuiDeps.cmake  # FetchContent GLFW + stb_image (admin + game) and Dear ImGui (admin only)
apps/admin|client|game/ # Build targets controlled by TS_BUILD_*
```

### Admin flow (ImGui)

`ts_admin` opens a GLFW + OpenGL3 window and drives ImGui screens owned by `src/editor/`:

1. **Loading** -- splash title + progress bar; auto-advances to Login  
2. **Login** -- username/password, Login, Dev login, Settings  
3. **ProjectManager** -- list projects, Open / double-click, New 2D (validated name), Settings, Logout; errors shown in-UI  
4. **Settings** -- edit `projects_root` (Browse opens an in-app ImGui folder picker), theme, `auto_login_dev`, username; Apply/Save still validates, writes settings, and reloads projects  
5. **Editor2D** -- live Hierarchy (create/duplicate/rename/delete, multi-select), Viewport2D canvas (drag-to-move, box select, snap-to-grid, pan MMB/RMB/Alt-drag, wheel zoom), Inspector (name, snapped transform, color/tint, layer/z, TileMap / Sprite / Player / Camera2D / Spawn Point sections) + Back, **Tile Palette** (tilemap painting) and **Supply Wagon** (project images); File → Save Scene; **Play / Pause / Stop** runs the scene in the Viewport  

Namespace: `ts::tombstone::editor`.

**Editor2D workspace** loads/saves `<project>/scene.json` (entities: id/name, x/y/w/h, rgba tint, layer/z, optional `tilemap` / `sprite` / `player` / `camera` / `spawn` objects, plus pan/zoom/grid, `grid_size`, `snap`, `selection`; older files without the new keys still load). Opening a 2D project loads scene.json when present, otherwise seeds Camera2D / Player / TileMap and writes an initial scene.json. Hierarchy/Inspector edits autosave; leaving the editor (Back / Quit) also saves. Hierarchy edits and Inspector fields update the viewport immediately. **Settings → Browse...** uses a portable ImGui directory browser (no extra native deps); Apply remains the step that validates/writes `projects_root` and reloads the project list.


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

Images are decoded with [stb_image](https://github.com/nothings/stb) (public domain / MIT, fetched at configure time for admin builds, pinned by commit + SHA-256). Decoding and caching (`src/gfx/TextureCache`) are renderer-agnostic: the admin app registers an OpenGL uploader, while `--smoke` runs headless and only reads image sizes. The cache keys by path and reloads a file when it changes on disk.

### Play mode (run the game inside the editor)

Hit **Play** and the scene rides: the Viewport stops being an editing canvas and runs the real game runtime on a snapshot of the scene. **Stop** throws the run away and puts the editor back exactly as it was: entities, selection, camera, undo history. Nothing done while playing writes `scene.json` or adds an undo step.

| Action | Input |
|---|---|
| Play / Stop | F5 or Ctrl+P (toolbar Play / Stop, Play menu) |
| Pause / resume | F6 (toolbar Pause) |
| Step one tick (paused) | F10 (toolbar Step) |
| Move player 1 | WASD or arrow keys; gamepad left stick / d-pad |
| Gamepads | pads 1-4 drive player slots 1-4 (pad 1 shares slot 1 with the keyboard) |
| Free camera | C toggles; drag to pan, wheel to zoom (C again hands back to the follow cam) |
| Collision overlay | K (also Play menu; works in edit mode too) |
| Launch Game | toolbar Launch / File menu: saves, then starts `ts_game` on this project |

While riding the viewport gets a copper frame and a `RIDING mm:ss tick N` badge (amber `PAUSED` and a dimmed scene while held); the status bar shows the play clock, tick count, measured ticks/s, player 1's position and the camera mode. Edit tools, the Hierarchy, Inspector, Tile Palette and Supply Wagon are locked, and Save is refused with a note. **Esc never ends a ride**: only F5 / Ctrl+P / Stop do. Play flushes any half-finished edit and autosaves first, so `ts_game` and Play see the same scene.

Gameplay components are edited in the Inspector (each change is one undo step):

- **Player**: player slot P1-P4 and speed (px/s). Movement is 8-way; diagonals are not faster.
- **Camera2D**: follow target, smoothing (seconds; 0 = locked on), zoom, optional bounds rect (**Fit to TileMap**). With no Camera2D the game follows player 1.
- **Spawn Point**: the player in this slot starts centred on it.

New projects seed **Player** with a P1 controller and a dynamic collider, and **Camera2D** following it.

### Collision and solid tiles

The game is top-down with no gravity: up to four riders move freely and the runtime (`src/runtime/World_Collision.cpp`, no UI or GL) keeps them out of the walls.

- **Solid tiles.** Every tile id in a tileset is solid or walkable. The built-in palette starts with Stone, Water, Wood, Adobe, Brick, Cactus, Coal and Gold Ore solid (Dirt, Sand, Grass and the rest are open ground); an image tileset starts with nothing solid. Project overrides live in `scene.json` under `tile_solidity` (per tileset, so every TileMap using that tileset agrees).
- **Colliders.** Any entity can carry an axis-aligned box collider: offset and size relative to the entity, **solid** or **trigger**, **static** or **dynamic**. Static solids (crates, fences, walls) block riders and never move. Dynamic solids (riders, barrels) get pushed. Entities without a collider are scenery.
- **Movement.** Each tick a rider moves X first, resolves against solid tiles and static solids, then Y, so pushing diagonally into a wall slides along it. A move longer than half the thinnest obstacle (half a tile, half a 4 px fence) is split into substeps, so nothing tunnels even at thousands of px/s. Riders stop flush: a 32 px rider stopped by a wall at x = 192 has its right edge at exactly 192.
- **Dynamic vs dynamic.** After moving, overlapping dynamic bodies are pushed apart along the axis of least overlap, half each (a few passes), so riders never finish a tick inside one another and a rider can shove a barrel.
- **Triggers.** A trigger collider never blocks. Each tick the World records enter / exit events for riders and dynamic bodies (exits before enters, each fired exactly once). The editor's Play status bar and the K overlay show the latest one ("Player rode into Gate (tick 171)"), and `ts_game` shows it in the window title. Triggers have no behaviour of their own yet: they are the hook for scripting.

**Solid-tile workflow.** In the **Tile Palette**, solid tiles carry a red corner wedge. **Right-click** a tile to toggle it, or use the **Solid** checkbox next to the brush tile. Each toggle is one undo step ("Solid: Grass" / "Walkable: Grass"), autosaves `scene.json`, and applies to the tileset of the TileMap being painted (or the built-in palette).

**Colliders in the Inspector.** The **Collision** section shows **+ Collider** (sized to the entity: dynamic for players, static otherwise), then Solid / Trigger, Static / Dynamic, Offset and Size, **Fit to Entity** and **Remove Collider**. Every change is one undo step, and a whole drag of Offset or Size is one step.

**K overlay.** **K** (or **Play → Collision Overlay**) toggles the collision overlay in both the edit Viewport and Play:

| Colour | Shows |
|---|---|
| Red | Solid tiles (each row merged into runs) |
| Copper | Static solid colliders |
| Green | Dynamic solid colliders (riders, barrels) |
| Gold with a cross | Triggers (filled while somebody is inside) |

A legend sits in the Viewport's bottom-left corner. In edit mode the overlay reflects the scene as edited, and in Play it reflects the live World. K is ignored while typing in a text field.

### ts_game (the standalone game)

```bash
./build/apps/game/ts_game --project ./build/apps/admin/TombStoneProjects/sample-2d-platformer
./build/apps/game/ts_game --smoke --project <dir>    # headless: load, ride 60 ticks, verify, exit
```

`ts_game` opens its own GLFW window (1280x720, resizable), loads `<project>/scene.json` and its images through the runtime, and draws tilemaps and sprites with a small fixed-function OpenGL renderer (no ImGui, no editor code). Keys: WASD / arrows (and gamepads 1-4) ride, P or F6 pause, F10 step while paused, R or F5 reload `scene.json` from disk, K collision overlay (same colours as the editor; `--collision` starts with it on), Esc quit. The title bar shows P1's position and the latest trigger event. The editor's **Launch Game** finds `ts_game` next to `ts_admin` or in the build tree (`build/apps/game/`); if it isn't built it says so (configure with `-DTS_BUILD_GAME=ON`).

**Runtime architecture.** `src/scene` owns the data (Entity2D and its optional components, tilemaps, the scene.json reader/writer) and `src/runtime` owns the game: a `World` built from a copy of the entities, advanced by a `PlaySession` at a fixed 60 Hz (accumulator, at most 8 catch-up ticks per frame) from an abstract per-player `InputFrame` (move x/y + action buttons for slots 0-3), with a follow camera and a renderer-neutral draw list (world-space quads + UVs) interpolated between ticks. Neither library knows about ImGui or OpenGL: the editor feeds it keyboard/gamepad input and draws the list with ImGui, `ts_game` feeds GLFW input and draws it with OpenGL, and `--smoke` feeds scripted input. Collision runs inside the World's tick (see above). Animation, scripting and networked players (a local server plus up to four clients) slot in as more per-tick systems and more `InputFrame` sources without touching the editor.

### scene.json format

`"version": 4` (current). Every entity has id/name, x/y/w/h, r/g/b/a and layer, plus optional component objects. Version 2 added `tilemap` and `sprite`, version 3 added the gameplay components `player`, `camera` and `spawn`, and version 4 adds `collider` and the top-level `tile_solidity`:

```json
"tilemap": {"cols": 8, "rows": 2, "tile_size": 32, "tileset": "", "encoding": "rle", "data": "8*4,8*1"},
"sprite": {"path": "assets/rider.png", "flip_x": false, "flip_y": false, "use_src": false, "src_x": 0, "src_y": 0, "src_w": 0, "src_h": 0},
"player": {"slot": 0, "speed": 160},
"camera": {"target": 2, "smoothing": 0.15, "zoom": 1, "use_bounds": false, "bounds_x": 0, "bounds_y": 0, "bounds_w": 1024, "bounds_h": 768},
"spawn": {"slot": 0},
"collider": {"x": 0, "y": 0, "w": 32, "h": 48, "type": "solid", "body": "dynamic"}
```

`collider` x/y are the offset from the entity's top-left corner, `type` is `solid` or `trigger`, and `body` is `static` or `dynamic`. The scene's `tile_solidity` lists the solid tile ids of each tileset that differs from the defaults (`""` = the built-in palette). A listed tileset uses exactly its list, and a toggle that brings a tileset back to its defaults drops its entry:

```json
"tile_solidity": [
  {"tileset": "", "solid": [3, 4, 5, 6, 8, 11, 12, 14, 15]},
  {"tileset": "assets/tiles.png", "solid": [7, 8, 9]}
]
```

Older files still load: a v1 entity named `TileMap*` becomes an empty 32 px grid covering its old rect, and a v1/v2 scene with no gameplay components gets a P1 controller on the entity named `Player` and a follow camera on `Camera2D` (targeting Player), so old projects play straight away. A v1-v3 scene gives every `player` entity a default dynamic collider (its whole rect) and starts with default tile solidity. A v4 rider without a collider stays a ghost. The file is written as v4 on the next save.

`data` is row-major run-length text: comma-separated `count*id` runs (a single cell is just `id`).

`--smoke` also runs the runtime headless: scripted input moves the player exactly speed x time at 60 Hz (diagonals clamped), the camera follows and respects bounds, spawn points place riders, pause holds the world and step runs exactly one tick, Play -> Stop in the editor restores the workspace byte-for-byte with undo history and `scene.json` (bytes and mtime) untouched, edit tools are locked while playing, and a v2 project opens and plays.

Collision `--smoke`: a rider stops flush on a solid tile with zero speed, slides along a wall while pushing diagonally (keeping exactly the parallel motion), never tunnels at 5000 px/s through a tile or a 4 px fence, is blocked by a static crate and shoves a dynamic one, and respects `tile_solidity` overrides. Two riders dropped inside each other separate half each and never overlap while charging head-on. A trigger fires one enter and one exit on the expected ticks. The overlay boxes are checked, along with the v3 -> v4 upgrade and a byte-identical v4 roundtrip. The editor half toggles a palette tile (one undo step, `scene.json` in sync through undo / redo), removes and edits a collider with undo / redo, then plays: the rider stops on the newly solid tile, rides into a trigger exactly once, collision edits are locked, and Stop leaves solidity and `scene.json` alone. `ts_game --smoke` checks P1's collider shows in the overlay and reports the box and trigger counts.

The editor `--smoke` also paints a multi-cell stroke and checks it is one undo step, a 2x2 brush, erase, a cancelled stroke, bucket fill, rect fill/erase, eyedropper, resize (keeps tiles; undo restores cut cells), tile size, undo-all / redo-all with `scene.json` in sync, the v2 RLE roundtrip, the v1 upgrade, sprite drop-create / assign / flip / source-rect roundtrip with undo/redo, missing and corrupt image fallbacks, tileset slicing, and import.

### Building admin with ImGui

Admin pulls **GLFW 3.4**, **Dear ImGui (docking branch)** and the single **stb_image.h** header via CMake `FetchContent` (`cmake/FetchImGuiDeps.cmake`); `ts_game` uses the same GLFW and stb_image (no ImGui). Downloaded sources live under the build directory (`build/_deps/...`) and are gitignored — they are **not** committed.

Requirements (in addition to C++20):

- OpenGL development libraries  
- On Linux: X11 (or Wayland) development packages for GLFW (`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`, `libgl1-mesa-dev`)  
- On Windows/MSVC: a working desktop OpenGL driver (Visual Studio 2019+ recommended)

```bash
cmake -S . -B build -DTS_BUILD_ADMIN=ON -DTS_BUILD_CLIENT=ON -DTS_BUILD_GAME=ON
cmake --build build --target ts_admin ts_game
./build/apps/admin/ts_admin          # GUI
./build/apps/admin/ts_admin --smoke  # headless flow smoke (no window, no GL)
./build/apps/game/ts_game --smoke --project <dir>
```

Only Admin fetches and links ImGui; Client fetches nothing. `-DTS_BUILD_ADMIN=OFF` skips `src/editor` and ImGui; with Admin and Game both off, no FetchContent deps are pulled at all.

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

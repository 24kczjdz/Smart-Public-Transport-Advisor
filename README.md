# Smart Public Transport Advisor

Desktop app (Qt 6 + C++) with a **Leaflet** map, **optional Google transit** (REST APIs from C++), and **local Hong Kong franchised-bus CSV** routing demos. The UI is embedded HTML (`QWebEngineView` + `QWebChannel`).

---

## Environment setup

This project needs **Qt 6** (with **Qt WebEngine**), **CMake** ≥ 3.16, and a **C++17** toolchain. You also need **Internet** when running the map app (Leaflet + tile CDN, and optional Google APIs).

### 1. Compiler

| OS | What to install |
|----|------------------|
| **macOS** | **Xcode** from the App Store (or [Xcode CLT](https://developer.apple.com/download/all/): `xcode-select --install`). |
| **Windows** | **Visual Studio 2022** with workload **Desktop development with C++**, or **Build Tools for Visual Studio** with MSVC and Windows SDK. |
| **Linux** | `g++` (e.g. Ubuntu: `sudo apt install build-essential`). |

### 2. CMake

- **macOS (Homebrew):** `brew install cmake`  
- **Windows:** Install from [cmake.org/download](https://cmake.org/download/) or `winget install Kitware.CMake`.  
- **Linux:** `sudo apt install cmake` (or your distro’s package).

Check: `cmake --version` (need ≥ 3.16).

### 3. Qt 6

You must have **Qt 6.x** with at least these libraries: **Qt6Core**, **Qt6Widgets**, **Qt6Network**, **Qt6WebEngineWidgets**, **Qt6WebChannel**. **Qt WebEngine** is required for the embedded map (it is included in a full Qt desktop install).

#### Option A — macOS: Homebrew (quickest)

```bash
brew install qt cmake
```

Qt is installed under `/opt/homebrew/opt/qt` (Apple Silicon) or `/usr/local/opt/qt` (Intel). CMake finds it with:

```bash
brew --prefix qt
```

If `cmake` is not on your `PATH`, use the full path, e.g. `/opt/homebrew/bin/cmake`.

#### Option B — Official Qt Online Installer (all platforms)

1. Open **[Download Qt (open source)](https://www.qt.io/download-open-source)** and download the **Qt Online Installer** for your OS.  
2. Create / sign in with a **Qt Account** (free).  
3. In the installer, select **Qt 6.8** or **6.9** (or another **6.x LTS / latest** that matches your course).  
4. Expand the kit for your compiler, e.g. **macOS** → *Qt 6.x.x for macOS*, **Windows** → *MSVC 2022 64-bit*, **Linux** → *GCC 64-bit*.  
5. Enable components (names vary slightly by version):  
   - **Qt WebEngine** (often under the same kit)  
   - **Qt 5 Compatibility Module** is *not* required for this repo  
6. Complete the install and note the path, e.g.  
   - macOS: `~/Qt/6.x.x/macos`  
   - Windows: `C:\Qt\6.x.x\msvc2022_64`  
   - Linux: `~/Qt/6.x.x/gcc_64`

Pass that directory to CMake as **`CMAKE_PREFIX_PATH`** (see **Build** below). Official guide: [Qt 6 Get Started](https://doc.qt.io/qt-6/gettingstarted.html).

#### Qt Creator (optional)

Install **Qt Creator** from the same installer if you want a GUI: open the project folder, select the **Kit** that matches your Qt 6 + compiler, then **Build** / **Run**.

### 4. Verify Qt for CMake

After install, you should have a file like:

`…/Qt/6.x.x/<kit>/lib/cmake/Qt6/Qt6Config.cmake`

The parent of `lib` (the **kit root**) is what you pass as `-DCMAKE_PREFIX_PATH=`.

---

## Build

```bash
cd TransportAdvisor
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<kit>
cmake --build build
```

**macOS (Homebrew Qt):**

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```

**Windows (example, adjust version path):**

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake --build build --config Release
```

---

## Run

**macOS (.app bundle):**

```bash
open build/TransportAdvisor.app
```

**Executable path:**

`build/TransportAdvisor.app/Contents/MacOS/TransportAdvisor`

After changing `html/map.html`, **rebuild** so Qt’s resource file (`resources.qrc`) picks up the update.

---

## Graph journey planner — coursework CSV model

The **Stop / Segment / Journey** model from your spec is in **`PtGraphAdvisor`**: load `stops.csv` + `segments.csv`, **DFS** simple paths (unique by stop sequence, `max_segments` default 8), rank by:

| Preference | Sort key |
|------------|----------|
| `cheapest` | total cost, then time |
| `fastest` | total time, then cost |
| `fewest_segments` | segment count, then time |
| `fewest_transfers` | mode-change count, then time |

**Transfer count** = number of adjacent segment pairs where `mode` differs (same rule as the Python reference).

### In the desktop app (interactive)

Run **TransportAdvisor** and use the **Graph journey planner** dock (right side by default). Choose **origin** and **destination** from the dropdowns, pick a **preference**, then **Find top journeys**. The app looks for `data/case1/stops.csv` and `data/case1/segments.csv` next to the binary, under the build tree, or from the current working directory (same rules as below).

### Optional: `pt_advisor` CLI (scripts / marking)

Non-interactive one-shot query (no stdin menu):

```bash
cmake --build build --target pt_advisor
./build/pt_advisor data/case1/stops.csv data/case1/segments.csv CEN_MTR WCH_MTR cheapest
```

Arguments: `stops.csv` `segments.csv` `origin_stop_id` `dest_stop_id` `[preference]`. For usage help, run `pt_advisor` with fewer than five arguments; it will remind you to use the desktop app for interactive input.

Sample **Case 1** network is under `data/case1/`. Segment files may include `#` comment lines and blank lines (ignored). Headers accept `stop_id,stop_name,type` and optional `region`; segments use `from,to,duration,cost,mode` (aliases `from_stop` / `to_stop` supported).

**Relation to the Qt map app:** `LocalBusCsvModel` still uses **real TD bus CSVs** for map polylines (heuristic). `PtGraphAdvisor` is the **explicit graph + DFS** layer; the map app embeds the same planner in the **Graph journey planner** dock.

---

## Configuration

Copy `config.ini.example` to **`TransportAdvisor.app/Contents/MacOS/config.ini`** (same folder as the binary), or place `config.ini` / `TransportAdvisor.ini` in the current working directory or next to the binary (see `MainWindow.cpp` search paths).

| Setting | Environment variable | Purpose |
|---------|----------------------|---------|
| `GOOGLE_MAPS_API_KEY` | `GOOGLE_MAPS_API_KEY` | Optional. Required only for **Plan (Google)** (Geocoding + Directions REST). **Maps JavaScript API is not used.** |
| `DATA_CSV_DIR` | `TRANSPORT_ADVISOR_CSV_DIR` | Optional. Absolute path to folder containing `STOP_BUS.csv`, `ROUTE_BUS.csv`, `RSTOP_BUS.csv`. |

If `DATA_CSV_DIR` is unset, the app walks upward from the executable looking for:

`Data/Bus,MiniBus,Tram/CSV/`

(e.g. parent **Mapper** repo layout: `Mapper/Data/Bus,MiniBus,Tram/CSV/`).

---

## Using the app

1. **Starting from** — where you are: type an address or use **latitude, longitude** (default is a Hong Kong centre point; **Locate me** fills your GPS coordinates into this field).
2. **Destination** — where you want to go (same validation; for **Plan from CSV** use a place name so stops can be matched, not coordinates only).
3. **Locate me** — browser geolocation updates the map marker and the **Starting from** field.
4. **Highlight on map** — choose whether the top suggestion is ranked by time, fare, or number of segments.
5. **Plan (Google)** — geocode both endpoints (when needed) + transit directions; results ranked in the three lists; polylines decoded in JS.
6. **Random CSV routes** — loads bus CSVs and shows random route segments (demo).
7. **Plan from CSV** — matches **destination** text to stop/route names, **nearest stop to your starting point** (coordinates or geocoded address), heuristic paths; ranks similarly.
8. **Graph journey planner** — dock panel (`data/case1` graph): pick origin and destination stops, preference, **Find top journeys** (same DFS model as `pt_advisor`).

On startup, a short delay triggers **Random CSV routes** once (if the bridge and data are ready).

Errors from the C++ side may appear in a **native message box**; validation errors also show in the sidebar.

---

## Map tiles (why not `tile.openstreetmap.org`?)

Qt **WebEngine** pages loaded from `qrc:` often send **no `Referer`** header. OpenStreetMap’s **standard tile server** may respond with **403** (“Referer required”). This project uses **CARTO** raster tiles (OSM data, separate CDN) so the map works without extra C++ request interceptors. Attribution remains OSM + CARTO per their terms.

---

## Project layout

| Path | Role |
|------|------|
| `CMakeLists.txt` | Qt6 target, sources, resources |
| `main.cpp` | `QApplication` entry |
| `MainWindow.*` | Window, `QWebEngineView`, `QWebChannel`, load `map.html`, read config, coursework graph dock |
| `TransportBridge.*` | Exposed to JS as `transport`: validation, Google flow, CSV preview/plan |
| `RouteService.*` | `QNetworkAccessManager` → Geocoding + Directions JSON |
| `RouteRanker.*` | Normalises Google routes into `byTime` / `byCost` / `byTransfers` |
| `LocalBusCsvModel.*` | Parses TD-style bus CSVs, builds paths and rankings |
| `DestinationValidator.*` | Destination string rules |
| `HkGridToWgs84.h` | Approximate HK grid → WGS84 for bus stop coordinates |
| `html/map.html` | Leaflet UI, polyline decode for Google, calls into C++ |
| `resources.qrc` | Embeds `map.html` |
| `web/index.html` | Standalone browser demo (Google **JavaScript** Directions only; not used by Qt build) |
| `PtGraphAdvisor.*`, `PtGraphTypes.*` | Coursework graph: CSV load, DFS journeys, preference ranking |
| `pt_advisor_main.cpp` | Console menu → `pt_advisor` executable |
| `data/case1/*.csv` | Example network (10 stops, 26 directed segments) |
| `config.ini.example` | Sample configuration |

---

## Standalone browser demo

`web/index.html` can be opened in a normal browser for quick tests. It expects a **Maps JavaScript API** key in the page (Google client-side Directions). The Qt application does **not** depend on that file.

---

## Mobile / WebAssembly

- **Android:** Possible with Qt 6 + WebEngine; verify on device; **INTERNET** permission required.
- **iOS:** Qt **WebEngine is not supported** on iOS. Use **Qt WebView** / `WKWebView` loading the same HTML, or keep logic in a static library and wrap natively.
- **WebAssembly:** WebEngine story differs; expect substantial porting.

---

## Limitations (coursework / demo scope)

- **CSV mode:** Graph search is **heuristic / random-ish**, not guaranteed optimal. **HK grid → lat/lng** is **approximate** (calibrated sample), not survey-grade.
- **Google mode:** Depends on Google coverage and fares; many transit responses omit fare text.
- **MTR CSVs** in `Mapper/MTR/` are **not** wired into this app’s router yet (no coordinates in those files as used here).

---

## Licence / data

- Map tiles: follow **CARTO** and **OpenStreetMap** attribution and applicable terms.
- Bus CSVs: use per your source (e.g. Hong Kong Transport Department open data) and MTR open data where applicable.

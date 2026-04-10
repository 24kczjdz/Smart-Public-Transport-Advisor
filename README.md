# Smart Public Transport Advisor

Desktop app (Qt 6 + C++) with a **Leaflet** map, **optional Google transit** (REST APIs from C++), and **local Hong Kong franchised-bus CSV** routing demos. The UI is embedded HTML (`QWebEngineView` + `QWebChannel`).

---

## What you need

| Requirement | Notes |
|-------------|--------|
| **Qt 6** | Components: Core, Widgets, Network, WebEngineWidgets, WebChannel |
| **CMake** ≥ 3.16 | e.g. `brew install cmake` |
| **C++17 compiler** | Xcode / MSVC / GCC |
| **Internet** | Loads Leaflet, map tiles, and (if used) Google APIs |

---

## Build

```bash
cd TransportAdvisor
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos
cmake --build build
```

**macOS (Homebrew Qt):**

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
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

1. **Locate me** — browser geolocation for your position (marker on map).
2. **Destination** — validated (non-empty, 3–200 characters, no control characters).
3. **Highlight on map** — choose whether the top suggestion is ranked by time, fare, or number of segments.
4. **Plan (Google)** — geocode destination + transit directions; results ranked in the three lists; polylines decoded in JS.
5. **Random CSV routes** — loads bus CSVs and shows random route segments (demo).
6. **Plan from CSV** — matches destination text to stop/route names, nearest stop to you, heuristic paths; ranks similarly.

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
| `MainWindow.*` | Window, `QWebEngineView`, `QWebChannel`, load `map.html`, read config |
| `TransportBridge.*` | Exposed to JS as `transport`: validation, Google flow, CSV preview/plan |
| `RouteService.*` | `QNetworkAccessManager` → Geocoding + Directions JSON |
| `RouteRanker.*` | Normalises Google routes into `byTime` / `byCost` / `byTransfers` |
| `LocalBusCsvModel.*` | Parses TD-style bus CSVs, builds paths and rankings |
| `DestinationValidator.*` | Destination string rules |
| `HkGridToWgs84.h` | Approximate HK grid → WGS84 for bus stop coordinates |
| `html/map.html` | Leaflet UI, polyline decode for Google, calls into C++ |
| `resources.qrc` | Embeds `map.html` |
| `web/index.html` | Standalone browser demo (Google **JavaScript** Directions only; not used by Qt build) |
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

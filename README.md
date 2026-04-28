# Smart Public Transport Advisor

A Python-based transit network search tool that models Hong Kong public transport (MTR, Bus, Walk) as a directed weighted graph. Given an origin and destination stop, it finds all simple paths using depth-first search and ranks them by travel time, cost, or number of segments.

---

## Language and Environment

| Item | Detail |
|---|---|
| Language | Python 3.10 or later |
| Platform | Windows / macOS / Linux |
| Dependencies | `networkx`, `matplotlib`, `PyQt6` |

No compilation is required. All source files are plain `.py` scripts run directly with the Python interpreter.

---

## Project Structure

```
networkModel/
├── nv02.py                    # Core library: network loading, DFS pathfinding, ranking
├── map/
│   ├── stop01.txt             # Stops data – small HK Island sample (10 stops)
│   ├── seg01.txt              # Segments data – small HK Island sample (27 segments)
│   ├── stop02.txt             # Stops data – abstract cardinal-direction network
│   ├── seg02.txt              # Segments data – abstract cardinal-direction network
│   ├── stop07.txt             # Stops data – extended HK network (46 stops)
│   ├── seg07.txt              # Segments data – extended HK network
│   ├── stop404.txt            # For error handling testing – a duplicate of stop02.txt
│   ├── seg404.txt             # For error handling testing - a dulicate of seg02.txt but missing line information
│   ├── stop502.txt            # For error handling testing – a duplicate of seg02.txt, instead of stop02.txt
│   └── seg502.txt             # For error handling testing - a dulicate of stop02.txt, instead of seg02.txt
├── graph_demo/
│   ├── app.py                 # Main program: PyQt6 interactive GUI
│   └── requirements.txt       # Python package dependencies
└── README.md                  # This file
```

Adding a new network is as simple as placing a `stop<id>.txt` and `seg<id>.txt` pair in `map/`. The app detects them automatically on startup.

### File descriptions

| File | Purpose |
|---|---|
| `nv02.py` | Core module used by `app.py`. Provides `load_network` (reads CSV files), `find_journeys` (DFS path search), and `rank_and_print` (sort and display results). |
| `map/stop*.txt` | Stops CSV files. Each row defines a stop with a unique ID, human-readable name, and transport type (`MTR` or `Bus`). |
| `map/seg*.txt` | Segments CSV files. Each row defines a directed edge between two stops with duration (minutes), cost (HKD), and mode (`MTR`, `Bus`, or `Walk`). |
| `graph_demo/app.py` | **Main program.** Interactive GUI for loading networks, querying routes, and visualising the network graph with the selected route highlighted. |
| `graph_demo/requirements.txt` | Lists `networkx`, `matplotlib`, and `PyQt6` with minimum version constraints. |

---

## Setup

Install the required packages once before running:

```bash
pip install -r graph_demo/requirements.txt
```

---

## Running the Program

```bash
python graph_demo/app.py
```

The app loads `map/stop01.txt` and `map/seg01.txt` by default. Use the **Select map files** button or edit the path fields manually to switch to a different network.

### Workflow

1. **Select a map set** from the **Map set** drop-down (e.g. `01`, `02`, `07`). The app scans the `map/` directory automatically and loads the network immediately. Alternatively, type file paths manually and click **Load txt files**, or use **Select map files…** to browse.
2. Choose **Origin** and **Destination** from the drop-down lists.
3. Choose a **Preference** (`fastest`, `cheapest`, `fewest_segments`, or `fewest_transfers`).
4. Set **Path index** to select which ranked journey to highlight on the graph (0 = best).
5. Click **Compute route** — route summaries appear in the text area and the selected route is highlighted on the graph canvas in the colour of each transit line.

---

## Map File Format

### Stops file (`stop*.txt`)

CSV with a header row. Each row defines one stop.

```
stop_id,stop_name,type
CEN_MTR,Central,MTR
CEN_BUS,Central,Bus
```

| Field | Description |
|---|---|
| `stop_id` | Unique identifier referenced in segments |
| `stop_name` | Human-readable station name |
| `type` | `MTR` or `Bus` |

### Segments file (`seg*.txt`)

CSV with a header row. Each row defines one directed edge.

```
from,to,duration,cost,mode,line
CEN_MTR,ADM_MTR,2,8.0,MTR,Island_Line
CEN_MTR,CEN_BUS,3,0.0,Walk,Walk
```

| Field | Description |
|---|---|
| `from` | Origin stop ID |
| `to` | Destination stop ID |
| `duration` | Travel time in minutes (integer) |
| `cost` | Fare in HKD (float) |
| `mode` | `MTR`, `Bus`, or `Walk` |
| `line` | Transit line name (e.g. `Island_Line`, `Route_40`); use `Walk` for walking segments |

Segments are **directed** — both directions must be listed explicitly. Walk segments connecting MTR and Bus stops at the same station always have `cost = 0.0` and `line = Walk`.

The `line` field is used to count transfers (a transfer occurs when consecutive non-Walk segments are on different lines) and to colour-code route edges in the graph.

---

## Sample Test Cases

All test cases use map set 07 (`map/stop07.txt` and `map/seg07.txt`). Start the app with `python graph_demo/app.py`, load those two files, then follow the steps for each case.

---

### Test 1 – Fastest route - from a “last-minute student” perspective

**Purpose:** Verify that the `fastest` preference returns the lowest-duration path.

**Steps:**
- Origin: `EAST_TST_MTR`, Destination: `KEN_MTR`, Preference: `fastest`

**Expected:** 

#1: EAST_TST_MTR -> HUNG_HOM_MTR -> ADM_MTR -> CEN_MTR -> HKU_MTR -> KEN_MTR
  Time=19 min, Cost=HKD 42.00, Transfers=2, Segments=5

  -> Hung Hom               [Tuen_Ma_Line]  2 min  HKD 8.00
  ~~ TRANSFER: Tuen_Ma_Line -> East_Rail_Line
  -> Admiralty              [East_Rail_Line]  8 min  HKD 10.00
  ~~ TRANSFER: East_Rail_Line -> Island_Line
  -> Central                [Island_Line]  2 min  HKD 8.00
  -> HKU                    [Island_Line]  4 min  HKD 8.00
  -> Kennedy Town           [Island_Line]  3 min  HKD 8.00

Top route is the MTR path `East Tsim Sha Tsui → Hung Hom [Tuen_Ma_Line] Hung Hom → Admiralty [East_Rail_Line] Admiralty → Kennedy Town [Island_Line]`, 19 min only.

---

### Test 2 – Cheapest route with Walk transfer - from a “budget commuter” perspective

**Purpose:** Verify that the `cheapest` preference minimises total cost and correctly incorporates zero-cost Walk segments.

**Steps:**
- Origin: `EAST_TST_MTR`, Destination: `KEN_MTR`, Preference: `cheapest`

**Expected:** 

#1: EAST_TST_MTR -> EAST_TST_BUS -> HUNG_HOM_BUS -> ADM_BUS -> CEN_BUS -> HKU_BUS -> KEN_BUS -> KEN_MTR
  Time=47 min, Cost=HKD 28.00, Transfers=3, Segments=7

  -> East Tsim Sha Tsui     [Walk]  3 min
  -> Hung Hom               [Route_1]  5 min  HKD 5.00
  ~~ TRANSFER: Route_1 -> Route_107
  -> Admiralty              [Route_107]  15 min  HKD 8.00
  ~~ TRANSFER: Route_107 -> Route_40
  -> Central                [Route_40]  5 min  HKD 5.00
  ~~ TRANSFER: Route_40 -> Route_5B
  -> HKU                    [Route_5B]  9 min  HKD 5.00
  -> Kennedy Town           [Route_5B]  7 min  HKD 5.00
  -> Kennedy Town           [Walk]  3 min

Top route is the bus path `East Tsim Sha Tsui → Hung Hom [Route_1] Hung Hom → Admiralty [Route_107] Admiralty → Central [Route_40] Central → Kennedy Town [Route_5B]`, with walk transfer between mtr and bus at East Tsim Sha Tsui and Kennedy Town. Cost HKD 28.00 only.

---

### Test 3 – Fewest transfer - from a “transfer-averse user” perspective

**Purpose:** Verify that the `fewest_transfers` preference minimises the number of transfers regardless of cost or time or segments.

**Steps:**
- Origin: `EAST_TST_MTR`, Destination: `KEN_MTR`, Preference: `fewest_transfers`

**Expected:** 
#1: EAST_TST_MTR -> AUSTIN_MTR -> NAN_CHANG_MTR -> HONG_KONG_MTR -> CEN_MTR -> HKU_MTR -> KEN_MTR
  Time=21 min, Cost=HKD 46.00, Transfers=1, Segments=6

  -> Austin                 [Tuen_Ma_Line]  2 min  HKD 8.00
  -> Nam Cheong             [Tuen_Ma_Line]  2 min  HKD 8.00
  -> Hong Kong              [Tuen_Ma_Line]  5 min  HKD 14.00
  -> Central                [Walk]  5 min
  ~~ TRANSFER: Tuen_Ma_Line -> Island_Line
  -> HKU                    [Island_Line]  4 min  HKD 8.00
  -> Kennedy Town           [Island_Line]  3 min  HKD 8.00

Top route is the MTR path `East Tsim Sha Tsui → Hong Kong [Tuen_Ma_Line] Hong Kong → Central [walk] Central → Kennedy Town [Island_Line]`, 1 transfers only.

---

### Test 4 – Fewest segments

**Purpose:** Verify that the `fewest_segments` preference minimises the number of stops regardless of cost or time or transfers.

**Steps:**
- Origin: `EAST_TST_MTR`, Destination: `KEN_MTR`, Preference: `fewest_segments`

**Expected:** 
#1: EAST_TST_MTR -> HUNG_HOM_MTR -> ADM_MTR -> CEN_MTR -> HKU_MTR -> KEN_MTR
  Time=19 min, Cost=HKD 42.00, Transfers=2, Segments=5

  -> Hung Hom               [Tuen_Ma_Line]  2 min  HKD 8.00
  ~~ TRANSFER: Tuen_Ma_Line -> East_Rail_Line
  -> Admiralty              [East_Rail_Line]  8 min  HKD 10.00
  ~~ TRANSFER: East_Rail_Line -> Island_Line
  -> Central                [Island_Line]  2 min  HKD 8.00
  -> HKU                    [Island_Line]  4 min  HKD 8.00
  -> Kennedy Town           [Island_Line]  3 min  HKD 8.00

  Top route is the MTR path `East Tsim Sha Tsui → Hung Hom [Tuen_Ma_Line] Hung Hom → Admiralty [East_Rail_Line] Admiralty → Kennedy Town [Island_Line]`, 5 segments only.

---

## Error Handling

### 01 – Invalid input: same origin and destination

**Purpose:** Verify that the program handles invalid input gracefully without crashing.

**Steps:**
- Set both Origin and Destination to the same stop (e.g., `CEN_MTR`)
- Click **Compute route**

**Expected:** A warning dialog appears stating "Origin and destination must differ." No crash occurs.

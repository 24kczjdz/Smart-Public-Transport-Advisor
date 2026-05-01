import argparse
import csv
import pathlib
from collections import defaultdict

# ------------------------------------------------------------
# 0. Command-line argument helpers
# ------------------------------------------------------------

def parse_args(argv=None):
    """Parse and return command-line arguments.

    Args:
        argv: Argument list to parse; defaults to sys.argv when None.

    Returns:
        argparse.Namespace with fields: stops, segments, origin, dest,
        preference, max_paths, max_len.
    """
    parser = argparse.ArgumentParser(description="nv02 network search helper")
    parser.add_argument("--stops", default="stops.txt", help="Path to stops CSV file")
    parser.add_argument("--segments", default="segments.txt", help="Path to segments CSV file")
    parser.add_argument("--origin", help="Origin stop ID")
    parser.add_argument("--dest", help="Destination stop ID")
    parser.add_argument("--preference", choices=["cheapest", "fastest", "fewest_stops", "fewest_transfers"], default="fastest",
                        help="Journey preference")
    parser.add_argument("--max-paths", type=int, default=200, help="Maximum number of paths to find")
    parser.add_argument("--max-len", type=int, default=8, help="Maximum number of segments per path")
    return parser.parse_args(argv)

# ------------------------------------------------------------
# 1. Load network from files
# ------------------------------------------------------------
def load_network(stops_file, segments_file):
    """Load stops and directed segments from CSV files.

    Args:
        stops_file:    Path to stops CSV with columns: stop_id, stop_name, type.
        segments_file: Path to segments CSV with columns: from, to, duration, cost, mode, line.

    Returns:
        stops: dict mapping stop_id -> (stop_name, type).
        graph: defaultdict(list) mapping stop_id -> [(to, duration, cost, mode, line), ...].

    Raises:
        FileNotFoundError: If either file does not exist.
        ValueError: If either file is empty or has no valid data.
    """
    stops = {}
    with open(stops_file) as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError(f"Stops file '{stops_file}' is empty or contains no valid data.")
        required = {'stop_id', 'stop_name', 'type'}
        missing = required - set(reader.fieldnames)
        if missing:
            raise ValueError(
                f"Stops file '{stops_file}' is missing required column(s): {', '.join(sorted(missing))}.\n"
                f"Expected columns: stop_id, stop_name, type"
            )
        for row in reader:
            stops[row['stop_id']] = (row['stop_name'], row['type'])
    if not stops:
        raise ValueError(f"Stops file '{stops_file}' is empty or contains no valid data.")

    graph = defaultdict(list)
    skipped = []
    with open(segments_file) as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError(f"Segments file '{segments_file}' is empty or contains no valid data.")
        required = {'from', 'to', 'duration', 'cost', 'mode', 'line'}
        missing = required - set(reader.fieldnames)
        #error handling
        if missing:
            raise ValueError(
                f"Segments file '{segments_file}' is missing required column(s): {', '.join(sorted(missing))}.\n"
                f"Expected columns: from, to, duration, cost, mode, line"
            )
        for row in reader:
            frm, to = row['from'], row['to']
            if frm in stops and to in stops:
                graph[frm].append((to, int(row['duration']), float(row['cost']), row['mode'], row['line']))
            else:
                # Segment references a stop ID not present in stops_file
                skipped.append((frm, to))
    if skipped:
        print(f"Warning: {len(skipped)} segment(s) skipped (stop ID not found): {skipped}")
    return stops, graph

# ------------------------------------------------------------
# 2. Auto map discovery
# ------------------------------------------------------------
def scan_map_sets(base_dir=None):
    """Scan for stop/segment file pairs in the map/ subdirectory.

    Args:
        base_dir: Base directory to search; defaults to the directory of this
                  script when None.

    Returns:
        Sorted list of (suffix, stops_path, seg_path) tuples, e.g.
        [('01', 'map/stop01.txt', 'map/seg01.txt'), ...].
    """
    if base_dir is None:
        base_dir = pathlib.Path(__file__).resolve().parent
    map_dir = pathlib.Path(base_dir) / "map"
    if not map_dir.is_dir():
        return []
    results = []
    for stop_file in sorted(map_dir.glob("stop*.txt")):
        suffix = stop_file.stem[4:]   # strip leading 'stop'
        seg_file = map_dir / f"seg{suffix}.txt"
        if seg_file.exists():
            results.append((suffix, str(stop_file), str(seg_file)))
    return results

# ------------------------------------------------------------
# 3. Generate all simple paths using DFS (with duplicate prevention)
# ------------------------------------------------------------
def find_journeys(graph, origin, dest, max_len=8, max_paths=200):
    """Find all simple paths from origin to dest using depth-first search.

    Args:
        graph:     Adjacency list as returned by load_network.
        origin:    Starting stop ID.
        dest:      Target stop ID.
        max_len:   Maximum number of segments allowed per path.
        max_paths: Hard cap on total paths collected; search stops early when reached.

    Returns:
        List of paths, each path being a list of (stop_id, duration, cost, mode, line)
        tuples representing each segment taken from origin to dest.
        Duplicate node sequences are excluded.
    """
    all_paths = []
    seen_sequences = set()

    def dfs(current, path_nodes, path_edges, visited):
        """Recursively explore neighbours; backtrack after each branch.

        Args:
            current:    Stop ID being visited.
            path_nodes: Ordered list of stop IDs on the current path (for dedup).
            path_edges: Ordered list of segment tuples on the current path.
            visited:    Set of stop IDs already on the current path (prevents cycles).
        """
        if len(all_paths) >= max_paths:
            return
        if current == dest:
            seq_key = tuple(path_nodes)
            if seq_key not in seen_sequences:
                seen_sequences.add(seq_key)
                all_paths.append(path_edges.copy())
            return
        if len(path_edges) >= max_len:
            return
        for nxt, dur, cost, mode, line in graph.get(current, []):
            if nxt not in visited:
                visited.add(nxt)
                path_nodes.append(nxt)
                path_edges.append((nxt, dur, cost, mode, line))
                dfs(nxt, path_nodes, path_edges, visited)
                # Backtrack
                path_edges.pop()
                path_nodes.pop()
                visited.remove(nxt)

    dfs(origin, [origin], [], {origin})
    return all_paths

# ------------------------------------------------------------
# 3. Rank journeys and print results
# ------------------------------------------------------------
def rank_and_print(journeys, stops, origin, dest, preference, top_k=3):
    """Sort journeys by preference and print the top results.

    Transfer count is computed by counting line changes among non-Walk segments
    (Walk legs are excluded because they are not considered transit transfers).
    Using line rather than mode means switching between two MTR lines (e.g.
    Island Line → East Rail Line) is correctly counted as one transfer.

    Args:
        journeys:   List of paths as returned by find_journeys.
        stops:      stops dict as returned by load_network.
        origin:     Starting stop ID (used for display).
        dest:       Destination stop ID (unused in output but kept for consistency).
        preference: Sorting strategy – 'cheapest', 'fastest', or 'fewest_stops'.
        top_k:      Number of top journeys to display (default 3).
    """
    if not journeys:
        print("No journey found.")
        return

    # Compute statistics for each journey
    scored = []
    for path in journeys:
        dur = sum(seg[1] for seg in path)
        cost = sum(seg[2] for seg in path)
        # Count line changes among transit (non-Walk) segments only
        non_walk = [seg for seg in path if seg[3] != 'Walk']
        transfers = sum(1 for i in range(1, len(non_walk)) if non_walk[i][4] != non_walk[i-1][4])
        scored.append((dur, cost, transfers, path))

    # Sort according to preference; use secondary key to break ties
    if preference == 'cheapest':
        scored.sort(key=lambda x: (x[1], x[0]))          # primary: cost, secondary: time
    elif preference == 'fastest':
        scored.sort(key=lambda x: (x[0], x[1]))          # primary: time, secondary: cost
    elif preference == 'fewest_transfers':
        scored.sort(key=lambda x: (x[2], x[0]))          # primary: transfers, secondary: time
    else:  # fewest_stops
        scored.sort(key=lambda x: (len(x[3]), x[0]))     # primary: segment count, secondary: time

    def fmt(sid):
        name, typ = stops[sid]
        return f"{name} ({typ})"

    print(f"\n=== Top {min(top_k, len(scored))} journeys (preference: {preference}) ===")
    for i, (dur, cost, transfers, path) in enumerate(scored[:top_k], 1):
        seq = [fmt(origin)] + [fmt(seg[0]) for seg in path]
        print(f"\n#{i}")
        print(f"Route: {' -> '.join(seq)}")
        print(f"Time: {dur} min, Cost: HKD {cost:.2f}, Transfers: {transfers}")
        for seg in path:
            stop_name, _ = stops[seg[0]]
            print(f"  -> {stop_name} ({seg[3]}/{seg[4]}, {seg[1]} min, HKD {seg[2]:.2f})")
    print()

# ------------------------------------------------------------
# 4. Main menu
# ------------------------------------------------------------
def main():
    """Run the interactive text menu for exploring the network.

    Attempts to load 'stops.txt' and 'segments.txt' from the current directory
    on startup. If the files are missing, the user can load them via option 4.

    Menu options:
        1 – List all stops
        2 – Query journeys between two stops
        3 – Show network summary (stop/segment counts by mode)
        4 – Load a different network from specified file paths
        5 – Exit
    """
    stops = {}
    graph = defaultdict(list)

    # Attempt to load default files; not required to start
    try:
        stops, graph = load_network("stops.txt", "segments.txt")
        print(f"Loaded default network: {len(stops)} stops, {sum(len(v) for v in graph.values())} directed segments")
    except (FileNotFoundError, ValueError):
        print("No default network found.")
        map_sets = scan_map_sets()
        if map_sets:
            print("Auto-discovered map sets:")
            for i, (suffix, sp, _seg) in enumerate(map_sets, 1):
                print(f"  {i}. map set '{suffix}'  ({sp})")
            sel = input("Load a map set (number) or press Enter to skip: ").strip()
            if sel.isdigit() and 1 <= int(sel) <= len(map_sets):
                _, sp, seg = map_sets[int(sel) - 1]
                try:
                    stops, graph = load_network(sp, seg)
                    print(f"Loaded: {len(stops)} stops, {sum(len(v) for v in graph.values())} directed segments")
                except (FileNotFoundError, ValueError) as e:
                    print(f"Error: {e}")
        else:
            print("Use option 4 to load a network file.")

    while True:
        print("\n" + "="*40)
        print()
        print("1. List all stops")
        print("2. Query journeys")
        print("3. Network summary")
        print("4. Load new network")
        print("5. Exit")
        choice = input("Choose (1-5): ").strip()

        if choice in ('1', '2', '3') and not stops:
            print("No network loaded. Please use option 4 to load a network file first.")
        elif choice == '1':
            print("\nStop list:")
            for sid, (name, typ) in sorted(stops.items()):
                print(f"  {sid}: {name} ({typ})")
        elif choice == '2':
            print("\nAvailable stop IDs:", ", ".join(sorted(stops.keys())))
            origin = input("Origin stop ID: ").strip()
            dest = input("Destination stop ID: ").strip()
            if origin not in stops or dest not in stops:
                print("Error: stop does not exist")
                continue
            if origin == dest:
                print("Error: origin and destination are the same")
                continue
            _pref_options = ['fastest', 'cheapest', 'fewest_stops', 'fewest_transfers']
            print("Preference:  1. fastest  2. cheapest  3. fewest_stops  4. fewest_transfers")
            pref_raw = input("Choose (1-4 or name): ").strip().lower()
            if pref_raw in ('1', '2', '3', '4'):
                pref = _pref_options[int(pref_raw) - 1]
            elif pref_raw in _pref_options:
                pref = pref_raw
            else:
                pref = 'fastest'
                print("Invalid preference, using 'fastest'")

            paths = find_journeys(graph, origin, dest, max_len=8)
            rank_and_print(paths, stops, origin, dest, pref)
        elif choice == '3':
            print(f"\nNumber of stops: {len(stops)}")
            print(f"Number of directed segments: {sum(len(v) for v in graph.values())}")
            mode_cnt = defaultdict(int)
            for edges in graph.values():
                for _, _, _, mode, _line in edges:
                    mode_cnt[mode] += 1
            for mode, cnt in mode_cnt.items():
                print(f"  {mode}: {cnt} segments")
        elif choice == '4':
            map_sets = scan_map_sets()
            if map_sets:
                print("\nAuto-discovered map sets:")
                for i, (suffix, sp, _seg) in enumerate(map_sets, 1):
                    print(f"  {i}. map set '{suffix}'  ({sp})")
                print("  0. Enter file paths manually")
                sel = input("Select map set (0 to enter manually): ").strip()
                if sel.isdigit() and 1 <= int(sel) <= len(map_sets):
                    _, new_stops_file, new_segs_file = map_sets[int(sel) - 1]
                else:
                    new_stops_file = input("Stops file path: ").strip()
                    new_segs_file = input("Segments file path: ").strip()
            else:
                new_stops_file = input("Stops file path: ").strip()
                new_segs_file = input("Segments file path: ").strip()
            try:
                new_stops, new_graph = load_network(new_stops_file, new_segs_file)
                stops, graph = new_stops, new_graph
                print(f"Loaded: {len(stops)} stops, {sum(len(v) for v in graph.values())} directed segments")
            except (FileNotFoundError, ValueError) as e:
                print(f"Error: {e}. Network unchanged.")
        elif choice == '5':
            print("Goodbye!")
            break
        else:
            print("Invalid choice")
        input("\nPress Enter to continue...")

# ------------------------------------------------------------
# 5. Entry point – CLI or interactive menu
# ------------------------------------------------------------
def cli_main():
    """Program entry point; selects between CLI mode and interactive menu.

    If both --origin and --dest are supplied on the command line, runs a
    single non-interactive query and exits. Otherwise, launches the
    interactive text menu (main()).

    Exits with code 1 if the network files cannot be loaded in CLI mode.
    """
    args = parse_args()
    if args.origin and args.dest:
        try:
            stops, graph = load_network(args.stops, args.segments)
        except (FileNotFoundError, ValueError) as exc:
            print(f"Error: {exc}")
            raise SystemExit(1)

        paths = find_journeys(graph, args.origin, args.dest, max_len=args.max_len, max_paths=args.max_paths)
        rank_and_print(paths, stops, args.origin, args.dest, args.preference)
        return

    main()

if __name__ == "__main__":
    cli_main()

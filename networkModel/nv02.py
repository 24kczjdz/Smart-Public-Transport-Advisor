import csv
from collections import defaultdict

# ------------------------------------------------------------
# 1. Load network from files
# ------------------------------------------------------------
def load_network(stops_file, segments_file):
    """Return (stops_dict, graph_dict)"""
    stops = {}
    with open(stops_file) as f:
        reader = csv.DictReader(f)
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
        for row in reader:
            frm, to = row['from'], row['to']
            if frm in stops and to in stops:
                graph[frm].append((to, int(row['duration']), float(row['cost']), row['mode']))
            else:
                skipped.append((frm, to))
    if skipped:
        print(f"Warning: {len(skipped)} segment(s) skipped (stop ID not found): {skipped}")
    return stops, graph

# ------------------------------------------------------------
# 2. Generate all simple paths using DFS (with duplicate prevention)
# ------------------------------------------------------------
def find_journeys(graph, origin, dest, max_len=8, max_paths=200):
    """Return list of unique paths (by node sequence), capped at max_paths."""
    all_paths = []
    seen_sequences = set()

    def dfs(current, path_nodes, path_edges, visited):
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
        for nxt, dur, cost, mode in graph.get(current, []):
            if nxt not in visited:
                visited.add(nxt)
                path_nodes.append(nxt)
                path_edges.append((nxt, dur, cost, mode))
                dfs(nxt, path_nodes, path_edges, visited)
                path_edges.pop()
                path_nodes.pop()
                visited.remove(nxt)

    dfs(origin, [origin], [], {origin})
    return all_paths

# ------------------------------------------------------------
# 3. Rank journeys and print results
# ------------------------------------------------------------
def rank_and_print(journeys, stops, origin, dest, preference, top_k=3):
    if not journeys:
        print("No journey found.")
        return

    # Compute statistics for each journey
    scored = []
    for path in journeys:
        dur = sum(seg[1] for seg in path)
        cost = sum(seg[2] for seg in path)
        non_walk = [seg for seg in path if seg[3] != 'Walk']
        transfers = sum(1 for i in range(1, len(non_walk)) if non_walk[i][3] != non_walk[i-1][3])
        scored.append((dur, cost, transfers, path))

    # Sort according to preference
    if preference == 'cheapest':
        scored.sort(key=lambda x: (x[1], x[0]))
    elif preference == 'fastest':
        scored.sort(key=lambda x: (x[0], x[1]))
    else:  # fewest_segments
        scored.sort(key=lambda x: (len(x[3]), x[0]))

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
            print(f"  -> {stop_name} ({seg[3]}, {seg[1]} min, HKD {seg[2]:.2f})")
    print()

# ------------------------------------------------------------
# 4. Main menu
# ------------------------------------------------------------
def main():
    stops = {}
    graph = defaultdict(list)

    # Attempt to load default files; not required to start
    try:
        stops, graph = load_network("stops.txt", "segments.txt")
        print(f"Loaded default network: {len(stops)} stops, {sum(len(v) for v in graph.values())} directed segments")
    except (FileNotFoundError, ValueError):
        print("No default network found. Use option 4 to load a network file.")

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
            pref = input("Preference (cheapest/fastest/fewest_segments): ").strip().lower()
            if pref not in ('cheapest', 'fastest', 'fewest_segments'):
                pref = 'fastest'
                print("Invalid preference, using 'fastest'")

            paths = find_journeys(graph, origin, dest, max_len=8)
            rank_and_print(paths, stops, origin, dest, pref)
        elif choice == '3':
            print(f"\nNumber of stops: {len(stops)}")
            print(f"Number of directed segments: {sum(len(v) for v in graph.values())}")
            mode_cnt = defaultdict(int)
            for edges in graph.values():
                for _, _, _, mode in edges:
                    mode_cnt[mode] += 1
            for mode, cnt in mode_cnt.items():
                print(f"  {mode}: {cnt} segments")
        elif choice == '4':
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

if __name__ == "__main__":
    main()

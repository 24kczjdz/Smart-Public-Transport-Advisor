#!/usr/bin/env python3
"""graph_demo/app.py – PyQt6 GUI for visualising nv02 transport network routes.

Loads stop and segment data via the nv02 module, lets the user pick an origin
and destination, computes all simple paths, ranks them by a chosen preference,
and draws the network graph with the selected route highlighted.
"""
import pathlib
import sys

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QPixmap
from PyQt6.QtWidgets import (
    QApplication,
    QComboBox,
    QFileDialog,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

import matplotlib
matplotlib.use("QtAgg")
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import networkx as nx
import matplotlib.patches as mpatches
from collections import defaultdict as _defaultdict

# Colour palette for transit lines – used for both route edge highlighting and the legend.
LINE_COLORS = {
    'Island_Line':    '#007DC5',   # blue
    'Tsuen_Wan_Line': '#EE3224',   # red
    'East_Rail_Line': '#53B7E8',   # light blue
    'Tuen_Ma_Line':   '#9B2335',   # maroon
    'Kwun_Tong_Line': '#00A040',   # green
    'Central_Line':   '#9B59B6',   # purple (abstract network)
    'Route_40':       '#FF8C00',   # dark orange
    'Route_4':        '#FFA500',   # orange
    'Route_71':       '#E67E22',   # amber
    'Route_5B':       '#F39C12',   # yellow-orange
    'Route_72':       '#D35400',   # burnt orange
    'Route_74X':      '#E74C3C',   # coral red
    'Route_107':      '#C0392B',   # deep red
    'Route_1':        '#8E44AD',   # violet
    'Route_970':      '#2ECC71',   # emerald
    'Walk':           '#aaaaaa',   # grey
}
_DEFAULT_LINE_COLOR = '#555555'   # fallback for unknown lines


def load_nv02_module():
    """Import and return the nv02 module from the parent directory.

    Inserts the parent of this file's directory into sys.path if needed,
    so that nv02.py is importable regardless of the working directory.

    Returns:
        The imported nv02 module object.
    """
    here = pathlib.Path(__file__).resolve().parent
    nv02_dir = here.parent
    if str(nv02_dir) not in sys.path:
        sys.path.insert(0, str(nv02_dir))
    import nv02
    return nv02


class GraphDemoApp(QMainWindow):
    """Main application window for the nv02 graph demo.

    Provides a GUI with:
    - File selectors for stops and segments CSV/TXT files.
    - Dropdowns for origin, destination, and journey preference.
    - A spinbox to cycle through ranked journey results.
    - A read-only text area showing the top-5 route summaries.
    - A Matplotlib canvas rendering the full network graph with the
      selected route highlighted in red.

    Attributes:
        nv02 (module):          The imported nv02 helper module.
        stops_path (Path):      Default path to the stops file.
        segments_path (Path):   Default path to the segments file.
        stops (dict):           stop_id -> (stop_name, type) mapping.
        graph (dict):           Adjacency list from nv02.load_network.
        journeys (list):        Sorted list of paths from the last query.
    """

    def __init__(self):
        """Initialise the window, load the nv02 module, and build the UI."""
        super().__init__()
        self.setWindowTitle("nv02 Graph Demo")
        self.resize(1200, 800)

        self.nv02 = load_nv02_module()

        self.map_dir = pathlib.Path(__file__).resolve().parent.parent / "map"
        # Default data file locations (relative to this file's grandparent)
        self.stops_path = self.map_dir / "stop01.txt"
        self.segments_path = self.map_dir / "seg01.txt"

        self.stops = {}
        self.graph = {}
        self.journeys = []
        self.current_origin = None
        self.current_dest = None

        self._build_ui()
        self.load_network_data()

    def _build_ui(self):
        """Construct and arrange all Qt widgets inside the main window.

        Layout (top to bottom):
            top_layout    – Map-set quick selector and "Or select map files manually…" button.
            file_layout   – Stops / segments path inputs and Load button.
            select_layout – Origin, destination, preference, path-index controls.
            result_text   – Read-only text area for route summaries.
            canvas        – Matplotlib figure for graph visualisation.
        """
        container = QWidget()
        main_layout = QVBoxLayout(container)

        # --- Map-set quick selector + file-picker button ---
        top_layout = QHBoxLayout()
        top_layout.addWidget(QLabel("Map set:"))
        self.map_set_combo = QComboBox()
        self.map_set_combo.setFixedWidth(70)
        for suffix in self._scan_map_sets():
            self.map_set_combo.addItem(suffix)
        self.map_set_combo.currentTextChanged.connect(self._on_map_set_selected)
        top_layout.addWidget(self.map_set_combo)
        self.preview_button = QPushButton("Or select map files manually…")
        self.preview_button.clicked.connect(self.choose_csv_files)
        top_layout.addWidget(self.preview_button)
        top_layout.addStretch()

        # --- Manual file path inputs + load button ---
        file_layout = QHBoxLayout()
        self.stops_input = QLineEdit(str(self.stops_path))
        self.segs_input = QLineEdit(str(self.segments_path))
        load_button = QPushButton("Load txt files")
        load_button.clicked.connect(self.load_network_data)

        file_layout.addWidget(QLabel("Stops file:"))
        file_layout.addWidget(self.stops_input)
        file_layout.addWidget(QLabel("Segments file:"))
        file_layout.addWidget(self.segs_input)
        file_layout.addWidget(load_button)

        # --- Route query controls ---
        select_layout = QGridLayout()
        self.origin_combo = QComboBox()
        self.dest_combo = QComboBox()
        self.pref_combo = QComboBox()
        self.pref_combo.addItems(["fastest", "cheapest", "fewest_stops", "fewest_transfers"])
        self.path_index_spin = QSpinBox()
        self.path_index_spin.setMinimum(1)
        self.path_index_spin.setMaximum(5)
        self.path_index_spin.setValue(1)
        self.path_index_spin.setEnabled(False)
        self.path_index_spin.valueChanged.connect(self._on_path_index_changed)
        self.route_button = QPushButton("Compute route")
        self.route_button.clicked.connect(self.compute_route)

        select_layout.addWidget(QLabel("Origin:"), 0, 0)
        select_layout.addWidget(self.origin_combo, 0, 1)
        select_layout.addWidget(QLabel("Destination:"), 0, 2)
        select_layout.addWidget(self.dest_combo, 0, 3)
        select_layout.addWidget(QLabel("Preference:"), 1, 0)
        select_layout.addWidget(self.pref_combo, 1, 1)
        select_layout.addWidget(self.route_button, 2, 0, 1, 4)

        # --- Route result text area ---
        self.result_text = QTextEdit()
        self.result_text.setReadOnly(True)
        self.result_text.setMinimumHeight(180)

        # --- Matplotlib canvas for the network graph ---
        self.figure = Figure(figsize=(8, 6))
        self.canvas = FigureCanvas(self.figure)

        main_layout.addLayout(top_layout)
        main_layout.addLayout(file_layout)
        main_layout.addLayout(select_layout)
        main_layout.addWidget(QLabel("Route result:"))
        main_layout.addWidget(self.result_text)
        main_layout.addWidget(QLabel("Graph visualization:"))
        route_index_layout = QHBoxLayout()
        route_index_layout.addWidget(QLabel("Route index:"))
        route_index_layout.addWidget(self.path_index_spin)
        route_index_layout.addStretch()
        main_layout.addLayout(route_index_layout)
        main_layout.addWidget(self.canvas, 1)

        self.setCentralWidget(container)

    def _scan_map_sets(self):
        """Return sorted list of map-set suffixes found in the map directory.

        Looks for files named stop*.txt and extracts the part between 'stop'
        and '.txt' (e.g. 'stop07.txt' → '07'). Only suffixes that also have a
        matching seg*.txt file are included.

        Returns:
            List of suffix strings, e.g. ['01', '02', '07'].
        """
        suffixes = []
        for stop_file in sorted(self.map_dir.glob("stop*.txt")):
            suffix = stop_file.stem[4:]          # strip leading 'stop'
            seg_file = self.map_dir / f"seg{suffix}.txt"
            if seg_file.exists():
                suffixes.append(suffix)
        return suffixes or ['']

    def _on_map_set_selected(self, suffix):
        """Update the file path inputs when the user picks a map-set suffix.

        Constructs stop<suffix>.txt and seg<suffix>.txt paths, updates the
        corresponding QLineEdit fields, and reloads the network.

        Args:
            suffix: The map-set suffix string selected in the combo box.
        """
        if not suffix:
            return
        self.stops_input.setText(str(self.map_dir / f"stop{suffix}.txt"))
        self.segs_input.setText(str(self.map_dir / f"seg{suffix}.txt"))
        self.load_network_data()

    def choose_csv_files(self):
        """Open file-picker dialogs for the stops and segments files.

        Prompts the user to select the stops file first, then the segments
        file. Updates the path input fields and immediately reloads the
        network data if both selections are confirmed.
        """
        stops_file, _ = QFileDialog.getOpenFileName(self, "Select stops*.txt", str(self.stops_path.parent), "CSV Files (*.csv *.txt)")
        if stops_file:
            self.stops_input.setText(stops_file)

        seg_file, _ = QFileDialog.getOpenFileName(self, "Select segments*.txt", str(self.segments_path.parent), "CSV Files (*.csv *.txt)")
        if seg_file:
            self.segs_input.setText(seg_file)

        self.load_network_data()

    def load_network_data(self):
        """Read stops and segments from the paths shown in the input fields.

        On success:
        - Repopulates the origin and destination combo boxes.
        - Clears any previous route result and redraws the blank graph.
        - Updates the status text with stop and segment counts.

        On failure:
        - Shows a critical error dialog; leaves existing network unchanged.
        """
        stops_file = pathlib.Path(self.stops_input.text()).expanduser()
        seg_file = pathlib.Path(self.segs_input.text()).expanduser()
        try:
            self.stops, self.graph = self.nv02.load_network(str(stops_file), str(seg_file))
        except Exception as exc:
            QMessageBox.critical(self, "Load failed", f"Failed to load CSV files:\n{exc}")
            return

        # Rebuild stop dropdowns from freshly loaded data
        self.origin_combo.clear()
        self.dest_combo.clear()
        ids = sorted(self.stops.keys())
        for sid in ids:
            name, _ = self.stops[sid]
            self.origin_combo.addItem(f"{sid} — {name}", sid)
            self.dest_combo.addItem(f"{sid} — {name}", sid)

        if ids:
            self.origin_combo.setCurrentIndex(0)
            # Default destination to the second stop so origin != dest
            self.dest_combo.setCurrentIndex(min(1, len(ids) - 1))

        self.result_text.clear()
        self.draw_graph([], None, None)
        self.journeys = []
        self.current_origin = None
        self.current_dest = None
        self.path_index_spin.setValue(1)
        self.path_index_spin.setMaximum(5)
        self.path_index_spin.setEnabled(False)
        self.result_text.setPlainText(f"Loaded {len(self.stops)} stops and {sum(len(v) for v in self.graph.values())} segments.")

    def compute_route(self):
        """Find, rank, and display journeys between the selected stops.

        Reads the current origin, destination, and preference from the UI,
        calls nv02.find_journeys, sorts via sort_journeys, then:
        - Writes the top-5 route summaries to result_text.
        - Highlights the journey at path_index_spin in the graph canvas.

        Shows a warning dialog for missing/identical endpoints or no result.
        """
        def parse_combo(combo):
            """Extract the stop ID from a combo item formatted as 'ID — Name'."""
            text = combo.currentText().strip()
            if " — " in text:
                return text.split(" — ")[0].strip()
            return text

        origin = parse_combo(self.origin_combo)
        dest = parse_combo(self.dest_combo)
        if not origin or not dest:
            QMessageBox.warning(self, "Input missing", "Please choose both origin and destination.")
            return
        if origin == dest:
            QMessageBox.warning(self, "Invalid route", "Origin and destination must differ.")
            return

        pref = self.pref_combo.currentText()
        self.journeys = self.nv02.find_journeys(self.graph, origin, dest, max_len=8)
        if not self.journeys:
            QMessageBox.information(self, "No route", "No journey was found.")
            self.result_text.setPlainText("No journey found.")
            self.draw_graph([], origin, dest)
            return

        self.journeys = self.sort_journeys(self.journeys, pref)
        self.path_index_spin.setMaximum(min(5, len(self.journeys)))
        self.path_index_spin.setValue(1)
        self.path_index_spin.setEnabled(True)
        self.nv02.rank_and_print(self.journeys, self.stops, origin, dest, pref, top_k=5)

        # Build plain-text summary for the top-5 journeys
        output = []
        for i, path in enumerate(self.journeys[:5], start=1):
            duration = sum(seg[1] for seg in path)
            cost = sum(seg[2] for seg in path)
            non_walk = [s for s in path if s[3] != 'Walk']
            transfers = sum(1 for j in range(1, len(non_walk)) if non_walk[j][4] != non_walk[j-1][4])
            route = " -> ".join([origin] + [seg[0] for seg in path])

            lines = [f"#{i}: {route} Time={duration} min, Cost=HKD {cost:.2f}, Transfers={transfers}, Segments={len(path)}", ""]

            prev_transit_line = None
            for seg in path:
                stop_name, _ = self.stops[seg[0]]
                mode, seg_line = seg[3], seg[4]
                if mode != 'Walk':
                    if prev_transit_line is not None and seg_line != prev_transit_line:
                        lines.append(f"~~ TRANSFER: {prev_transit_line} -> {seg_line}")
                    prev_transit_line = seg_line
                lines.append(f"-> {stop_name} [{seg_line}] {seg[1]} min HKD {seg[2]:.2f}")

            output.append("\n".join(lines))
        self.result_text.setPlainText("\n\n".join(output))

        # Highlight the journey selected by the spinbox (1-based to match #1 #2 in result text)
        self.current_origin = origin
        self.current_dest = dest
        index = min(self.path_index_spin.value(), len(self.journeys)) - 1
        route_edges = self.highlight_route_edges(self.journeys[index], origin)
        self.draw_graph(route_edges, origin, dest)

    def _on_path_index_changed(self, index):
        """Redraw the graph with the route at the new spinbox index (1-based)."""
        if not self.journeys or self.current_origin is None or self.current_dest is None:
            self.result_text.setPlainText("No route computed yet. Please click 'Compute route' first.")
            return
        journey_index = min(index, len(self.journeys)) - 1
        route_edges = self.highlight_route_edges(self.journeys[journey_index], self.current_origin)
        self.draw_graph(route_edges, self.current_origin, self.current_dest)

    def highlight_route_edges(self, journey, origin):
        """Convert a journey path into a list of directed (from, to) edge tuples.

        Args:
            journey: A path as returned by nv02.find_journeys – list of
                     (stop_id, duration, cost, mode) tuples.
            origin:  The starting stop ID (not included in journey segments).

        Returns:
            List of (from_id, to_id) tuples in traversal order.
        """
        edges = []
        current = origin
        for step in journey:
            next_node = step[0]
            edges.append((current, next_node))
            current = next_node
        return edges

    def sort_journeys(self, journeys, preference):
        """Sort journeys by the given preference using a three-key tuple.

        Sorting keys by preference:
            'cheapest':        (cost, duration, segment_count)
            'fastest':         (duration, cost, segment_count)
            'fewest_stops': (segment_count, duration, cost)

        Walk-only segments are excluded when counting transfers, consistent
        with nv02.rank_and_print.

        Args:
            journeys:   List of paths from nv02.find_journeys.
            preference: One of 'cheapest', 'fastest', 'fewest_stops'.

        Returns:
            New sorted list of paths (ascending by the chosen key).
        """
        def score(path):
            """Compute a sortable (primary, secondary, tertiary) score tuple."""
            duration = sum(seg[1] for seg in path)
            cost = sum(seg[2] for seg in path)
            non_walk = [seg for seg in path if seg[3] != 'Walk']
            transfers = sum(1 for i in range(1, len(non_walk)) if non_walk[i][4] != non_walk[i-1][4])
            if preference == 'cheapest':
                return (cost, duration, len(path))
            if preference == 'fastest':
                return (duration, cost, len(path))
            if preference == 'fewest_transfers':
                return (transfers, duration, cost)
            return (len(path), duration, cost)  # fewest_stops

        return sorted(journeys, key=score)

    def draw_graph(self, route_edges, origin=None, dest=None):
        """Render the full network graph on the Matplotlib canvas.

        Node colour scheme:
            Green  (#2ca02c) – origin stop
            Red    (#d62728) – destination stop
            Yellow (#ffcc00) – intermediate stops on the highlighted route
            White  (#ffffff) – all other stops

        Route edges are drawn in the colour defined for their transit line in
        LINE_COLORS (width 3.5); Walk edges on the route are drawn in grey.
        A legend listing each line present on the route is added to the axes.
        All non-route edges are drawn in light grey (width 0.8, 50 % alpha).

        Layout preference: Kamada-Kawai for deterministic placement; falls
        back to spring layout when KK raises an exception (e.g. empty graph).

        Args:
            route_edges: List of (from_id, to_id) tuples to highlight.
                         Pass an empty list to draw the graph without a route.
            origin:      Stop ID to colour green; None for no highlighting.
            dest:        Stop ID to colour red; None for no highlighting.
        """
        G = nx.DiGraph()
        for sid, (name, typ) in self.stops.items():
            G.add_node(sid, label=f"{name}\n{sid}", type=typ)
        for frm, edges in self.graph.items():
            for to, duration, cost, mode, line in edges:
                G.add_edge(frm, to, duration=duration, cost=cost, mode=mode, line=line)

        self.figure.clear()
        ax = self.figure.add_subplot(111)
        ax.clear()
        ax.axis('off')

        if not G.nodes:
            self.canvas.draw()
            return

        n = len(G.nodes())
        try:
            pos = nx.kamada_kawai_layout(G)
        except Exception:
            # Spring layout as fallback; k scales spacing to node count
            k = 2.5 / (n ** 0.5)
            pos = nx.spring_layout(G, seed=42, k=k, iterations=80)

        # Add 12 % padding around the bounding box so labels don't clip
        xs = [p[0] for p in pos.values()]
        ys = [p[1] for p in pos.values()]
        pad_x = (max(xs) - min(xs)) * 0.12 or 0.5
        pad_y = (max(ys) - min(ys)) * 0.12 or 0.5
        ax.set_xlim(min(xs) - pad_x, max(xs) + pad_x)
        ax.set_ylim(min(ys) - pad_y, max(ys) + pad_y)

        # Scale font size down for larger graphs to prevent label overlap
        font_size = max(5, min(8, 120 // n))
        labels = nx.get_node_attributes(G, 'label')

        # Collect all nodes that appear on the highlighted route
        route_nodes = set()
        if route_edges:
            for u, v in route_edges:
                route_nodes.add(u)
                route_nodes.add(v)

        # Assign colour and size per node based on its role in the route
        node_colors = []
        node_sizes = []
        for node in G.nodes():
            if node == origin:
                node_colors.append('#2ca02c')   # green – start
                node_sizes.append(900)
            elif node == dest:
                node_colors.append('#d62728')   # red – end
                node_sizes.append(900)
            elif node in route_nodes:
                node_colors.append('#ffcc00')   # yellow – intermediate
                node_sizes.append(750)
            else:
                node_colors.append('#ffffff')   # white – not on route
                node_sizes.append(450)

        nx.draw_networkx_nodes(G, pos, ax=ax, node_size=node_sizes, node_color=node_colors, edgecolors='#333333')
        nx.draw_networkx_labels(G, pos, labels=labels, font_size=font_size, ax=ax)

        # Draw background edges (not part of highlighted route)
        default_edges = [e for e in G.edges() if e not in route_edges]
        nx.draw_networkx_edges(
            G,
            pos,
            edgelist=default_edges,
            ax=ax,
            edge_color='#bbbbbb',
            arrowsize=10,
            width=0.8,
            alpha=0.5,
            connectionstyle='arc3,rad=0.08',
        )

        # Draw highlighted route edges grouped by line, each in its line colour
        if route_edges:
            edges_by_line = _defaultdict(list)
            for u, v in route_edges:
                edge_line = G[u][v].get('line', 'Unknown') if G.has_edge(u, v) else 'Unknown'
                edges_by_line[edge_line].append((u, v))

            legend_handles = []
            for edge_line, line_edges in edges_by_line.items():
                color = LINE_COLORS.get(edge_line, _DEFAULT_LINE_COLOR)
                nx.draw_networkx_edges(
                    G,
                    pos,
                    edgelist=line_edges,
                    ax=ax,
                    edge_color=color,
                    arrowsize=18,
                    width=3.5,
                    alpha=0.95,
                    connectionstyle='arc3,rad=0.08',
                )
                legend_handles.append(mpatches.Patch(color=color, label=edge_line))

            if legend_handles:
                ax.legend(handles=legend_handles, loc='upper left', fontsize=7, framealpha=0.8)

        self.figure.tight_layout(pad=0.3)
        self.canvas.draw()


def main():
    """Create the QApplication, show the main window, and enter the event loop."""
    app = QApplication(sys.argv)
    window = GraphDemoApp()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

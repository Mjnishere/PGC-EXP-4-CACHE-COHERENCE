#!/usr/bin/env python3
"""Two-core snooping-cache simulator for MSI, MESI and MOESI.

Workload:
  Core 0 reads and writes counter x.
  Core 1 reads and writes counter y.
  'adjacent' places x and y on the same cache line (false sharing).
  'padded' places x and y on separate cache lines.
"""
import sys


def make_cores(n=2):
    return [dict() for _ in range(n)]


def access(proto, caches, core, line, op, stats):
    """Perform one read (R) or write (W) and update protocol statistics."""
    mine = caches[core]
    state = mine.get(line, "I")
    others = [i for i in range(len(caches)) if i != core]
    holders = [i for i in others if caches[i].get(line, "I") != "I"]

    if op == "R":
        if state != "I":
            return
        stats["BusRd"] += 1
        new_state = "S"
        for i in holders:
            other_state = caches[i][line]
            if other_state == "M":
                if proto == "MOESI":
                    caches[i][line] = "O"
                    stats["c2c"] += 1
                else:
                    caches[i][line] = "S"
                    stats["writeback"] += 1
            elif other_state == "O":
                stats["c2c"] += 1
            elif other_state == "E":
                caches[i][line] = "S"
        if not holders and proto in ("MESI", "MOESI"):
            new_state = "E"
        mine[line] = new_state
    else:
        if state == "M":
            return
        if state == "E":
            mine[line] = "M"
            stats["silent E->M"] += 1
            return
        if state in ("S", "O"):
            stats["BusUpgr"] += 1
        else:  # Invalid state: request ownership.
            stats["BusRdX"] += 1
            for i in holders:
                other_state = caches[i][line]
                if other_state in ("M", "O"):
                    if proto == "MOESI":
                        stats["c2c"] += 1
                    else:
                        stats["writeback"] += 1
        for i in holders:
            caches[i][line] = "I"
            stats["invalidations"] += 1
        mine[line] = "M"


def run(proto, layout, iterations):
    keys = ["BusRd", "BusRdX", "BusUpgr", "silent E->M",
            "invalidations", "writeback", "c2c"]
    stats = dict.fromkeys(keys, 0)
    caches = make_cores()
    line_x, line_y = ("A", "A") if layout == "adjacent" else ("A", "B")
    log = []
    for _ in range(iterations):
        for core, line, op in ((0, line_x, "R"), (0, line_x, "W"),
                               (1, line_y, "R"), (1, line_y, "W")):
            access(proto, caches, core, line, op, stats)
            log.append((core, line, op,
                        caches[0].get("A", "I"),
                        caches[1].get("A", "I")))
    stats["bus transactions"] = (
        stats["BusRd"] + stats["BusRdX"] + stats["BusUpgr"]
    )
    return stats, log


def main():
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    if iterations < 1:
        raise SystemExit("Iterations must be a positive integer.")

    print("State trace, first 2 iterations, adjacent layout "
          "(line A: core 0 state / core 1 state)")
    for proto in ("MSI", "MESI", "MOESI"):
        _, log = run(proto, "adjacent", 2)
        print(f"\n{proto}:")
        for core, line, op, s0, s1 in log:
            print(f" core{core} {op} -> C0={s0} C1={s1}")

    keys = ["bus transactions", "BusRd", "BusRdX", "BusUpgr",
            "invalidations", "writeback", "c2c", "silent E->M"]
    print(f"\nTotals for {iterations} iterations")
    print(f"{'layout':9} {'protocol':8} " +
          " ".join(f"{key:>16}" for key in keys))
    for layout in ("adjacent", "padded"):
        for proto in ("MSI", "MESI", "MOESI"):
            stats, _ = run(proto, layout, iterations)
            print(f"{layout:9} {proto:8} " +
                  " ".join(f"{stats[key]:16d}" for key in keys))


if __name__ == "__main__":
    main()

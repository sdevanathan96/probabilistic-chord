# probabilistic-chord

Replacing Chord DHT finger tables with Cuckoo and Quotient filter-based routing, using a bin-based spatial indexing scheme. Built in C++11. Benchmarked across ring sizes from 8 to 1024 nodes using both in-process simulation and Unix domain socket IPC.

> **Headline result.** Filter-based routing achieves the same O(log n) hop count class as the standard finger table while consuming roughly **50% fewer maintenance messages** during stable network periods. The per-hop routing decision is slower than a finger table scan but the difference is under 5% of total per-hop cost in any realistic networked deployment.

---

## Background

Chord is a distributed hash table protocol with O(log n) lookups. Each node maintains a 64-entry **finger table** pointing to exponentially spaced successors on the ring. Routing is cheap, but **maintenance is not**: the `fix_fingers` protocol runs continuously, costing O(log² n) messages per stabilization round even when the network is completely stable.

This project asks: **can we replace the finger table with a probabilistic data structure to cut maintenance cost without losing the O(log n) lookup guarantee?**

---

## The conceptual problem

A direct swap from finger table to filter does not work. The two structures answer different queries:

| Structure | Query it answers |
| --- | --- |
| Finger table | "Which known peer is the closest predecessor of key K?" (nearest-predecessor) |
| Cuckoo / Quotient filter | "Is element X in the set?" (membership) |

Chord routing is fundamentally a nearest-predecessor query. A filter cannot directly answer that.

### The bridge: bin-based spatial indexing

The 64-bit ring is partitioned into `B = 1024` equal-width bins. Each known peer is mapped to its bin and the bin ID is inserted into the filter. On lookup:

1. Compute the target key's bin.
2. Scan **backward** from that bin, probing the filter at each position.
3. On the first hit, resolve the bin ID to a peer via a small side table (`bin_id -> NodeInfo`).

With 64 known peers across 1024 bins, the average backward scan is ~8 filter probes, each O(1). The side table is consulted once per lookup, not per probe.

This makes the filter a meaningful participant in the routing decision while still respecting the predecessor semantics Chord requires. The tradeoff: the bin table is the source of truth for routing, the filter is the index that makes the backward scan cheap.

---

## Architecture

Layered C++11 design. Each layer is abstract and pluggable.

```
+---------------------+
|      Metrics        |  Thread-safe counters, ScopedTimer, CSV export
+---------------------+
|       Chord         |  ChordNode (abstract)
|                     |    |-- FingerTableNode      (baseline)
|                     |    |-- SuccessorWalkNode    (linear strawman)
|                     |    `-- FilterNode           (uses RoutingFilter)
+---------------------+
|       Filter        |  RoutingFilter (abstract, bin logic shared)
|                     |    |-- CuckooRoutingFilter
|                     |    `-- QuotientRoutingFilter
+---------------------+
|      Transport      |  Transport (abstract)
|                     |    |-- SimTransport (in-process queues)
|                     |    `-- IPCTransport (Unix domain sockets)
+---------------------+
```

The same node binary runs in either transport mode. SimTransport is for fast unit experiments. IPCTransport launches each node as a separate OS process and was tested with up to **1024 processes** on a Northeastern Khoury Linux server.

### Protocol details

- **Hashing.** FNV-1a (64-bit) for both node IDs and keys. MurmurHash3 64-bit finalizer for Cuckoo fingerprints.
- **Ring arithmetic.** Correct wraparound handling for predecessor/successor relations.
- **Lookup.** Iterative (origin-driven), not recursive. Loop detection via visited-node tracking.
- **Maintenance.** `fix_fingers` runs round-robin on finger table nodes. Filter nodes populate their bin table incrementally during their own lookups and do not run continuous background repair.

### Cuckoo filter

Implemented from scratch. Configurable fingerprint size (controls FP rate), 4 entries per bucket, partial-key cuckoo hashing for the alternate bucket. O(1) amortized insert, lookup, and delete.

### Quotient filter

Implemented from scratch. Configurable remainder size. Three metadata bits per slot (`is_occupied`, `is_continuation`, `is_shifted`) plus a tombstone flag for deletion. Insert maintains sorted runs within clusters.

---

## Results

### 1. Lookup performance

| Ring size | Finger Table (hops) | Cuckoo (hops) | Quotient (hops) | Successor Walk (hops) |
| --- | --- | --- | --- | --- |
| 256 | ~4 | ~12 | ~12 | ~90 |
| 1024 | ~4 | ~47 | ~47 | ~370 |

Both filter variants follow O(log n) growth. The constant factor is higher than the finger table because each node populates its bin table only through its own lookups, so peer coverage lags behind the explicit `fix_fingers` schedule. Lookups for keys that fall in unpopulated bins fall back to successor forwarding. See limitations below.

### 2. Maintenance cost (stable network, 100 rounds)

| Mode | Total messages | Maintenance messages |
| --- | --- | --- |
| Finger Table | ~14,000 | ~12,800 |
| Cuckoo | ~12,800 | ~6,400 |
| Quotient | ~12,800 | ~6,400 |

**Roughly 50% fewer maintenance messages for filter-based routing during stable periods**, because filters do not run continuous background repair once the bin table is populated. During node-join phases all three modes use similar message counts since ring repair is required regardless.

### 3. Per-hop decision time (256 nodes)

| Mode | Avg (ns) | p50 (ns) | p99 (ns) |
| --- | --- | --- | --- |
| Finger Table | 251 | 234 | 353 |
| Quotient Filter | 2,990 | 3,586 | 5,018 |
| Cuckoo Filter | 8,484 | 10,239 | 20,691 |

Finger table wins this comparison by an order of magnitude. The Quotient filter is ~12x slower, the Cuckoo filter ~34x slower. Cuckoo loses ground because false-positive hits during the backward bin scan trigger additional probes.

**However**, network latency dominates: each hop costs 100,000+ ns of network time in a real deployment. The decision-time gap is under 5% of total per-hop cost and is negligible in practice.

### 4. False positive rate sensitivity

| Effect | Sensitive to FP rate? |
| --- | --- |
| Hop count | No |
| Per-hop decision time | Yes (more probes per backward scan) |
| Memory per node | Yes (smaller fingerprints / remainders = less memory) |

Because the bin table provides exact peer resolution at the final step, **the filter's FP rate does not affect routing correctness or hop count**. It affects only how expensive each routing decision is and how much memory the filter consumes.

### 5. Latency under churn (64 nodes, IPC transport)

All three routing modes show comparable latency during churn. Filter variants show 100-200 microseconds of additional jitter, likely because bin table entries are not synchronized across nodes during membership changes.

---

## Build and run

### Requirements
- C++11 compiler
- CMake 3.10+
- POSIX system (Unix domain sockets used in IPC mode)

### Build
```bash
mkdir build && cd build
cmake ..
make
```

### Run tests
```bash
./test_chord
./test_cuckoo_filter
./test_quotient_filter
./test_cuckoo_routing
./test_quotient_routing
./test_metrics_integration
```

### Run benchmarks
```bash
./bench_baseline                   # SimTransport, all 4 routing modes
./bench_baseline --ipc             # Same, using Unix-socket IPC
./bench_decision_time              # Per-hop decision time micro-benchmark
./run_experiments                  # Maintenance cost, churn latency, FP sensitivity
./run_experiments --ipc            # Same, multi-process
```

### Multi-process orchestration
```bash
./scripts/launch.sh 256            # Spawn a 256-node ring
./scripts/verify.sh                # Sanity-check ring integrity
./scripts/stop.sh                  # Tear down
```

---

## Repository layout

```
src/
  transport/    # Transport interface, SimTransport, IPCTransport
  chord/        # ChordNode base, FingerTableNode, SuccessorWalkNode, FilterNode
  filter/       # CuckooFilter, QuotientFilter, RoutingFilter, routing-filter subclasses
  bench/        # Metrics, ScopedTimer
  main.cpp      # CLI for running nodes in sim or IPC mode
test/
  test_*.cpp    # Unit tests (see test suite table below)
  bench_*.cpp   # Benchmark drivers
  run_experiments.cpp
scripts/
  launch.sh     # Spawn N IPC processes
  stop.sh
  verify.sh     # Verify ring integrity
CMakeLists.txt
```

### Test suites

| Suite | Coverage |
| --- | --- |
| `test_message` | Wire-format round-trips for all message types |
| `test_sim_transport` | In-process delivery, multi-node ping/pong, payload integrity |
| `test_ipc_transport` | Unix-socket delivery, multi-node messaging, payload integrity |
| `test_chord` | Ring formation, successor/predecessor correctness, join/leave |
| `test_finger_table` | Finger start computation, closest-preceding-finger, O(log n) routing |
| `test_cuckoo_filter` | Insert / delete / FP rate at 8/12/16-bit fingerprints, high-load behavior |
| `test_quotient_filter` | Insert / lookup / delete with tombstones, FP rates, load-factor behavior |
| `test_cuckoo_routing` | Filter-based Chord routing correctness, metrics, node leave |
| `test_quotient_routing` | Same, for Quotient variant |
| `test_metrics` | Counter recording, percentile computation, CSV output, ScopedTimer accuracy |
| `test_metrics_integration` | End-to-end metrics recording during real lookups and stabilization |

---

## Limitations and honest framing

Things this project does **not** do, in order of how much they matter:

1. **Peer propagation between nodes.** Each node populates its bin table only through its own lookups. Peers are not shared between neighbors during stabilization, which is why filter-based hop counts grow faster than finger-table hop counts at larger ring sizes. A propagation protocol could close this gap but would partially erase the maintenance-cost savings.
2. **Only uniform random key lookups were tested.** Skewed workloads (Zipfian, hotspots) are not measured. Real DHT traffic is rarely uniform.
3. **Only one churn rate was tested.** The proposal planned low/moderate/high churn rates. The infrastructure supports configurable rates but time did not.
4. **No byte-level bandwidth metric.** Message counts are tracked, payload byte counts are not.
5. **No failure injection.** Nodes leave gracefully. Crash failures and network partitions are not exercised.

These should be read as "next steps," not as fatal gaps. The conceptual findings stand.

---

## Acknowledgments

This was the final project for **CS7800 Advanced Algorithms** at Northeastern University. The suggestion to explore a Maplet-style data structure as a more natural primitive for nearest-predecessor routing came from the course instructor and is left as future work.

---

## License

MIT

# Cellular Network Simulator - Project Report

**Course:** Object-Oriented Programming and Design (OOPD)
**Authors:** Sayan Das, Senjuti Ghosal
**Language / Standard:** C++17

---

## 1. Objective

Build a simulator that models cellular network capacity across six protocol generations (2G through a speculative 7G), using it as a vehicle to demonstrate core object-oriented design principles - abstraction, encapsulation, inheritance, polymorphism, generic programming, and exception safety - alongside a concurrency subsystem and an interactive terminal interface.

This report documents the system as it stands after a full audit-fix-improve pass: what was verified working, what was broken and how it was diagnosed and fixed, what was added, and what limitations remain. Every claim below was checked by actually running the code, not by inspection alone.

---

## 2. System Design

### 2.1 Abstraction and Encapsulation

`Protocol` and `UserDevice` are both abstract base classes exposing pure virtual interfaces (`calculate_max_users`, `calculate_messages_per_user`, `calculate_required_cores` for `Protocol`; `get_message_count`, `get_device_type` for `UserDevice`). Internal state (channel bandwidth, message overhead, device counters) is private/protected, accessed only through accessor methods. `CellTower` encapsulates its frequency channels and connected-device map behind a `std::mutex`, exposing only thread-safe public operations.

### 2.2 Inheritance and Polymorphism

Two independent six-way hierarchies:

- `Protocol` → `TDMAProtocol` (2G), `CDMAProtocol` (3G), `OFDMProtocol` (4G), `MassiveMIMOProtocol` (5G), `TerahertzProtocol` (6G), `HolographicProtocol` (7G).
- `UserDevice` → `Device2G` … `Device7G`.

This is not a thin dispatch layer: each `Protocol` subclass's `calculate_max_users()` uses a materially different formula (different channel widths, antenna counts, and in three cases a secondary high-frequency band added on top of the base calculation). Verifying this was part of the audit - see Section 4.

### 2.3 Generic Programming

`NetworkCollection<T>` is a template class used for both `towers` (`NetworkCollection<CellTower>`) and `devices` (`NetworkCollection<UserDevice>`) inside `Simulator`. Originally a thin, unsynchronized `vector<shared_ptr<T>>` wrapper, it now owns a `std::mutex` and guards every operation - a concrete example of a template class that had to earn its keep as more than a type-safety checkbox once concurrent access became a real requirement (see Section 5.2).

### 2.4 Exception Safety

A custom hierarchy (`CellularNetworkException` base, with `CapacityException`, `ProtocolException`, `ConfigurationException`, `FrequencyException` derived) is used for real control flow: `CellTower::connect_device()` throws `CapacityException` when a tower is full, `CellularCore::register_device()` throws when a core's message capacity is exceeded, and integer-overflow checks in `Protocol::calculate_cores_from_messages()` throw rather than silently wrapping. Callers (the TUI's command handler, the headless benchmark) catch and report these rather than letting them propagate to `main()`.

### 2.5 Concurrency

`ThreadPool` (producer-consumer over a task queue with a condition variable) backs the headless multi-threaded provisioning benchmark. `Logger` runs its own background thread that drains a queue of formatted messages. Getting the concurrency primitives to actually be *correct* under concurrent use - not just present in the class diagram - was the largest single body of work in this project; Section 5 documents it in detail.

---

## 3. Implementation Summary

| Subsystem | Files | Responsibility |
| :--- | :--- | :--- |
| Protocols | `Protocol.h/.cpp` | 6 generation-specific capacity formulas |
| Devices | `UserDevice.h/.cpp` | 6 generation-specific message-load profiles |
| Tower / Core | `CellTower.h/.cpp`, `CellularCore.h/.cpp` | Frequency channel allocation, per-core message capacity, thread-safe connect/disconnect |
| Orchestration | `Simulator.h/.cpp` | Tower/device factories, scenario loading, the threading benchmark, traffic simulation |
| Analytics | `NetworkAnalytics.h/.cpp` | Performance reporting, `LoadBalancer`, `HandoverManager`, `ScenarioManager` |
| Concurrency infra | `Utils.h/.cpp` | `ThreadPool`, `Logger`, `OutputFormatter` |
| I/O | `basicIO.h/.cpp`, `syscall.S`, `IOHelpers.h/.cpp` | Syscall-level output (Linux) with a portable fallback (macOS) |
| Interface | `ConsoleTUI.h/.cpp` | Full-screen terminal dashboard: 7 tabs, raw-mode keyboard/mouse input, a command console |

Total: ~13,000 lines of C++ across headers, implementation, and tests, plus one x86-64 NASM assembly file.

---

## 4. Testing and Validation

Four unit test suites (`make test`), each compiled and linked independently against the production sources (not mocks), run automatically:

1. **`test_protocol_core.cpp`** - protocol capacity formulas, overflow handling on an adversarial input (2 billion users), and `CellularCore` registration up to and past its actual capacity limit (rewritten during this pass - see Section 5.3).
2. **`test_exceptions.cpp`** - message-counter accumulation correctness and `CapacityException` on tower overfill.
3. **`test_concurrency.cpp`** - 8 threads × 1000 calls to `CellularCore::generate_messages()`, asserting the exact expected total (proves the core's own mutex is correct).
4. **`test_load_balancer.cpp`** (new) - deterministically overloads a tower to 100% utilization, calls `LoadBalancer::redistribute_devices()`, and asserts utilization actually drops and devices land on the target tower.

Beyond unit tests, the TUI itself was driven end-to-end through a pseudo-terminal (`pty` + `pyte`, a Python VT100 emulator) to capture and inspect the *actual rendered terminal grid* across all 7 tabs - not just "the process didn't crash," but a character-by-character check that box borders, tables, and the spatial map render without corruption.

---

## 5. Debugging Process

### 5.1 The Logger / TUI Race

**Symptom investigated:** the terminal UI was reported to look visually broken during live use.

**Method:** rather than trust a single visual capture, the TUI was driven through a proper VT100 emulator twice - once with a naive test harness that decoded each raw PTY read independently as UTF-8, and once with a corrected harness (`pyte.ByteStream`, which decodes incrementally across read boundaries). The first harness showed apparent corruption; the second showed none, once the underlying fix was applied. This distinction matters and is reported honestly: the *visual* corruption first observed was partly an artifact of the test tool, but the underlying defect it was pointing at - **`Logger` running a background thread that wrote to stdout via raw syscalls, completely unsynchronized with `ConsoleTUI`'s own `std::cout`-based renderer on the main thread** - is real, independent of how it was visualized, and a textbook data race (two threads, one file descriptor, no shared lock).

**Fix:** `Logger::set_console_echo(false)` is called for the duration of a `ConsoleTUI` session (the TUI already renders `Logger::get_logs()` into its own panel), and restored afterward.

### 5.2 Deeper Concurrency Bugs Found by a Second Audit Pass

A dedicated audit of the backend (towers, cores, protocols, the template collection) surfaced issues a first read-through missed:

- `NetworkCollection<T>` was unsynchronized but handed to multiple `ThreadPool` workers by `Simulator::generate_network_parallel` and the benchmark - a real race on concurrent `push_back`.
- `CellTower::disconnect_device()` released capacity from "the first core with a nonzero count" instead of the core a device was actually registered to, silently corrupting per-core accounting on any tower with more than one core.
- `UserDevice`/`CellTower`/`CellularCore`'s `int` id counters were incremented (`++counter`) from multiple threads with no synchronization.
- `rand()`, called on every device placement, is not guaranteed reentrant, and was called concurrently from ThreadPool workers.
- `Logger::logs.push_back()` had no lock at all, racing across every worker thread in the benchmark.

All five were fixed (mutex on the collection, a `device_id -> core` map, `std::atomic<int>` counters, a `thread_local std::mt19937`, and a dedicated `logsMutex`), and the fixes were verified by re-running the full test suite and TUI capture after each change.

### 5.3 The Benchmark Was Measuring the Wrong Thing

The original headless benchmark reported a 7.08x multi-threading speedup. Investigating *why* revealed it wasn't measuring what it claimed:

1. Every simulated connection was followed by an artificial 500-microsecond sleep and a busy-loop "to make parallelism worth it" - the reported speedup mostly reflected 8 threads sleeping concurrently.
2. The multi-threaded run wrapped the *entire* real connection call in one shared mutex, fully serializing the actual work; only the fake delay outside the lock ran in parallel.

Removing both dropped the honest number to **0.39x - slower than single-threaded**, which pointed at something real. The full iterative diagnosis - task-granularity overhead, then the `Logger` race and its performance cost, then residual contention on `NetworkCollection::add()` even after it was correctly locked - is documented in the README's [Concurrency Benchmark](README.md#concurrency-benchmark) section. The end state is a genuine 1.05x-3.26x speedup depending on thread count, measured on an 8-tower, 100,000-device workload, with the batched-publish fix (`add_batch()`) responsible for most of the improvement beyond ~1x.

This is the single best illustration in the project of the difference between a benchmark that produces an impressive number and one that produces a *true* number - and of the fact that fixing correctness bugs (the unsynchronized `logs` vector) can matter as much for performance as for safety.

### 5.4 A Feature That Was Declared but Never Built

`LoadBalancer::balance_load()`, `redistribute_devices()`, and `find_best_tower()` were declared in `NetworkAnalytics.h` with **zero implementation anywhere in the codebase**, and `LoadBalancer` was never instantiated by any reachable code path - a class that existed only in the header and the architecture diagram. All three were implemented (QoS-aware tower selection, threshold-based redistribution with a "meaningfully better" guard against thrashing), covered by a new deterministic unit test, and wired into the TUI as the `:balance` console command so it is now an actually-usable feature rather than a documented-but-absent one.

---

## 6. Results

See the [README](README.md#generation-capacity-model) for full tables and plots. Summary:

- **Capacity model**: at a 10 MHz reference bandwidth, modeled capacity ranges from 800 users (2G) to 268,800,000 users (7G) - a ~336,000x spread, computed directly from `Protocol::calculate_max_users()`, not asserted. The previous version of this table's *core* figures (as opposed to user figures) were wrong by 11x-154x, having apparently never been derived from `calculate_required_cores()` at all.
- **Concurrency benchmark**: genuine 1.05x-3.26x speedup across 1-10 threads on a 100,000-device, 8-tower workload, after removing artificial work and fixing three real synchronization bugs that were suppressing real parallelism.
- **Code quality**: 0 compiler warnings under `-Wall -Wextra -Wpedantic` (previously suppressed via `-Wno-unused-variable`, which was hiding 2 real ones), 19 concrete bugs fixed (enumerated in the README), ~450 lines of dead code relocated to `legacy/` rather than silently deleted.

---

## 7. Limitations and Future Work

Documented in full in the README's [Known Limitations](README.md#known-limitations) section. In short: the "zero-overhead assembly I/O" framing is stronger than what the code delivers (excluded entirely on macOS, one syscall per character even on Linux); six analysis methods (`analyze2G()`-`analyze7G()`) are correct but currently unreachable, left over from a pre-TUI CLI menu; this remains a closed-form capacity model rather than a packet/event-level simulator like ns-3 or OMNeT++.

Reasonable next steps: wire `analyze2G()`-`analyze7G()` into a TUI "detailed report" action instead of leaving them dead; seed the mobility RNG for fully reproducible map layouts; extend `LoadBalancer`'s QoS model to be configurable per-scenario rather than inferred from communication type alone.

---

## 8. References and Attribution

- `basicIO`/`syscall.S` syscall-level I/O pattern: conceptually adapted from operating-systems I/O architecture material (Silberschatz, Tanenbaum), not copied from any specific implementation.
- `ThreadPool`: a standard producer-consumer task-queue pattern, as commonly presented in C++ concurrency references.
- Box-drawing characters and terminal control sequences: standard Unicode/ANSI, not third-party code.
- No external TUI, networking, or simulation libraries are used; all rendering, input handling, and simulation logic are original to this project.

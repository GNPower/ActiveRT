# Using the CLI to Print ActiveRT Stats

Once the CLI is wired up (see [Setting Up the CLI](setup)), connect a
terminal to your device's UART and use the following commands.

---

## `activert summary`

Prints a one-line status for every registered Active Object and event pool.

```text
> activert summary

Active Objects (2):
  cmd_ao   [prio=5] events=1024 dropped=0  max_us=42  health=OK
  data_ao  [prio=3] events=8192 dropped=12 max_us=18  health=WARN

Event Pools (2):
  cmd_pool   8/ 8 free  peak= 6  failures=0
  data_pool 14/32 free  peak=29  failures=3
```

---

## `activert list`

Lists the names of all registered Active Objects.

```text
> activert list
[0] cmd_ao
[1] data_ao
```

---

## `activert show <name>`

Detailed statistics for one Active Object, including per-queue breakdown.

```text
> activert show cmd_ao

Active Object: cmd_ao
  Priority:         5
  Events processed: 1024
  Events dropped:   0
  Notifications:    64
  Dispatch time:    min=2us  avg=18us  max=42us

  Queue 0 (cmd):
    Depth:        0 / 8 (current / max)
    Peak:         6 / 8 (75%)
    Posts:        1024 ok  0 failed
```

---

## `activert pool <name>`

Detailed statistics for one event pool.

```text
> activert pool data_pool

Event Pool: data_pool
  Capacity:     32 events  (event size = 16 bytes)
  Current:      18 / 32 (56%)
  Peak:         29 / 32 (90%)
  Free:         14
  Allocs:       8192 attempted  8189 ok  3 failed
  Failure rate: 0.04%
```

---

## `activert health`

Evaluates health thresholds for every registered component.

```text
> activert health

Health check:
  cmd_ao    - OK
  data_ao   - WARNING: queue 0 utilisation 82% (>80%)
  cmd_pool  - OK
  data_pool - WARNING: failure rate 0.04% (pool near exhaustion)
```

---

## `activert reset [name]`

Resets statistics counters. Without an argument, resets all components.

```text
> activert reset            # reset all
> activert reset cmd_ao     # reset one AO
> activert reset data_pool  # reset one pool
```

---

## `activert perf`

Shows which AO has the highest event throughput and which has the longest
maximum dispatch time.

```text
> activert perf

Performance summary:
  Busiest AO:  data_ao   (8192 events/period)
  Slowest AO:  cmd_ao    (max dispatch = 42us)
```

---

## `activert report`

Prints a full diagnostic report — equivalent to running `summary`,
`health`, and `perf` together, followed by per-queue detail for every AO.
Useful for capturing a complete snapshot to a log file.

```text
> activert report
=== ActiveRT Full Report ===
... (full output) ...
```

---

## Tips

- **Capture to a file**: most terminal emulators support logging. Use
  `activert report` and save the output to a `.txt` file for offline analysis.
- **Periodic snapshots**: schedule an AO or timer to call
  `activert_stats_print_summary()` from code every N seconds for
  continuous monitoring.
- **After a fault**: call `activert report` immediately after detecting a
  health warning to get a full snapshot before counters are reset.

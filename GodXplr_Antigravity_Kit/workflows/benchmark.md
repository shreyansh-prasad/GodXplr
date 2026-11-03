# Workflow — GodXplr Benchmark Protocol

## 1. Before measurement

- Identify the exact metric and user-visible latency it represents.
- Freeze workload, dataset, seed, file count, directory-depth distribution and data size.
- Record commit, build type, compiler, Qt version, OS build, CPU, RAM, storage and power mode.
- Specify cold/warm cache semantics. Do not mix them.
- Specify warm-up count, run count, background load and whether process/service restarts occur.
- Run competitor tests on the same machine, same dataset and comparable settings wherever licensing/technical constraints allow.

## 2. Required metrics

Report p50/p95/p99 for latency; FPS alone is insufficient. For UI use frame-time histogram, missed-frame count, input-to-present latency, CPU and peak/resident memory. For search use query length/type, result count, time-to-first-page, time-to-final-page and cancellation latency. For indexing use files/s, bytes/s, CPU, disk I/O, memory, disk growth and catch-up lag.

## 3. Data integrity

Save raw measurements (CSV/JSON), dataset manifest and exact command line under `docs/bench/<subsystem>/<date>-<commit>/`. Keep failed runs; label them invalid with a reason rather than deleting them.

## 4. Reporting

Every report distinguishes target from observed result. State whether the data is synthetic, VHDX, local real-world or a competitor comparison. Never claim a result generalizes to all hardware based on one PC.

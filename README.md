## Project Goal

**Push a hobby RISC VM into the *single-digit-nanosecond* arena using the
same latency-engineering tricks found in market-data feed handlers and
matching engines.**

---

## Performance Road-Map

| Phase | Deliverable (tag) | Target Metric |
|-------|------------------|---------------|
| **v0.1 ✅** | Baseline interpreter | **≈ 105 ns p99**, 9.5 M instr/s |
| **v0.2 ✅** | Lock-free **ring-buffer bus** (multi-VM) | 160 spins/msg|
| **v0.3** | **LLVM ORC JIT** | **< 25 ns p99**, 40 M instr/s |
| **v0.4** | Simulated **NIC / pcap feed** | Replay 10 Gbps in real-time |
| **v0.5** | **Prometheus exporter + Grafana** | Live histograms, < 1% overhead |

**End-state KPI:** *< 10 ns instruction latency & > 100 M instr/s on a single core,
with deterministic tail (< 3 × mean).*

---

## V0.1 (5/17/2025) — LC-3 Low-Latency VM (baseline interpreter)

| Metric (i7-12700K) | Value                  |
|--------------------|------------------------|
| Instructions run   | **100,000,000**        |
| p99 latency        | **≈ 105 ns / instr**   |
| Throughput         | **≈ 9.5 M instr/s**    |

*Demo*
![baseline1](bandicam 2025-06-28 17-55-52-344)

*Result*
![baseline](Screenshot%202025-05-17%20194628.png)

---

## V0.2 (6/28/2025) — Lock-Free Ring Buffer Bus (Multi-VM)

✅ **Inter-VM lock-free message passing working.**

Measured metrics from `consumer.obj`:

| Metric                 | Value                 |
|------------------------|-----------------------|
| Messages Received      | **50,000**            |
| Avg Spins per Msg      | **160.09**            |
| Avg µs per Msg         | **1466.35 µs**        |
| Msgs/sec               | **681.96**            |
| Elapsed Time           | **14.66 s**           |
| Instruction Rate       | **3409.82 MIPS**      |

*Demo*
![ring-buffer1](bandicam 2025-06-28 17-55-52-344)

*Result*
![ring-buffer](bandicam%202025-06-28%2022-44-59-487.jpg)

This establishes a minimal multi-core style message bus in software, allowing parallel VMs to communicate through high-performance, wait-free infrastructure. Future improvements will focus on:
- Reducing inter-VM message latency below 10 µs.
- Increasing throughput with cache-line optimization and zero-copy signaling.
- Introducing realistic workload simulation (via NIC/pktgen input).

---
### Build & Run Instructions

```bash
# build
g++ -std=c++17 -O2 lc3-alt-win-v2.cpp -o dual-vm -luser32

# run benchmark
./dual-vm






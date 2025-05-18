## Project Goal

**Push a hobby RISC VM into the *single-digit-nanosecond* arena using the
same latency-engineering tricks found in market-data feed handlers and
matching engines.**

---

## Performance Road-Map

| Phase | Deliverable (tag) | Target Metric |
|-------|------------------|---------------|
| **v0.1 ✅** | Baseline interpreter | **≈ 105 ns p99**, 9.5 M instr/s |
| **v0.2** | **LLVM ORC JIT** | **< 25 ns p99**, 40 M instr/s |
| **v0.3** | Lock-free **ring-buffer bus** (multi-VM) | **< 10 µs p99** message latency |
| **v0.4** | Simulated **NIC / pcap feed** | Replay 10 Gbps in real-time |
| **v0.5** | **Prometheus exporter + Grafana** | Live histograms, < 1 % overhead |

**End-state KPI:** *< 10 ns instruction latency & > 100 M instr/s on a single core,
with deterministic tail (< 3 × mean).*


# V0.1 (5/17/2025)

# LC-3 Low-Latency VM (baseline interpreter)

| Metric (i7-12700K) | Value              |
|--------------------|--------------------|
| Instructions run   | **100 000 000**    |
| p99 latency        | **≈ 105 ns / instr** |
| Throughput         | **≈ 9.5 M instr/s** |

![baseline](Screenshot%202025-05-17%20194628.png)

```bash
# build
g++ -std=c++17 -O2 src/lc3_vm.cpp -o lc3-vm

# run benchmark
./lc3-vm bench/sum_loop.obj --quiet





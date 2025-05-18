# LC-3 Low-Latency VM (baseline interpreter)

| Metric (i7-12700K) | Value              |
|--------------------|--------------------|
| Instructions run   | **100 000 000**    |
| p99 latency        | **≈ 105 ns / instr** |
| Throughput         | **≈ 9.5 M instr/s** |

![baseline](Screenshot%202025-05-17%20194628.png)
# BASELINE (5/17/2025)

```bash
# build
g++ -std=c++17 -O2 src/lc3_vm.cpp -o lc3-vm

# run benchmark
./lc3-vm bench/sum_loop.obj --quiet





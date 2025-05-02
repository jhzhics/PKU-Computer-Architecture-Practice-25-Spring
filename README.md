# 使用说明

# 依赖
参考devcontainer配置
```json
{
  "image": "harsonlau/riscv-tools:latest",
}
```

# Sim
## Build
```bash
cd sim
make
```

## Executeables

- sim/build/CacheSimulator

A Cache simulator to simulate single cache

- sim/build/CacheSimulator3-2

A Cache simulator to simulate 2-level cache for lab 3.2
The cache config is set in `sim/src/executable/cache_sim_lab3-2.cpp`

- sim/build/Simulator

A simulator to simulate the whole system with mem/arch config
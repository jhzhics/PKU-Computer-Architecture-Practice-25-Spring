# Usage
## cache_sim.py
1. 构建cache模拟器
```bash
cd sim && make && cd ..
```

2. 生成图像
将两个trace文件放置于工作目录下
```bash
python3 scripts/lab3/cache_sim.py -f trace1.txt
python3 scripts/lab3/cache_sim.py -f trace2.txt
```
相关依赖在`requirements.txt`
产生trace1.csv和trace2.csv(已经预先生成)
然后在part1.ipynb分析出图像
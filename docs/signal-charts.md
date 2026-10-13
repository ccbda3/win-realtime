四个信号图的参数都来自同一来源—— IQ 数据 （I/Q 采样值）。

## 数据来源
```
gps-core.exe → TCP (port 1999) → 
rf_core.c (设备线程) → onUsrpIq() → 
ChartWidget::setData()
```
IQ 数据是一维 float 数组，格式为： [I0, Q0, I1, Q1, I2, Q2, ...] ，每个采样点由 I（同相）和 Q（正交）两个 float 值组成。

## 每个图的绘制参数
### 1. SPECTRUM（频谱图）
- 输入数据 ：IQ 采样 → FFT 变换后的频域幅值（dB）
- 核心参数 ：
  - data[] — 频域各频点的 dB 值
  - displayMin / displayMax — 当前帧的动态范围（自动缩放）
  - maxHoldData[] — 历史最大值轨迹（每帧衰减 0.3%， *= 0.997f ）
  - zoom / pan — 鼠标滚轮缩放、拖拽平移
- 绘制方式 ：折线图 + 渐变填充 + maxHold 轨迹
### 2. WATERFALL（瀑布图）
- 输入数据 ：同频谱图的频域幅值
- 核心参数 ：
  - data[] — 当前帧各频点值
  - noiseFloor — 底噪基准线（ displayMin + range * 0.6 ）
  - waterfallImg — 256 色索引图，每帧向上滚动一行
  - zoom / pan — 频率范围选择
- 色板 ：蓝→青→黄→白的渐变色映射（0~255）
### 3. TIME（时域图）
- 输入数据 ：原始 IQ 采样值（时域）
- 核心参数 ：
  - data[] — 时域各采样点的幅值（归一化到 ±1.0）
  - zoom / pan — 时间窗口缩放
- 绘制方式 ：折线图，中心线为零点，显示波形
### 4. CONST（星座图）
- 输入数据 ：原始 IQ 值对
- 核心参数 ：
  - data[i] = I 值， data[i+1] = Q 值（归一化 ±1.0）
  - maxR — 绘制半径（ min(w,h)/2 - 16 ）
  - zoom — 散点缩放
- 绘制方式 ：每个 (I, Q) 对画一个像素点，同心圆网格辅助线
## 统一参数
参数 含义 connected false = 显示 WAITING，true = 开始绘图 data 上游传来的 IQ / 频谱数据 zoom 缩放系数（滚轮控制） pan 平移偏移量（拖拽控制）

所有图的更新频率由设备线程的 IQ 数据推送频率决定（每秒约 1000 帧，每帧 2560 采样点）。
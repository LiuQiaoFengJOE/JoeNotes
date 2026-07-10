# Challenge: Moonblue Night

这次题目的目标，不是还原 `landscape_challenge_reference.png`，
而是把同一份 RAW 调成一种新的风格：

- 冷蓝
- 夜感
- 低饱和
- 山体更像剪影
- 水面保留一点月光感

目标图我已经先调好了，你要向它靠：

- 目标图：
  `challenge_target_moonblue/04_display_preview.png`

## 固定输入

必须使用同一份 RAW：

- `landscape_challenge_sensor_rggb8_640x480.raw`

调参时要固定 RAW，不要重生输入：

- 必须带 `--skip-generate`

## 你可以动的参数

- `--black-level`
- `--wb-r`
- `--wb-g`
- `--wb-b`
- `--lsc-strength`
- `--gamma`
- `--ccm-strength`
- `--saturation`

## 你不该做的事

1. 不要改输入 RAW。
2. 不要只看最终图。
3. 不要一口气把所有参数乱改一遍。
4. 不要只追“像 reference”，这题的目标不是原 reference，而是 `Moonblue Night` 风格。

## 建议起点

你可以直接从坏起点开始：

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\moonblue_try_00 --black-level 16 --wb-r 0.55 --wb-g 1.85 --wb-b 2.10 --lsc-strength 0.12 --gamma 3.05
```

之后每次新建一个目录，例如：

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\moonblue_try_01 ...
```

## 重点观察

每次至少看这三张：

- `01_black_level_preview.png`
- `02_lsc_wb_preview.png`
- `04_display_preview.png`

## 你要追的观感

不是“普通蓝色”，而是下面这几个同时成立：

1. 天空偏冷蓝，但不是脏青。
2. 山体明显压暗，有夜景剪影感。
3. 水面不能死黑，要留一点反射层次。
4. 整体颜色别太冲，饱和度应偏克制。
5. 光源附近要有一点冷月晕感。

## 推荐调参顺序

1. 先把 `black level` 调到夜景地基成立。
2. 再把 `wb-r / wb-g / wb-b` 往冷蓝夜景方向推。
3. 再用 `lsc-strength` 调边角和中心关系。
4. 再用 `gamma` 调整体夜感和层次。
5. 最后再用 `ccm-strength / saturation` 做风格微调。

## 交作业方式

你调完后，把你认为最接近目标的一组目录告诉我，例如：

- `moonblue_try_05`

我会按下面 3 件事给你复盘：

1. 你这一步最接近目标的是哪里
2. 还差在哪个模块
3. 下一步最该先动哪个参数

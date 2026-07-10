# Challenge: Child Flying A Red Kite

这是一道全新的 RAW 调试题，使用的不是之前的 landscape RAW。

## 这次的素材链

我先生成了一张新的参考场景：

- 蓝色天空
- 绿色草地
- 一个小孩
- 一只红色风筝

相关文件：

- 参考图：
  `kite_challenge_reference.png`
- 参考 RGB 原图：
  `kite_challenge_reference_rgb24_640x480.raw`

然后我用这张参考图生成了一份**故意带缺陷的 Bayer RAW**：

- `kite_challenge_sensor_rggb8_640x480.raw`

## 你这次要做什么

你不是去追原始参考图 `kite_challenge_reference.png`，
而是去追我已经用这份坏 RAW 调出来的“正常图方向”：

- 目标图：
  `kite_challenge_target_normal/04_display_preview.png`

你的起点是我故意设错的版本：

- 错误起点：
  `kite_challenge_badstart_final/04_display_preview.png`

## 固定输入

你必须固定使用：

- `kite_challenge_sensor_rggb8_640x480.raw`

调参时必须带：

- `--skip-generate`

## 你可以调的参数

- `--black-level`
- `--wb-r`
- `--wb-g`
- `--wb-b`
- `--lsc-strength`
- `--gamma`
- `--ccm-strength`
- `--saturation`

## 推荐起跑命令

你可以直接从错误起点开始：

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\kite_challenge_sensor_rggb8_640x480.raw --output-dir .\kite_user_try_00 --black-level 20 --wb-r 0.70 --wb-g 1.70 --wb-b 1.80 --lsc-strength 0.08 --gamma 3.00
```

之后每次新建一个目录，例如：

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\kite_challenge_sensor_rggb8_640x480.raw --output-dir .\kite_user_try_01 ...
```

## 重点看什么

每次至少看：

- `01_black_level_preview.png`
- `02_lsc_wb_preview.png`
- `04_display_preview.png`

## 你要追的观感

目标图的关键观感是：

1. 天空回到比较正常、柔和的蓝色。
2. 草地恢复成正常绿色，不再发青发灰。
3. 红风筝要重新变得醒目。
4. 人物和拉线不能完全淹掉。
5. 整体画面从“坏 RAW 的偏青错误图”回到“可接受的正常图”。

## 我给你的工作方式建议

按这个顺序调：

1. `black level`
2. `wb-r / wb-g / wb-b`
3. `lsc-strength`
4. `gamma`
5. `ccm-strength / saturation`

## 交作业方式

你调完之后，把你觉得最接近目标图的目录名告诉我，例如：

- `kite_user_try_05`

我会按下面 3 件事继续带你走：

1. 你已经调对了哪一层
2. 还差在哪个参数组
3. 下一步最值得先拧哪个参数

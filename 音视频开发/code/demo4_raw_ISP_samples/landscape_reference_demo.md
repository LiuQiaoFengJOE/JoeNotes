# Landscape Reference Tuning Demo

目标：把 `landscape_challenge_sensor_rggb8_640x480.raw` 尽量调到接近 `landscape_challenge_reference.png`。

原则：

1. 调参时固定同一份 RAW，不要重生输入。
2. 每次尝试输出到一个新目录。
3. 按固定顺序调：`black level -> wb -> lsc -> gamma`。
4. 重点看：
   - `01_black_level_preview.png`
   - `02_lsc_wb_preview.png`
   - `04_display_preview.png`

## Step 0: bad start

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\demo_step_00_badstart --black-level 16 --wb-r 0.55 --wb-g 1.85 --wb-b 2.10 --lsc-strength 0.12 --gamma 3.05
```

看点：

- 最终图明显偏青。
- 黑位太低，整体发飘。
- 边角亮度关系不自然。

## Step 1: fix black level first

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\demo_step_01_black --black-level 60 --wb-r 0.55 --wb-g 1.85 --wb-b 2.10 --lsc-strength 0.12 --gamma 3.05
```

看 `demo_step_01_black/01_black_level_preview.png`：

- 暗部会比 bad start 更沉下去。
- 这一步只修“地基”，颜色还不对是正常的。

## Step 2: fix white balance

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\demo_step_02_wb --black-level 60 --wb-r 3.20 --wb-g 1.15 --wb-b 0.85 --lsc-strength 0.12 --gamma 3.05
```

看 `demo_step_02_wb/04_display_preview.png`：

- 整体偏青会明显缓解。
- 这一步先修颜色方向，不用急着追最终观感。

## Step 3: fix lens shading

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\demo_step_03_lsc --black-level 60 --wb-r 3.20 --wb-g 1.15 --wb-b 0.85 --lsc-strength 1.10 --gamma 3.05
```

看 `demo_step_03_lsc/02_lsc_wb_preview.png`：

- 这一阶段主要看中心到边角的亮度关系。
- 不要只用最终图判断这一步对不对。

## Step 4: gamma last

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\demo_step_04_gamma --black-level 60 --wb-r 3.20 --wb-g 1.15 --wb-b 0.85 --lsc-strength 1.10 --gamma 2.30
```

看 `demo_step_04_gamma/04_display_preview.png`：

- 这一阶段才开始修整体显示观感。
- `gamma` 只管观感层次，不用来救偏色。

## Step 5: refit toward reference

这是当前目录里最值得作为“接近 reference”的示范参数：

```powershell
.\simple_isp_demo.exe --skip-generate --input-bayer .\landscape_challenge_sensor_rggb8_640x480.raw --output-dir .\demo_step_05_refit --black-level 68 --wb-r 3.40 --wb-g 1.15 --wb-b 0.82 --lsc-strength 1.55 --gamma 2.20
```

这组参数来自 challenge 重建命令里写死的生成参数，因此它是当前 demo 内最强的已知锚点，不是拍脑袋试出来的。

## 结果对照

- 参考图：`landscape_challenge_reference.png`
- 错误起点：`demo_step_00_badstart/04_display_preview.png`
- 最终示范：`demo_step_05_refit/04_display_preview.png`

## 这套 demo 里的判断标准

算“调到位”，至少要满足：

1. `01_black_level_preview.png` 里暗部不发灰，也没死黑。
2. `02_lsc_wb_preview.png` 里边角与中心的亮度关系基本合理。
3. `04_display_preview.png` 的天空、山体、水面、前景层次接近参考图。
4. 调参过程有目录留档，能回比每一步变化。

## 还能调什么

在固定 challenge RAW 的前提下，除了前面 5 个主旋钮，还能继续试：

- `--ccm-strength`
  - 作用：控制固定颜色矩阵的介入强度
  - 适合处理：整体色相还是太偏、颜色分离过强、风格太“染”
- `--saturation`
  - 作用：控制 CCM 之后的色彩强度
  - 适合处理：画面太“冲”或者太“灰”

我实际补跑过几组：

- `demo_step_06_ccm_low`
- `demo_step_07_sat_low`
- `demo_step_08_ccm_sat_low`
- `demo_step_09_warm_try`

结论是：

- 降一点 `saturation` 会让观感更柔和一点。
- 降一点 `ccm-strength` 能稍微减轻“染色感”。
- 但这两个旋钮都只能微调风格，不能把当前 demo 直接拉成 reference 那种晚霞暖调。

## 为什么还是差很多

因为这个 demo 的 ISP 本身就很简化，只做了：

1. black level
2. LSC + WB
3. demosaic
4. 固定 CCM
5. saturation
6. gamma
7. RGB 转 YUV

它没有这些能力：

- 独立的色调映射
- 分区/局部对比度控制
- 更自由的 tone curve
- 单独的高光压制
- 分通道 LSC
- 更复杂的颜色校正模型

所以它能做到的是：

- 从“明显错误”调到“基本接近”

但做不到：

- 把参考图那种暖色天空、高光层次、山体色调、反射氛围完整还原出来

如果要继续逼近 reference，当前目录里最值得继续试的方向是：

1. 在 `demo_step_05_refit` 基础上轻降 `--saturation`
2. 小幅下调 `--ccm-strength`
3. 再微调 `wb-r / wb-b`

但要接受一个现实：再怎么拧，这个 demo 也有明显上限。

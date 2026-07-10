
# dng_toolkit

纯 Python 的 Bayer RAW → DNG 写入工具，支持通过 YAML 配置灵活设置 DNG 标签（CFA、黑白电平、色彩矩阵、白平衡、有效区域、默认裁剪等）。

## 依赖
```bash
pip install numpy tifffile pyyaml
```

## 快速开始
```bash
python run_example.py
# 输出 out_example.dng，可在 RawTherapee / Adobe Camera Raw 等打开
```

## 自定义
- 编辑 `dng_config.yaml` 调整你的相机/RAW参数
- 如你的 RAW 是厂商打包格式，请先解包为 (H,W) 的 uint16，再调用 `write_bayer_dng`
- 将 `run_example.py` 中的假 RAW 替换为你的实际 RAW 阵列即可

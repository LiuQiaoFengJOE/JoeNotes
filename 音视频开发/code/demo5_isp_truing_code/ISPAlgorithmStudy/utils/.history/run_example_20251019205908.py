
import numpy as np
from dng_config import DngConfig
from dng_tool import write_bayer_dng, CFA

def main():
    cfg = DngConfig.load_from_yaml("dng_config.yaml")

    H, W = cfg.geometry.height, cfg.geometry.width
    bit = cfg.levels.bit_depth
    maxv = (1<<bit) - 1

    # 生成一张假 RAW：中心亮、边缘暗的径向梯度，便于肉眼验证
    y, x = np.ogrid[:H, :W]
    cy, cx = H/2.0, W/2.0
    r = np.sqrt((y-cy)**2 + (x-cx)**2)
    r = (r / r.max())
    raw = ( (1.0 - r) * maxv ).astype(np.uint16)

    args = cfg.to_writer_args()
    cfa = CFA(cfg.cfa.pattern)

    # 把进阶标签（如HSM）合进主写入
    extra = cfg.extra_exif_tags()

    write_bayer_dng(
        path="out_example.dng",
        raw=raw,
        cfa=cfa,
        extra_extratags=extra,
        compression=1,  # 也可改为 cfg.storage.compression
        rowsperstrip=cfg.storage.rows_per_strip,
        planarconfig=cfg.storage.planar_config,
        software=cfg.core.software,
        description=cfg.core.description,
        **args
    )
    print("Wrote: out_example.dng")

if __name__ == "__main__":
    main()


import numpy as np
import tifffile

# ---- 简易 CFA 描述与映射 ----
class CFA:
    _map = {
        "RGGB": [0, 1, 1, 2],
        "GRBG": [1, 0, 2, 1],
        "GBRG": [1, 2, 0, 1],
        "BGGR": [2, 1, 1, 0],
    }
    _plane_colors = [0, 1, 2]  # 0=R,1=G,2=B (DNG CFAPlaneColor)

    def __init__(self, pattern: str):
        p = pattern.upper()
        if p not in self._map:
            raise ValueError(f"Unsupported CFA pattern: {pattern}")
        self.pattern = p
        self.pattern_2x2 = self._map[p]

    @property
    def cfa_repeat_dim(self):
        return (2, 2)

    @property
    def cfa_pattern(self):
        return self.pattern_2x2

    @property
    def cfa_plane_color(self):
        return self._plane_colors  # [R,G,B]

    @property
    def cfa_layout(self):
        # 1 = Rectangular (standard Bayer)
        return 1


# ---- 工具函数：把 raw 规范化到 16bit 存储域（uint16） ----
def normalize_raw_to_uint16(raw, bit_depth, black_level, white_level,
                            clip_before_scale=True):
    raw = np.asarray(raw)

    # 支持标量或四值/2x2黑白电平，统一得到标量黑白电平以便归一化存储（DNG仍会记录真实Black/White）
    def to_scalar(val, name):
        arr = np.asarray(val)
        if arr.ndim == 0:
            return float(arr)
        if arr.size == 4 or arr.size == 2*2:
            # 取平均做归一化；原始细分黑电平仍会通过标签保留
            return float(arr.mean())
        if arr.size == 1:
            return float(arr.ravel()[0])
        raise ValueError(f"{name} expects scalar or 2x2/length-4, got shape {arr.shape}")

    bl = to_scalar(black_level, "black_level")
    wl = to_scalar(white_level, "white_level")

    # 转浮点做缩放
    raw_f = raw.astype(np.float32)

    if clip_before_scale:
        raw_f = np.clip(raw_f, bl, wl)

    denom = max(wl - bl, 1e-6)
    norm = (raw_f - bl) / denom
    norm = np.clip(norm, 0.0, 1.0)
    out = (norm * 65535.0 + 0.5).astype(np.uint16)
    return out


# ---- 主函数：写DNG ----
def write_bayer_dng(
    path: str,
    raw: np.ndarray,
    bit_depth: int,
    cfa: CFA,
    black_level=0,
    white_level=None,
    unique_camera_model: str = "Generic Camera",
    as_shot_neutral=None,                 # [Nr, Ng, Nb]
    color_matrix1=None,                   # 9 values row-major
    calibration_illuminant1: int = 21,    # 21=D65
    active_area=None,                     # (top, left, bottom, right)
    default_crop_origin=None,             # (x,y)
    default_crop_size=None,               # (w,h)
    normalize_to_16bit=True,
    clip_before_scale=True,
    # 可选：额外自定义 extratags（会合并写入）
    extra_extratags=None,
    compression: int = 1,
    rowsperstrip: int = 16,
    planarconfig: int = 1,
    software: str = "dng_tool 1.0",
    description: str = "Bayer RAW to DNG"
):
    """
    将 Bayer RAW 写为 DNG（基于TIFF容器）。
    """
    raw = np.asarray(raw)
    if raw.ndim != 2:
        raise ValueError("raw must be 2D Bayer mosaic")

    h, w = raw.shape

    if white_level is None:
        white_level = (1 << bit_depth) - 1

    # 生成用于存储的像素（不改变原数据的统计，仅线性映射到16bit）
    if normalize_to_16bit:
        img16 = normalize_raw_to_uint16(
            raw, bit_depth, black_level, white_level, clip_before_scale=clip_before_scale
        )
        bits_per_sample = 16
        dng_white_level = np.array([white_level], dtype=np.uint32)
    else:
        # 直接以 >=bit_depth 的uint16存储
        if raw.dtype != np.uint16:
            img16 = raw.astype(np.uint16)
        else:
            img16 = raw
        bits_per_sample = max(bit_depth, 8)
        dng_white_level = np.array([white_level], dtype=np.uint32)

    # 组织BlackLevel：支持标量 / 4值 / 2x2
    def format_blacklevel(bl):
        arr = np.asarray(bl, dtype=np.float32)
        if arr.ndim == 0:
            repeat = (1, 1)
            values = arr.reshape(1)
        elif arr.size in (4,):
            repeat = (2, 2)
            values = arr.reshape(4)
        elif arr.shape == (2, 2):
            repeat = (2, 2)
            values = arr.reshape(4).astype(np.float32)
        else:
            raise ValueError("black_level expects scalar or length-4 / 2x2 array")
        return repeat, values

    bl_repeat, bl_values = format_blacklevel(black_level)

    # 基本边界标签
    if active_area is None:
        active_area = (0, 0, h, w)
    if default_crop_origin is None:
        default_crop_origin = (0, 0)
    if default_crop_size is None:
        default_crop_size = (w, h)

    # DNG强制/常用标签（参考Adobe DNG 1.4规范）
    extratags = []

    # DNGVersion (50706) & BackwardVersion (50707)
    extratags.append((50706, "BYTE", 4, (1, 4, 0, 0), True))   # DNG 1.4.0.0
    extratags.append((50707, "BYTE", 4, (1, 2, 0, 0), True))   # Backward 1.2.0.0

    # UniqueCameraModel (50708) ASCII, 必需
    ucm = unique_camera_model + "\x00"
    extratags.append((50708, "ASCII", len(ucm), ucm, True))

    # CFAPlaneColor (50710) & CFALayout (50711)
    extratags.append((50710, "BYTE", len(cfa.cfa_plane_color), tuple(cfa.cfa_plane_color), True))
    extratags.append((50711, "SHORT", 1, cfa.cfa_layout, True))

    # CFARepeatPatternDim (33421) & CFAPattern (33422)
    extratags.append((33421, "SHORT", 2, tuple(cfa.cfa_repeat_dim), True))
    extratags.append((33422, "BYTE", 4, tuple(cfa.cfa_pattern), True))

    # BlackLevelRepeatDim (50713) & BlackLevel (50714)
    extratags.append((50713, "SHORT", 2, tuple(bl_repeat), True))
    bl_vals = tuple(map(float, bl_values.tolist()))
    extratags.append((50714, "DOUBLE", len(bl_vals), bl_vals, True))

    # WhiteLevel (50717)
    wl_arr = tuple(int(x) for x in np.atleast_1d(dng_white_level))
    extratags.append((50717, "LONG", len(wl_arr), wl_arr, True))

    # ActiveArea (50829) top,left,bottom,right
    extratags.append((50829, "LONG", 4, tuple(map(int, active_area)), True))

    # DefaultCropOrigin (50719) & DefaultCropSize (50720)
    extratags.append((50719, "RATIONAL", 2, (int(default_crop_origin[0]), 1, int(default_crop_origin[1]), 1), True))
    extratags.append((50720, "RATIONAL", 2, (int(default_crop_size[0]), 1, int(default_crop_size[1]), 1), True))

    # CalibrationIlluminant1 (50778)
    extratags.append((50778, "SHORT", 1, int(calibration_illuminant1), True))

    # ColorMatrix1 (50721) – 建议提供
    if color_matrix1 is not None:
        cm1 = np.asarray(color_matrix1, dtype=np.float64)
        if cm1.size != 9:
            raise ValueError("color_matrix1 must have 9 elements (3x3)")
        extratags.append((50721, "DOUBLE", 9, tuple(cm1.ravel().tolist()), True))

    # AsShotNeutral (50728) – 建议提供
    if as_shot_neutral is not None:
        asn = np.asarray(as_shot_neutral, dtype=np.float64)
        if asn.size != 3:
            raise ValueError("as_shot_neutral must be length-3 [Nr,Ng,Nb]")
        extratags.append((50728, "DOUBLE", 3, tuple(asn.tolist()), True))

    # 合并外部扩展标签
    if extra_extratags:
        extratags.extend(extra_extratags)

    # PhotometricInterpretation = 32803 (CFA)
    photometric = 32803

    with tifffile.TiffWriter(path, bigtiff=False, byteorder="<") as tiff:
        tiff.write(
            data=img16,
            dtype=np.uint16,
            photometric=photometric,
            planarconfig=planarconfig,
            compression=compression,
            software=software,
            description=description,
            metadata=None,
            extratags=extratags,
            rowsperstrip=rowsperstrip,
            bitspersample=bits_per_sample,
            samplesperpixel=1,
            sampleformat=1
        )


# ---- 便捷：从裸字节缓冲解包为二维RAW ----
def unpack_bayer_from_bytes(buf: bytes, width: int, height: int, bit_depth: int, byteorder="little"):
    """
    将裸字节缓冲区解包成 (H, W) 的 uint16（右对齐）。
    这里只处理每像素16bit对齐的简单场景。若为厂商packed格式（如5字节存4个10bit），请先外部解包。
    """
    if bit_depth in (8, 10, 12, 14, 16):
        arr = np.frombuffer(buf, dtype="<u2" if byteorder == "little" else ">u2", count=width*height)
        arr = arr.reshape(height, width)
        return arr
    else:
        raise ValueError("Unsupported bit depth for this simple unpacker")


from dataclasses import dataclass, field
from typing import List, Tuple, Optional, Dict, Any
import yaml
import numpy as np

@dataclass
class CoreCfg:
    dng_version: Tuple[int,int,int,int] = (1,4,0,0)
    backward_version: Tuple[int,int,int,int] = (1,2,0,0)
    make: Optional[str] = None
    unique_camera_model: str = "Generic Camera"
    software: str = "dng_tool 1.0"
    description: str = "Bayer RAW to DNG"

@dataclass
class GeometryCfg:
    width: int
    height: int
    orientation: int = 1  # EXIF Orientation

@dataclass
class CfaCfg:
    pattern: str = "RGGB"         # RGGB/GRBG/GBRG/BGGR
    layout: int = 1               # 1=Rectangular
    cfa_plane_color: Tuple[int,int,int] = (0,1,2)

@dataclass
class LevelsCfg:
    bit_depth: int = 12
    black_level: Dict[str, Any] = field(default_factory=lambda: {"repeat_dim":[1,1], "values":[64]})
    white_level: int = 4095
    normalize_to_16bit: bool = True
    clip_before_scale: bool = True

@dataclass
class RegionCfg:
    active_area: Tuple[int,int,int,int]
    default_crop_origin: Tuple[int,int] = (0,0)
    default_crop_size: Tuple[int,int] = None  # 若None则用(width,height)

@dataclass
class ColorCfg:
    calibration_illuminant1: int = 21
    color_matrix1: Optional[List[List[float]]] = None  # 3x3
    as_shot_neutral: Optional[Tuple[float,float,float]] = None

@dataclass
class BaselineCfg:
    exposure: Optional[float] = None
    noise: Optional[float] = None
    sharpness: Optional[float] = None

@dataclass
class ExifCfg:
    exposure_time: Optional[str] = None
    f_number: Optional[float] = None
    iso: Optional[int] = None
    focal_length_mm: Optional[float] = None
    exposure_program: Optional[int] = None
    exposure_compensation: Optional[float] = None
    metering_mode: Optional[int] = None

@dataclass
class GpsCfg:
    lat_ref: Optional[str] = None
    lon_ref: Optional[str] = None
    alt_ref: Optional[int] = None
    lat: Optional[float] = None
    lon: Optional[float] = None
    alt: Optional[float] = None

@dataclass
class ProfileCfg:
    hue_sat_map_dims: Optional[Tuple[int,int,int]] = None
    hue_sat_map_data1: Optional[List[float]] = None
    hue_sat_map_encoding: Optional[str] = None

@dataclass
class OpcodesCfg:
    opcode_list2: List[str] = field(default_factory=list)

@dataclass
class StorageCfg:
    compression: int = 1
    rows_per_strip: int = 16
    planar_config: int = 1

@dataclass
class DngConfig:
    core: CoreCfg
    geometry: GeometryCfg
    cfa: CfaCfg
    levels: LevelsCfg
    region: RegionCfg
    color: ColorCfg = field(default_factory=ColorCfg)
    baseline: BaselineCfg = field(default_factory=BaselineCfg)
    exif: ExifCfg = field(default_factory=ExifCfg)
    gps: GpsCfg = field(default_factory=GpsCfg)
    profile: ProfileCfg = field(default_factory=ProfileCfg)
    opcodes: OpcodesCfg = field(default_factory=OpcodesCfg)
    storage: StorageCfg = field(default_factory=StorageCfg)

    @staticmethod
    def load_from_yaml(path: str) -> "DngConfig":
        with open(path, "r", encoding="utf-8") as f:
            y = yaml.safe_load(f)

        # 小工具：把 list 转 tuple
        def to_tuple(x, n=None):
            if x is None: return None
            t = tuple(x)
            if n is not None and len(t)!=n:
                raise ValueError(f"Length mismatch, expect {n}, got {len(t)}")
            return t

        core = CoreCfg(
            dng_version=to_tuple(y["core"].get("dng_version", [1,4,0,0]), 4),
            backward_version=to_tuple(y["core"].get("backward_version", [1,2,0,0]), 4),
            make=y["core"].get("make"),
            unique_camera_model=y["core"].get("unique_camera_model", "Generic Camera"),
            software=y["core"].get("software", "dng_tool 1.0"),
            description=y["core"].get("description", "Bayer RAW to DNG"),
        )

        geometry = GeometryCfg(
            width=y["geometry"]["width"],
            height=y["geometry"]["height"],
            orientation=y["geometry"].get("orientation", 1)
        )

        cfa = CfaCfg(
            pattern=y["cfa"].get("pattern","RGGB"),
            layout=y["cfa"].get("layout",1),
            cfa_plane_color=to_tuple(y["cfa"].get("cfa_plane_color",[0,1,2]),3)
        )

        levels = LevelsCfg(
            bit_depth=y["levels"].get("bit_depth",12),
            black_level=y["levels"].get("black_level", {"repeat_dim":[1,1],"values":[64]}),
            white_level=y["levels"].get("white_level",(1<<y["levels"].get("bit_depth",12))-1),
            normalize_to_16bit=y["levels"].get("normalize_to_16bit", True),
            clip_before_scale=y["levels"].get("clip_before_scale", True)
        )

        region = RegionCfg(
            active_area=to_tuple(y["region"]["active_area"],4),
            default_crop_origin=to_tuple(y["region"].get("default_crop_origin",[0,0]),2),
            default_crop_size=to_tuple(y["region"].get("default_crop_size"),2)                     if y["region"].get("default_crop_size") else None
        )

        cm1 = y.get("color",{}).get("color_matrix1")
        if cm1 is not None:
            # 允许 '0.1; 0.2; 0.3' 形式
            mat = []
            for row in cm1:
                if isinstance(row,str):
                    mat.append([float(x) for x in row.split(";")])
                else:
                    mat.append([float(x) for x in row])
            cm1 = mat

        color = ColorCfg(
            calibration_illuminant1=y.get("color",{}).get("calibration_illuminant1",21),
            color_matrix1=cm1,
            as_shot_neutral=to_tuple(y.get("color",{}).get("as_shot_neutral"),3),
        )

        baseline = BaselineCfg(**y.get("baseline",{}))
        exif     = ExifCfg(**y.get("exif",{}))
        gps      = GpsCfg(**y.get("gps",{}))
        profile  = ProfileCfg(**y.get("profile",{}))
        opcodes  = OpcodesCfg(**y.get("opcodes",{}))
        storage  = StorageCfg(**y.get("storage",{}))

        return DngConfig(core, geometry, cfa, levels, region, color, baseline, exif, gps, profile, opcodes, storage)

    # —— 转换为写入器参数（给 write_bayer_dng）——
    def to_writer_args(self) -> Dict[str, Any]:
        # BlackLevel 处理
        bl = self.levels.black_level
        repeat = tuple(bl.get("repeat_dim",[1,1]))
        values = bl.get("values",[64])
        if repeat==(2,2) and len(values)!=4:
            raise ValueError("When black_level.repeat_dim is [2,2], provide 4 values (R,Gr,Gb,B).")
        if repeat==(1,1) and len(values)!=1:
            raise ValueError("When black_level.repeat_dim is [1,1], provide a single scalar in values.")

        # 默认裁剪尺寸
        crop_size = self.region.default_crop_size or (self.geometry.width, self.geometry.height)

        # 映射成 write_bayer_dng 的关键字段
        args = dict(
            bit_depth=self.levels.bit_depth,
            black_level=np.array(values) if len(values)>1 else float(values[0]),
            white_level=int(self.levels.white_level),
            unique_camera_model=self.core.unique_camera_model,
            as_shot_neutral=self.color.as_shot_neutral,
            color_matrix1=sum(self.color.color_matrix1, []) if self.color.color_matrix1 else None,
            calibration_illuminant1=int(self.color.calibration_illuminant1),
            active_area=tuple(self.region.active_area),
            default_crop_origin=tuple(self.region.default_crop_origin),
            default_crop_size=tuple(crop_size),
            normalize_to_16bit=self.levels.normalize_to_16bit,
            clip_before_scale=self.levels.clip_before_scale
        )
        return args

    # —— 生成 extratags（示例，仅包含 DNG 基础扩展；EXIF/GPS 可按需补充）——
    def extra_exif_tags(self) -> list:
        tags = []
        # DNGVersion / Backward
        dv = tuple(self.core.dng_version); bv = tuple(self.core.backward_version)
        tags.append((50706, "BYTE", 4, dv, True))
        tags.append((50707, "BYTE", 4, bv, True))
        # UniqueCameraModel
        ucm = self.core.unique_camera_model + "\x00"
        tags.append((50708, "ASCII", len(ucm), ucm, True))
        # Profile（HSM 示例）
        if self.profile.hue_sat_map_dims:
            dims = tuple(self.profile.hue_sat_map_dims)
            tags.append((50937, "SHORT", 3, dims, True)) # ProfileHueSatMapDims
        if self.profile.hue_sat_map_data1:
            arr = tuple(self.profile.hue_sat_map_data1)
            tags.append((50938, "FLOAT", len(arr), arr, True)) # ProfileHueSatMapData1
        if self.profile.hue_sat_map_encoding:
            enc = self.profile.hue_sat_map_encoding + "\x00"
            tags.append((50936, "ASCII", len(enc), enc, True))
        return tags

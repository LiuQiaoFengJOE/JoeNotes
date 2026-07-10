# DNG_Creator_Demo.py
import os
import subprocess
import numpy as np

def create_dng_from_raw(raw_file_path, output_dng_path, image_width, image_height, bayer_pattern='RGGB', bits_per_sample=16, make="DemoCam", model="Prototype"):
    """
    一个将Bayer RAW文件转换为DNG的演示函数。
    
    参数:
        raw_file_path (str): 输入的RAW二进制文件路径。
        output_dng_path (str): 输出的DNG文件路径。
        image_width (int): RAW图像的宽度（像素）。
        image_height (int): RAW图像的高度（像素）。
        bayer_pattern (str): Bayer滤镜阵列模式，例如 'RGGB', 'BGGR', 'GRBG', 'GBRG'。
        bits_per_sample (int): 每个像素的比特数，例如 8, 10, 12, 16。
        make (str): 相机厂商信息。
        model (str): 相机型号信息。
    """
    
    # 检查输入的RAW文件是否存在
    if not os.path.isfile(raw_file_path):
        print(f"错误：找不到RAW文件 '{raw_file_path}'")
        return False

    # 准备ExifTool执行命令
    exiftool_cmd = ['exiftool', '-overwrite_original', '-@', '-']
    
    # 构建ExifTool参数
    params = [
        '-F', # 强制创建标签
        f'-MakerNotes={make}',
        f'-Model={model}',
        f'-ImageWidth={image_width}',
        f'-ImageHeight={image_height}',
        '-Orientation=Horizontal (normal)', # 设置图像方向
        f'-BitsPerSample={bits_per_sample}',
        '-SamplesPerPixel=1', # RAW数据通常为单通道
        '-PhotometricInterpretation=Color Filter Array', # 指明为CFA（色彩滤镜阵列）图像
        '-CFARepeatPatternDim=2 2', # Bayer模式是2x2重复的
        f'-CFAPattern2={bayer_pattern}', # 设置Bayer模式
        '-BlackLevel=0', # 黑电平。根据实际情况调整，这里设为0
        '-WhiteLevel=65535', # 白电平。对于16位数据，通常为(2^16-1)=65535
        '-DNGVersion=1 5 0 0', # DNG格式版本
        '-DNGBackwardVersion=1 4 0 0',
        '-ColorMatrix1=1 0 0 0 1 0 0 0 1', # 色彩矩阵（示例为单位矩阵，需根据相机特性调整）
        '-CalibrationIlluminant1=Unknown', # 校准光源
        f'-{make}:Model={model}',
    ]

    # 指定源文件（你的RAW数据文件）
    params.append(f'-srcraw={raw_file_path}')
    
    # 最终输出的DNG文件名
    params.append(output_dng_path)

    try:
        # 执行ExifTool命令
        print("正在生成DNG文件，这可能需要几秒钟...")
        result = subprocess.run(exiftool_cmd, input='\n'.join(params), text=True, capture_output=True)
        
        # 检查执行结果
        if result.returncode == 0:
            print(f"成功！DNG文件已保存至: {output_dng_path}")
            # 打印ExifTool的标准输出和信息（如果有）
            if result.stdout:
                print("ExifTool 信息:", result.stdout)
            return True
        else:
            print("ExifTool 执行出错:")
            print("错误信息:", result.stderr)
            print("标准输出:", result.stdout)
            return False
            
    except FileNotFoundError:
        print("错误：未找到 'exiftool' 命令。请确保ExifTool已安装并添加到系统PATH环境变量中。")
        return False
    except Exception as e:
        print(f"发生未知错误: {str(e)}")
        return False

# === 使用示例 ===
if __name__ == "__main__":
    # 请根据你的实际情况修改以下参数
    input_raw = "./image.raw"  # 你的输入RAW文件
    output_dng = "./output_image.dng" # 输出的DNG文件名
    
    img_width = 3648   # 请替换为你的RAW图像实际宽度
    img_height = 2736  # 请替换为你的RAW图像实际高度
    bayer_pat = 'RGGB' # 请替换为你的传感器Bayer模式
    bit_depth = 16     # 请替换为你的RAW数据位深

    # 调用函数进行转换
    success = create_dng_from_raw(
        raw_file_path=input_raw,
        output_dng_path=output_dng,
        image_width=img_width,
        image_height=img_height,
        bayer_pattern=bayer_pat,
        bits_per_sample=bit_depth,
        make="YourCameraBrand", # 可修改相机品牌
        model="YourCameraModel" # 可修改相机型号
    )
    
    if success:
        print("转换完成！")
    else:
        print("转换失败，请检查以上错误信息。")
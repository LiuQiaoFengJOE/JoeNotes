# hclinux change list

## Master

* TBD

## hclinux-2025.12.y

- 新增功能 - 海奇HOS应用开发架构：

  - 新增加应用apps-hstudio-projector


  - apps-hstudio-projector功能，界面，基本和apps-projector保持一致


  - 基于全新图像化开发工具HStudio，HStudio在PC Windows系统运行，完全可视化，快速，所见即所得开发界面。


  - HStudio可以生成基于PC的lvgl模拟器代码LVGL界面代码；可以生成基于hcrtos/hclinux的LVGL界面代码


  - 提供PC的lvgl模拟器(VScode)，通过远程调用(USB)平台的服务，在PC模拟器直观模拟在平台上的运行效果


  - 新增系统服务层(hsvc)，界面通过服务层，调用系统/驱动接口。各层之间独立运行：

    ```
    UI -> hsvc -> hudi -> system/dirver
    ```


  - 人机界面和服务控制分离，完全解耦。没有启动服务，界面的调用就不生效，编译不会报错


- 支持png边读边解边缩，可以在128M方案上播放50M以上png图片

- 优化图像解码，使用图像边解边缩功能，更好支持超大图片的解码

- 支持图像解码多宫格模式支持，提供更丰富的显示效果

  

## hclinux-2024.08.y

* hccast aircast/miracast/iptv: 删除了废弃的API，并规范了部分API命名。此优化删除或修改了部份旧的API，因此升级SDK时上层应用需要做对应修改，主要修改有以下接口文件：
  * hccast_com.h
  * hccast_air.h
  * hccast_mira.h
* 支持AirP2P/Miracast共存模式
* Youtube支持历史记录
* 应用增加背光亮度调节功能
* 支持多屏异显功能
* 支持多路解码功能
* 支持HDMI CEC功能
* 优化了OTA升级程序以避免升级过程中文件系统出错的问题
* 新增驱动接口封装层(HUDI)
* AUDIO增加新功能
  * 音轨之间平滑切换(使能后会消耗更多的内存/CPU)
  * 音频的倍速播放，【0.5~3】倍速
  * 音频的变调功能，【-12 ~ 12】个半音阶
  * 支持dts音频解码(需要license，仅供内部研发参考)
  * 支持ac3/eac3/truehd/dts的bypass(需要license，仅供内部研发参考)
* 多媒体新增功能
  * 支持无缝循环播放
  * transcode支持传入内存地址

## hclinux-2024.02.y

* 支持hcprogrammer

  * 1代/2代均支持USB非空片升级
  * 2代支持空片升级(依赖USB upgrade键)
  * 1代/2代均支持UART空片升级
  
* 投影仪应用新功能：记忆播放/自动播放。支持多个外设记忆播放，上电，插拔外设自动播放上次播放文件(当前不在播放状态)，30秒记录一次播放时间。实现了视频，音频，电子书的记忆播放。如果多种媒体都有记忆，优先视频的记忆播放。

  打开功能：CONFIG_APPS_MEDIA_MEMORY_PLAY，如下设置

  ```
  make menuconfig
   > External options > Applications Configuration > Applications(projector) Configuration [*] Support media memory play
  ```

  

* 投影仪应用新功能： 缩略图浏览显示（视频，图片，音乐）。在媒体列表下，转码显示媒体缩略图。目前，缩略图和预览只能选择一项。

  打开功能：CONFIG_APPS_TRANSCODE_THUMBNIAL_SHOW

```
make menuconfig
 > External options > Applications Configuration > Applications(projector) Configuration 
 						[*] Enable image tanscode thumbnail showed in media list
```



- 投影仪应用新功能： 图片效果选择（全屏，剪切等）。在进入全屏播放时，播放控制栏增加一个按钮选择图片效果：全屏，自动，剪切等。下张图片显示时将按这种效果显示

  打开功能： CONFIG_APPS_IMAGE_DISPLAY_MODE_CHANGE

```
make menuconfig
 > External options > Applications Configuration > Applications(projector) Configuration  
 				[*]   Enable change image display mode(realsize, full,etc) in playbar 
```


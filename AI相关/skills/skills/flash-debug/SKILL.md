---
name: flash-debug
description: 用 Keil 命令行编译 + J-Link 烧录 STM32F407，并通过串口看日志 / 启动单步调试。当用户说烧进去看看、下载固件、上板验证、单步调试、看串口日志、跑一下固件时触发。这是嵌入式版的"冒烟测试"。
---

把固件编译、用 J-Link 烧进 STM32F407、看串口日志，必要时启动单步调试。
这是 Web demo 里 `make run` 的嵌入式对应物——固件必须真烧上板才算"跑起来"。

工具链：Keil MDK 命令行编译 → JLink.exe 脚本化烧录 → 串口终端看日志 →
单步调试交给 Keil/VSCode 的人工 GUI（Claude 准备好配置后移交）。

## 1. 编译（Claude 可自动跑）

```bash
scripts/build.bat       # 内部：UV4.exe -b proj.uvprojx -j0 -o build_log.txt
```

UV4 异步返回，**必须读 `build_log.txt`** 判断：出现 `0 Error(s)` 才算过；有 error
先修，别烧。顺带看 `Program Size: Code/RO/RW/ZI`，估 Flash/RAM 占用是否溢出
（F407VG: 1MB Flash / 192KB RAM，其中 64KB 是 DMA 不可达的 CCM）。

编译失败时：把 `build_log.txt` 的报错行贴出来定位；常见是头文件路径、宏未定义、
CubeMX 重生成覆盖了 USER CODE。

## 2. 烧录（Claude 可自动跑，不依赖 Keil GUI）

```bash
scripts/flash.bat       # 内部：JLink.exe -device STM32F407VG -if SWD -speed 4000
                        #        -autoconnect 1 -CommanderScript scripts/flash.jlink
```

`scripts/flash.jlink` 内容（已随仓库提供）：
```
si SWD
speed 4000
r
halt
loadfile build/<proj>.hex      ; 也可 loadfile build/<proj>.axf（ELF）
r
g
q
```

要点：
- `device` 名按实际芯片改（STM32F407VG / VE / ZG / IG…），错了会连不上或校验失败。
- 连不上排查：SWD 接线(SWDIO/SWCLK/GND/复位)、目标供电、`speed` 调低到 1000、
  芯片是否读保护(RDP)——必要时 `JLink.exe` 里 `unlock STM32F4` 解锁（会清片，先确认）。
- 想用 Keil 配好的下载设置烧，也可 `UV4.exe -f proj.uvprojx -o flash_log.txt`，
  但本仓库优先用上面的 JLink 脚本，可被完全自动化。

## 3. 看日志：串口 (USART)

```bash
scripts/serial.bat COM3 115200   # 打开串口持续打印 LOG() 输出（COM口/波特率按实际传参）
```

前提：固件已实现 `bsp/log.c` 的 `LOG(...)`，把 `printf`/格式化结果通过 `USART1`
（默认 PA9 TX，115200-8-N-1）发出。串口日志的好处是**不挑调试器**——ST-Link 虚拟
串口、J-Link、独立 USB-TTL 模块都能收，换板子/换仿真器都不用改代码。

要点：
- 先确认 COM 口号：`powershell "[System.IO.Ports.SerialPort]::GetPortNames()"` 列出可用串口。
- 波特率必须和固件一致（默认 115200），不一致会是乱码。
- 串口打印是**阻塞**的：只在任务/主循环里打，**不要在 ISR、紧时序、临界区里打**，否则
  拖慢中断响应、打乱实时性。要在 ISR 附近记录就先置标志，回到任务里再打。
- 没输出时排查：TX 引脚/接线、GND 共地、USART 时钟与波特率配置、是否真跑到打印点、
  USB-TTL 的 RX 是否接到 MCU 的 TX。

## 4. 单步调试（GUI，人工驱动——Claude 准备配置后移交）

Claude 不能点 GUI。两条路，按你习惯选其一，Claude 负责把配置就绪：

**A. VSCode + Cortex-Debug（贴合"VSCode 写代码"）**
确保 `.vscode/launch.json` 形如（已随仓库提供模板）：
```jsonc
{
  "type": "cortex-debug", "request": "launch", "servertype": "jlink",
  "device": "STM32F407VG", "interface": "swd",
  "executable": "build/<proj>.axf",
  "runToEntryPoint": "main", "svdFile": "STM32F407.svd"
}
```
然后请用户在 VSCode 里按 F5（让用户操作，或提示 `! code .` 打开）。日志仍走串口：
另开一个终端跑 `scripts/serial.bat COMx 115200` 看 `LOG()` 输出（与单步调试并行）。

**B. Keil uVision**
Options for Target → Debug → 选 `J-LINK / J-TRACE Cortex` → Settings 里 SWD、
4MHz、Flash Download 勾 "Reset and Run"。请用户在 uVision 里 Ctrl+F5 进调试、
F11 单步。Claude 可代为核对/修改这些工程设置项，但进调试需用户点。

移交话术示例：给出断点建议位置（函数:行）、要观察的变量/寄存器、预期时序，
让用户单步后把现象/串口输出回贴，Claude 据此继续定位。

## 输出

每次执行后简明汇报：编译是否 0 错 + size、烧录是否成功、串口关键日志摘录；
若是调试移交，给出"在哪打断点、看什么、预期什么"的清单。失败如实报错误原文。

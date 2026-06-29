---
name: fw-review
description: 审核 STM32/FreeRTOS 固件改动，聚焦正确性 bug、中断与并发安全、内存/栈、实时性、分层与项目约定。当用户说审核固件、code review、检查这段 C 代码、看看有没有问题、烧板前自检时触发。
---

审核本工程的 C 固件改动，给出按严重程度排序的问题清单。

## 范围

默认审核**当前未提交的改动**。先确定审什么：

```bash
git diff --stat              # 看改了哪些文件
git diff                     # 工作区改动
git diff --staged            # 暂存区改动
```

若用户指定文件/commit 范围，就审那个范围。只审改动相关代码，不泛读整仓。
`Drivers/`（HAL/CMSIS 厂商层）与 CubeMX 生成的非 USER CODE 区不审。

## 审核维度（按优先级，先找真 bug 再谈整洁）

### 1. 正确性（最高）
- 边界：数组/缓冲区越界、off-by-one、整数溢出（尤其 `uint8/16` 运算与移位）、
  无符号下溢（`u - v` 当 v>u）、除零、定点换算精度丢失
- 指针：空指针、未初始化、悬垂；`memcpy/memset` 长度与 `sizeof` 是否匹配
- 返回值：HAL/RTOS API 返回值是否检查？错误路径是否提前 return 且不泄漏资源？
- 逻辑：`if` 漏 `break`/`return` 导致贯穿；`==` 误写 `=`；位操作掩码错

### 2. 中断与并发安全（嵌入式核心）
- **`volatile`**：ISR 与主循环/任务共享的变量、寄存器映射指针是否 `volatile`？
  缺了会被编译器优化掉读取，调试版正常、Release 版诡异。
- **ISR 纪律**：中断里是否误调阻塞 HAL（`HAL_Delay`、轮询超时）、`printf`、
  `malloc`？是否过长？是否只置标志/发队列？
- **FreeRTOS from-ISR**：中断里调的是否 `...FromISR` 版本？`xHigherPriorityTaskWoken`
  是否传入并在末尾 `portYIELD_FROM_ISR`？
- **中断优先级**：调 FreeRTOS API 的中断，其抢占优先级数值是否 ≥
  `configMAX_SYSCALL_INTERRUPT_PRIORITY`？否则临界区失效，偶发死机。
- **临界区/原子**：多字节共享数据的读改写是否加保护（`taskENTER_CRITICAL`/关中断）？
  保护范围是否过大（影响实时性）或过小（仍有竞态）？
- **任务通信**：是否用队列/信号量而非裸全局变量？队列满/空、信号量超时是否处理？

### 3. 内存与栈（无 MMU，崩了很隐蔽）
- **动态内存**：是否违反"无 malloc"约定？若用，碎片/失败是否处理？
- **栈深**：任务栈是否够（递归、大局部数组、深调用链、`printf` 很吃栈）？
  是否开了栈溢出检测（`configCHECK_FOR_STACK_OVERFLOW`）？
- **DMA 缓冲**：是否落在 **CCM(0x10000000) 致 DMA 读不到**？是否对齐、`volatile`、
  长度不越界？收发期间缓冲是否被复用？
- 大对象是否误放栈上而非 static？const 表是否放 Flash(`const`)而非占 RAM？

### 4. 实时性
- 是否在任务/ISR 里忙等(`while(!flag);`)而非让出 CPU？
- 周期任务是否用 `vTaskDelayUntil` 保证周期，而非 `vTaskDelay` 漂移？
- 临界区/关中断时间是否过长，影响其它中断响应？
- 看门狗喂狗位置是否合理（不会因某分支卡死漏喂，也不会无脑喂掩盖死锁）？

### 5. 分层与架构（本工程特有）
- 调用链是否守 `app → service → bsp → HAL`？有无跨层（app/service 直接 `HAL_xxx`、
  bsp 之外碰寄存器）？
- 外设访问是否经 bsp 暴露的接口；句柄是否透传而非全局可变状态？
- HAL 返回值是否转成本工程语义错误码；日志是否走 `LOG` 串口宏（**绝不在 ISR 里打**，
  串口阻塞会拖死中断）？
- 任务是否静态创建、优先级/栈深与 spec 一致？

### 6. 整洁度（最低，可选）
- 魔数是否提为宏/枚举（寄存器位、量程、超时）？
- 重复逻辑可否抽到 bsp helper？死代码、误导注释、`#if 0` 残留？

## 验证

尽量跑编译，把警告也当线索：
```bash
scripts/build.bat        # 读 build_log.txt：0 Error(s)？warning 往往指向真 bug
```
编译失败或有可疑 warning 优先报告。条件允许时提示用户用 `/flash-debug` 上板 + 串口
复现。

## 输出格式

按严重程度分组，每条给 `文件:行号`、问题、为什么是问题、建议改法：

```
## 🔴 必须修复（bug / 会死机 / 数据错）
- bsp/adc.c:42 — DMA 目标缓冲 g_buf 定义在 CCM 段，DMA 无法访问，采样恒为 0。
  建议：移出 CCM（默认 RAM 段）并 __attribute__((aligned(4))) + volatile。
- app/sensor_task.c:88 — 中断里调 xQueueSend（非 FromISR 版），会破坏调度。
  建议：改 xQueueSendFromISR 并 portYIELD_FROM_ISR(woken)。

## 🟡 建议修复（约定 / 健壮性 / 实时性）
- service/foo_svc.c:31 — HAL_ADC_Start 返回值未判，硬件异常时静默出错。

## 🟢 可选优化（整洁度）
- ...

## ✅ 没问题的地方
（简要肯定，避免只挑刺）
```

改动很小且没问题就直说"已检查 X 维度，未发现问题"，不硬凑。

## 原则
- **不臆测**：拿不准标"需上板确认"，不当确凿 bug。嵌入式很多问题要实测复现。
- **可操作**：每条给具体改法（哪个段、哪个 API、哪个优先级）。
- **抓重点**：宁报 3 个真会死机/数据错的问题，不堆 20 条风格挑剔。

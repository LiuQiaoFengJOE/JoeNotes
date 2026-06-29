---
name: iic-driver
description: 基于本工程的软件 I2C 底层(t_i2c_soft)，按 reg/drive/app 三层为某个 I2C 器件生成一套分层驱动 + 使用手册。当用户说"完成 xxx 的驱动""写个 xxx 的 I2C 驱动""加个 I2C 器件 xxx""根据 doc 里的手册做 xxx 驱动"时触发。
---

为某个 **I2C 器件** 生成一套**强分层**的驱动代码，底层复用本工程已有的软件 I2C
(`t_i2c_soft`)。这是 `/new-driver` 的 I2C 器件专用特化版：`/new-driver` 面向任意外设
(bsp→service→app)，本 skill 面向"挂在软件 I2C 总线上的具体芯片"，产出按 **reg / drive /
app** 三层切分的 5 个文件 + 1 份使用手册。

> 触发后**第一件事**：若用户只说了器件名而没给引脚/地址，**必须先用 AskUserQuestion 问全
> 契约**（见步骤 1），再动手。用户的一句话"完成 xxx 的驱动"就是入口。

## 底层约定（不要重复造轮子）

软件 I2C 句柄与原语已在仓库里，**直接调用，不要重写时序**：

- 总线对象：`t_i2c_t *` —— 由 `t_i2c_create(arg, cfg_out, cfg_in, delay, w, r, &gpio)` 创建
  （`Middlewares/Third_Party/p_tlib/t_i2c_soft/t_i2c_soft.h`）。
- 原语：`t_i2c_start / t_i2c_stop / t_i2c_send_byte / t_i2c_wait_ack(0=ACK) /
  t_i2c_read_byte / t_i2c_ack / t_i2c_nack`。
- GPIO 回调与微秒延时已在 `Core/t_i2c_soft_if/t_i2c_soft_if.c` 实现并对外可见：
  `gpio_cgf_out / gpio_cgf_in / gpio_w / gpio_r / iic_delay`（app 层 `extern` 引用即可，
  不要再写一份）。
- 引脚描述结构 `t_i2c_gpio_t { pin_scl, pin_sda, scl_port, sda_port }`，每条总线一个实例。

参考现成实现：`Core/i2c_app/t_sy70300b.c`（控制 GPIO/多模块）、`t_ina228.c`（16-bit 寄存器）。
**注意**：这两个旧文件把 reg+drive+app 揉在一对 .c/.h 里——那正是本 skill 要改进的反面教材，
新器件一律按下面的三层拆分写。

## 步骤

### 1. 确认契约（信息不足时逐项用 AskUserQuestion 问）

- **器件型号 + 数据手册**：在 `./doc/` 找对应 PDF/TXT（如 `doc/SY70300B.txt`）。找不到就问路径。
- **7-bit 从机地址**（及是否可由寄存器改写）。
- **寄存器宽度**：寄存器地址 8-bit？数据 8-bit 还是 16-bit（大端/小端）？读是否用"重复起始"。
- **模块数量 / 总线数**：几个同型号器件？各挂在哪条软件 I2C 总线（SCL/SDA 引脚）。
- **每个模块的 GPIO**（这是用户最该提供的）：
  - 总线：`SCL` / `SDA` 的 port+pin
  - 控制（可选，没有就传 NULL）：`EN`(使能,极性) / `RST`(复位,极性) / `INT`(中断输入)
- **FreeRTOS 装配**：任务名、优先级、栈深、调度周期；上电是否自动初始化(`xxx_init_if`)。
- **要暴露哪些业务 API**：探测在线、读 ID、读/写某量（带单位换算）、故障读取等。

把引脚信息整理成一张表回显给用户确认，再生成代码。新增引脚前对照 `docs/specs/` 资源表防冲突。

### 2. 读手册抽取寄存器与帧格式

读 `./doc` 里的手册（优先 `.txt`，PDF 用 Read 的 pages 分段），抽出：寄存器地址映射、关键位定义、
I2C 读写帧格式（写指针→重复起始→读）、上电/复位时序、量纲换算公式。**把帧格式写进 .c 文件头注释。**

### 3. 生成 5 个文件（命名 + 分层硬性规定）

`XXX` = 器件名（小写如 `sy70300b`）。全部放 `Core/i2c_app/`。

```
t_XXX_reg.h     纯寄存器/位/常量宏定义，无函数、无依赖(除 stdint)
t_XXX_drive.h   句柄 struct + 错误码枚举 + 函数声明
t_XXX_drive.c   XXX_write / XXX_read (单寄存器读写, 便于移植) + 业务函数; 只依赖 t_i2c_soft
t_XXX_app.h     app 装配接口声明 (XXX_init_if / XXX_task_init)
t_XXX_app.c     GPIO 管脚宏定义 + t_i2c_create 实例化 + FreeRTOS 任务 (用户层)
```

**分层铁律**：
- `reg.h` 不含任何函数，只有 `#define`/`enum`，可被 drive 和测试单独包含。
- `drive.c` **不出现任何具体引脚号/GPIOx**，不调 `t_i2c_create`，不建任务——只认 `t_i2c_t *bus`
  和 7-bit 地址（句柄透传）。这样换板子只改 app 层。
- `app.c` 是**唯一**写死引脚宏、调 `t_i2c_create`、`extern gpio_*`、建 FreeRTOS 任务的地方。
- 每个 drive 必须提供 `XXX_write_reg` / `XXX_read_reg`（单寄存器读写），其余业务函数都走它俩——
  用户移植到别的 MCU/硬件 I2C 时只需替换这两个函数。

### 4. 编码规范

- **Doxygen (C 版)**：每个文件头 `@file/@brief/@details`；每个公开函数 `@brief/@param/@return`。
  错误码用枚举，函数返回 `0=成功 非0=失败` 或语义枚举（与现有风格一致，二选一并贯穿）。
- **所有 .h 必带 C++ 块**，固定骨架（注意：纠正任务里的拼写 `DRVIE`→`DRIVE`）：
  ```c
  #ifndef __XXX_DRIVE_H__
  #define __XXX_DRIVE_H__
  #include <stddef.h>
  #include "stdint.h"
  #ifdef __cplusplus
  extern "C" {
  #endif

  /* ... */

  #ifdef __cplusplus
  }
  #endif
  #endif /* __XXX_DRIVE_H__ */
  ```
- **句柄透传，无全局可变状态**：器件状态放 `XXX_t` 句柄，drive 层函数首参恒为 `XXX_t *dev`。
- **遵循 CLAUDE.md 全局约定**：无动态内存（任务静态/`xTaskCreate` 按现状）、HAL 返回值必判、
  ISR 不打印/不阻塞、串口 `printf`/`LOG` 只在任务里打。
- **配置顺序坑**：写多寄存器再统一使能的器件，先全部设值再 enable（见 [[sy70300b-set-then-enable]]）。

### 5. drive.c 单寄存器读写参考骨架

```c
/** @brief 写单个 8-bit 寄存器。 @return 0 成功, 非0 失败(NACK/参数错) */
uint8_t XXX_write_reg(XXX_t *dev, uint8_t reg, uint8_t val)
{
    if (dev == NULL || dev->bus == NULL) return 1;
    t_i2c_start(dev->bus);
    t_i2c_send_byte(dev->bus, (dev->addr << 1) | 0x00);
    if (t_i2c_wait_ack(dev->bus)) { t_i2c_stop(dev->bus); return 1; }
    t_i2c_send_byte(dev->bus, reg);
    if (t_i2c_wait_ack(dev->bus)) { t_i2c_stop(dev->bus); return 1; }
    t_i2c_send_byte(dev->bus, val);
    if (t_i2c_wait_ack(dev->bus)) { t_i2c_stop(dev->bus); return 1; }
    t_i2c_stop(dev->bus);
    return 0;
}

/** @brief 读单个 8-bit 寄存器(重复起始)。 @return 0 成功 */
uint8_t XXX_read_reg(XXX_t *dev, uint8_t reg, uint8_t *val)
{
    if (dev == NULL || dev->bus == NULL || val == NULL) return 1;
    t_i2c_start(dev->bus);
    t_i2c_send_byte(dev->bus, (dev->addr << 1) | 0x00);
    if (t_i2c_wait_ack(dev->bus)) { t_i2c_stop(dev->bus); return 1; }
    t_i2c_send_byte(dev->bus, reg);
    if (t_i2c_wait_ack(dev->bus)) { t_i2c_stop(dev->bus); return 1; }
    t_i2c_start(dev->bus);                                /* 重复起始 */
    t_i2c_send_byte(dev->bus, (dev->addr << 1) | 0x01);
    if (t_i2c_wait_ack(dev->bus)) { t_i2c_stop(dev->bus); return 1; }
    *val = t_i2c_read_byte(dev->bus);
    t_i2c_nack(dev->bus);                                 /* 末字节 NACK */
    t_i2c_stop(dev->bus);
    return 0;
}
```
16-bit 寄存器（如 INA228）：读两字节，第一字节回 `t_i2c_ack`、末字节 `t_i2c_nack`，按手册大小端拼装。

### 6. app.c 装配参考骨架

```c
#include "t_XXX_app.h"
#include "stm32f4xx_hal.h"

/* —— 唯一写死引脚的地方 —— */
#define XXX1_SCL_PORT GPIOB
#define XXX1_SCL_PIN  GPIO_PIN_8
#define XXX1_SDA_PORT GPIOB
#define XXX1_SDA_PIN  GPIO_PIN_9
/* EN/RST/INT 视器件而定 */

extern void gpio_cgf_in(void *port, uint16_t pin);
extern void gpio_cgf_out(void *port, uint16_t pin);
extern void gpio_w(void *port, uint16_t pin, uint8_t v);
extern uint8_t gpio_r(void *port, uint16_t pin);
extern void iic_delay(uint32_t us);

static const t_i2c_gpio_t s_XXX1_hw = {
    .pin_scl = XXX1_SCL_PIN, .scl_port = XXX1_SCL_PORT,
    .pin_sda = XXX1_SDA_PIN, .sda_port = XXX1_SDA_PORT,
};
static t_i2c_t *s_bus1;
static XXX_t    s_dev1;

void XXX_init_if(void)
{
    s_bus1 = t_i2c_create(NULL, gpio_cgf_out, gpio_cgf_in, iic_delay,
                          gpio_w, gpio_r, &s_XXX1_hw);
    XXX_init(&s_dev1, s_bus1, XXX_ADDR_DEFAULT);
    /* 先设值, 后统一使能 */
}

static void XXX_task(void *p) { (void)p; XXX_init_if(); for (;;) { /* ... */ vTaskDelay(1000); } }
void XXX_task_init(void) { xTaskCreate(XXX_task, "XXX", 384, NULL, 1, NULL); }
```

### 7. 编译自检

把 5 个文件加入 Keil 工程后跑 `scripts/build.bat`，读 `build_log.txt` 确认 `0 Error(s)`，
看 size 是否还放得下（可调 `/flash-debug` 上板，`/fw-review` 复审）。

### 8. 生成使用手册（硬性要求：每个驱动都要有）

写到 `docs/manuals/t_XXX_manual.md`，至少含：
- 器件简介、数据手册出处、7-bit 地址、寄存器宽度/帧格式。
- **接线表**：每个模块的 SCL/SDA/EN/RST/INT → MCU 引脚。
- **快速上手**：调 `XXX_init_if()` / 建任务的最小例子。
- **API 速查**：`XXX_write_reg/XXX_read_reg` + 各业务函数（签名、参数、返回、单位）。
- **移植说明**：换 MCU/硬件 I2C 时只替换 `XXX_write_reg/XXX_read_reg`。
- **常见问题**：无 ACK 排查（上拉、地址、供电）、配置顺序坑等。

## 完成清单

- [ ] 契约问全并回显引脚表确认；数据手册已读、帧格式写进 .c 头注释
- [ ] 5 文件命名/分层正确：reg.h 无函数、drive 无引脚号、app 唯一写死引脚
- [ ] 提供 `XXX_write_reg/XXX_read_reg`，业务函数全走它俩；句柄透传无全局可变态
- [ ] 全文件 Doxygen 注释；所有 .h 带 C++ 块与 `__XXX_*_H__` 卫哨
- [ ] `build.bat` → `0 Error(s)`，size 不溢出
- [ ] `docs/manuals/t_XXX_manual.md` 使用手册已生成

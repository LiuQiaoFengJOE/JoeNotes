---
name: new-driver
description: 按 bsp(HAL封装) -> service(业务) -> app(FreeRTOS任务) 分层实现一个新外设驱动/功能（端到端竖切）。当用户要新增外设驱动、写某传感器/通信模块、实现 spec 里的功能、或说"加个 xxx 驱动/功能"时触发。
---

按本工程分层架构实现一个新的外设驱动或功能。调用方向：

```
app (FreeRTOS任务/调度)  ->  service (业务逻辑)  ->  bsp (HAL封装/硬件访问)
        app/                     service/                  bsp/
                                                     ↓ 仅 bsp 调用 ST HAL
                                                  Drivers/ (HAL/CMSIS)
```

> **底层约定**：bsp 层用 **ST HAL 库**，配置由 **CubeMX** 生成。引脚/外设/DMA/NVIC
> 的启用先在 `.ioc` 里配好并重生成，再写 bsp 封装。**业务代码(app/service)绝不直接
> 调 `HAL_xxx`**，一律通过 bsp 暴露的接口——这样 service 能在主机端注入 mock 单测。

## 输入

一句话需求，或一份 `docs/specs/{功能名}.md`。若有 spec，先读它确定引脚、外设资源、
接口、异常、实时性约束。

## 步骤

### 1. 确认契约（信息不足时用 AskUserQuestion）

- 外设类型与实例（UART2 / SPI1 / ADC1 / TIM3…）、引脚与 AF
- bsp 要暴露哪些接口（init / 读 / 写 / 注册回调 / 反初始化）
- 阻塞 vs 中断 vs DMA 方式
- 归属哪个 FreeRTOS 任务、优先级、栈深、采样/调度周期
- 资源是否与现有占用冲突（查 spec 资源表）

### 2. 调研现有模式（用 Explore subagent）

看一遍 `bsp/` 下最相近的现有驱动（如 `bsp/uart.c`）、`app/` 的任务创建写法、
串口日志宏 `LOG` 的用法，保持接口风格与命名一致，避免重复造轮子。

### 3. CubeMX 先行

在 `.ioc` 里启用外设、配引脚 AF、DMA stream/channel、NVIC 优先级
（凡用 FreeRTOS API 的中断，抢占优先级数值 ≥ `configMAX_SYSCALL_INTERRUPT_PRIORITY`），
重生成代码。**只动 USER CODE 区之外由 CubeMX 管的部分；自己的逻辑写进 bsp/。**

### 4. 自底向上实现三层

按 **bsp → service → app** 顺序写，每层写完即可独立编译 review。
下例以外设 `Foo`（如 `Adc`）为例，`foo` 为小写。

#### 4.1 板级驱动 —— `bsp/foo.c` + `bsp/foo.h`

```c
/* bsp/foo.h —— 对外只暴露接口与句柄，隐藏 HAL 细节，便于 service 注入 mock。*/
#ifndef BSP_FOO_H
#define BSP_FOO_H
#include <stdint.h>

typedef enum { FOO_OK = 0, FOO_ERR_HW, FOO_ERR_TIMEOUT, FOO_ERR_PARAM } foo_status_t;

/* Foo 句柄：状态随实例走，不用全局可变量。*/
typedef struct {
    void   *hw;            /* 指向 HAL handle，如 &hadc1，对上层不透明 */
    volatile uint16_t last;/* 与 ISR 共享 → volatile */
} foo_t;

foo_status_t foo_init(foo_t *f);
foo_status_t foo_read(foo_t *f, uint16_t *out, uint32_t timeout_ms);
#endif
```

```c
/* bsp/foo.c —— 唯一允许 #include HAL 并调 HAL_xxx 的地方。*/
#include "bsp/foo.h"
#include "main.h"            /* CubeMX 生成的 hadc1 等 extern 句柄 */

foo_status_t foo_init(foo_t *f) {
    if (f == NULL) return FOO_ERR_PARAM;
    f->hw   = &hadc1;        /* 绑定 CubeMX 句柄 */
    f->last = 0;
    return FOO_OK;
}

foo_status_t foo_read(foo_t *f, uint16_t *out, uint32_t timeout_ms) {
    if (f == NULL || out == NULL) return FOO_ERR_PARAM;
    if (HAL_ADC_Start((ADC_HandleTypeDef *)f->hw) != HAL_OK) return FOO_ERR_HW;
    if (HAL_ADC_PollForConversion((ADC_HandleTypeDef *)f->hw, timeout_ms) != HAL_OK)
        return FOO_ERR_TIMEOUT;
    *out = (uint16_t)HAL_ADC_GetValue((ADC_HandleTypeDef *)f->hw);
    f->last = *out;
    return FOO_OK;
}
```

要点：
- 句柄入参，无全局可变状态；与 ISR 共享的成员 `volatile`。
- HAL 返回值必判，转成本层 `foo_status_t` 语义码。
- 只有 bsp 碰 HAL；中断回调 (`HAL_ADC_ConvCpltCallback`) 也放本文件，里面**只**
  置标志/发队列，不做阻塞、不打日志。

#### 4.2 业务逻辑 —— `service/foo_svc.c`（可主机端单测）

```c
/* service 编排业务、做校验/换算/异常转译，依赖 bsp 接口而非 HAL。
   单测时把 foo_t 的操作替换为 mock 即可在 PC 上跑。*/
#include "bsp/foo.h"
#include "service/foo_svc.h"

#define FOO_VREF_MV   3300u
#define FOO_FULL      4095u    /* 12-bit */

svc_status_t foo_svc_read_mv(foo_t *f, uint16_t *mv) {
    uint16_t raw;
    foo_status_t st = foo_read(f, &raw, 50 /*ms*/);
    if (st == FOO_ERR_TIMEOUT) return SVC_ERR_TIMEOUT;
    if (st != FOO_OK)          return SVC_ERR_HW;
    *mv = (uint16_t)((uint32_t)raw * FOO_VREF_MV / FOO_FULL);  /* 换算放这层 */
    return SVC_OK;
}
```

要点：换算、阈值、状态机、异常转译都在 service；不碰 HAL、不碰 FreeRTOS API 细节
（任务相关的留给 app），保持可在 PC 上注入 mock 测试。

#### 4.3 任务装配 —— `app/foo_task.c` + 在 `app/main.c` 建任务

```c
/* app 负责 FreeRTOS 任务、周期调度、把数据给到其它模块（队列）。*/
#include "FreeRTOS.h"
#include "task.h"
#include "service/foo_svc.h"
#include "bsp/log.h"            /* LOG() 串口日志宏 */

static foo_t s_foo;           /* 任务私有，static 限定作用域 */

void foo_task(void *arg) {
    (void)arg;
    foo_init(&s_foo);
    const TickType_t period = pdMS_TO_TICKS(100);
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        uint16_t mv;
        if (foo_svc_read_mv(&s_foo, &mv) == SVC_OK) {
            LOG("foo: %u mV\r\n", mv);
            /* xQueueSend(...) 把结果交给消费者 */
        } else {
            LOG("foo read failed\r\n");
        }
        vTaskDelayUntil(&last, period);   /* 周期精确，优于 vTaskDelay */
    }
}
```

在 `main.c` 用 **静态创建**（无动态内存约定）注册任务：
```c
static StaticTask_t s_foo_tcb;
static StackType_t  s_foo_stack[256];   /* 栈深按 spec 估算，留余量 */
xTaskCreateStatic(foo_task, "foo", 256, NULL, 3 /*优先级*/, s_foo_stack, &s_foo_tcb);
```

要点：
- 任务私有数据 `static`；周期调度用 `vTaskDelayUntil`。
- 优先级/栈深来自 spec；与 ISR/其它任务通信走队列/信号量，不用全局裸变量。
- 凡 ISR 内通知任务，用 `xQueueSendFromISR` 等 `FromISR` 版本。

### 5. 测试

- **service 层（主机端）**：用 CMocka/Unity 在 PC 编译，给 `foo_read` 打桩返回
  构造值，断言 `foo_svc_read_mv` 的换算与异常转译。
- **bsp/app（在板）**：`/flash-debug` 烧录后用串口看 `LOG` 日志、用 J-Link 单步验证时序、
  逻辑分析仪/示波器核对波形与采样周期。

### 6. 编译 + 自检清单

跑 `scripts/build.bat`，读 `build_log.txt` 确认 `0 Error(s)`，看 size 是否还放得下。

- [ ] 调用链严格 app→service→bsp，**只有 bsp 碰 HAL**，app/service 无 `HAL_xxx`
- [ ] 句柄/参数透传，无全局可变状态；ISR 共享变量 `volatile`
- [ ] ISR 内不阻塞/不打印，用 `...FromISR` 通知任务并处理唤醒标志
- [ ] DMA 缓冲非 CCM、对齐、`volatile`；中断优先级符合 FreeRTOS 约束
- [ ] HAL 返回值全判，转成语义错误码 + `LOG`
- [ ] 任务静态创建，栈深/优先级合 spec；`build.bat` 0 错、size 不溢出

完成后用 `/flash-debug` 上板验证，再用 `/fw-review` 做一遍审核。

# 严格功能闭环 Testcase 要求

## 目的

本文档用于约束 DMX/TSI/TSG 项目的 testcase 编写、评审和维护方式。核心原则是：

- 先证明功能，再谈 coverage
- 先定义 FAIL 条件，再谈 PASS 条件
- 先看外部可观测证据，再决定是否需要内部辅助定位

可复用的方法论和脚本清单见 `doc/TB通用skill与Python脚本总结.md`。

## 核心原则

1. testcase 必须以功能闭环为目标，不能以“补 coverage”本身作为目标。
2. testcase 只有同时满足以下两点才能判为闭环：
   - 目标功能被真实触发
   - 检查逻辑能把错误实现抓出来
3. 不能使用 tolerated pass。
   - 期望行为没观察到，就必须 FAIL
   - 或明确标记为“未闭环 testcase”，不能混入有效回归
4. 协议相关功能优先使用 bit 级、byte 级或 cycle 级证据。
   - 不能只靠中断拉高、done 拉高、寄存器能读回来这种弱证据
5. testcase 名称、注释、刺激、检查四者必须一致。
   - 名称写的是 threshold，就要证明 threshold 被触发并被正确清除
   - 名称写的是 underrun，就要证明 underrun 事件、pending、恢复链路都成立

## 本项目硬性规则

### 1. 场景设计

- 优先做黑盒功能场景，先从外部端口、寄存器、内存写入结果证明行为。
- coverage 只能作为功能 testcase 的副产物，不能反过来为 coverage 生造刺激。
- bist/test_mode 不是当前功能验证范围。
  - 功能 testcase 中必须保持 `test_mode=0`
  - 功能 testcase 中必须保持 `test_se=0`
  - 功能 testcase 中必须保持 `dmx_sram_bist_start=0`
  - 功能 testcase 中必须保持 `dmx_sram_bist_mode=0`

### 2. PASS/FAIL 规则

- PASS 必须由明确的功能证据支持，不能由“没报错”替代。
- FAIL 条件必须在 testcase 里显式写出来，至少包含：
  - 功能未触发
  - 关键观测点未达到
  - 清除/恢复行为不符合预期
  - 输出数据与输入参考不一致
- 对 HMB 写数据信号的比对必须采用协议语义重建后的文本结果，不允许只看 transaction 次数。
- packet compare 必须是严格模式：
  - packet 总数不一致，FAIL
  - 尾包未 flush，FAIL
  - packet 内存在 unknown/mismatch，FAIL
  - 写冲突或地址重叠覆盖，FAIL

### 3. TS 到 HMB 的数据闭环

- 需要验证 TS 流路径时，必须把 HMB 接口实际写入的数据按 HMB 协议重建成 packet 文本。
- packet 文本必须能和参考 TS packet 一一对应。
- `expected` golden 文件统一放在 `tb/expected` 目录。
- 仿真产生的 `actual`/`reference` 文本统一落在 run 目录，作为本次 testcase 的直接证据。
- 必须同时保存：
  - HMB raw trace
  - 实际重建 packet 文本
  - 期望 packet 文本
- 对有限长度 TS 流，必须显式考虑尾包 flush 策略。
  - 不能默认“流结束后最后一个 packet 一定会自然刷出”
  - 如果 DUT 需要额外边界条件才能吐出最后一个 packet，testcase 必须明确建模并说明原因
  - 不能把“尾包没出来”当作可接受现象

### 4. 寄存器访问写法

- testcase 中所有 base address 和 register offset 都必须使用宏定义。
- 禁止在 testcase 中直接写立即数地址或 offset。
- 每一条寄存器读写语句都必须写注释，说明该寄存器对应的功能。
- 注释要解释“为什么写/读这个寄存器”，而不是只重复地址本身。

### 5. task 命名规则

- 所有 task 都必须带 IP 前缀。
- 通用 task 使用 `dmx_` 前缀。
- 如果 task 已经对应明确 IP，则把 IP 名字放在最前面：
  - `dmx1_*`
  - `dmx2_*`
  - `tsi_*`
  - `tsg_*`
- 不允许保留无前缀的通用名字，如 `run_test`、`flag_error`、`write_sel`。

### 6. 复位与稳定性

- testcase 必须从明确、可重复的功能复位状态开始。
- 复位后要检查 DFT/BIST 控制保持失活。
- testcase 不能依赖脏状态、上一个 case 残留数据或未定义初值。

### 7. TB 内部判定与脚本边界

- testcase 的最终 PASS/FAIL 必须由 TB 内部 task 收口，不能依赖 Makefile 后处理决定结果。
- 如果需要借助外部脚本做文本比较，允许在 TB task 中通过 `$system()` 或等价方式调用。
- 外部脚本的职责仅限于：
  - 协议语义重建
  - 文本生成
  - 差异检查
- 外部脚本返回非零状态时，TB 必须立即转成 FAIL。
- Makefile 可以保留手工调试入口，但这些入口不能作为正式回归 PASS/FAIL 的唯一来源。
- golden 更新必须是显式动作，不能在正式回归中自动覆盖 `tb/expected` 下的参考文件。

### 8. Makefile 与 testcase 生成规则

- Makefile 只能负责组织编译、运行、compare、coverage 和波形准备，不能替代 testcase 本身的功能判定。
- 工具命令禁止写死绝对路径。
  - `python`、`vcs`、`urg`、`verdi` 等必须通过变量或环境 `PATH` 解析
  - 目录迁移后不应要求逐台机器手工改工具路径
- 工程路径必须从 Makefile 所在目录相对推导，禁止依赖工程绝对根目录。
- filelist 中的 RTL、TB 路径也必须使用相对路径，保证目录搬迁后仍可直接使用。
- testcase 必须由统一脚本生成，不能长期依赖手工维护大量输入向量文本。
- testcase 生成脚本必须支持：
  - 生成基础回归
  - 生成聚合回归
  - 按 testcase 名筛选生成
  - 按真实数据集或外部样本裁剪生成
- testcase 输入数据来源允许分为两类：
  - 规则构造或参考模型生成的数据
  - manifest 管理的真实数据集或外部样本
- 真实数据集列表必须由 manifest 或统一索引管理，不能在多个脚本里零散硬编码文件名。
- testcase 生成脚本中的路径解析必须兼容目录搬迁，不能把旧目录层级写死。
- testcase 组织应固定化，至少保证：
  - 输入向量文件位置清晰
  - golden 输出文件位置清晰
  - testcase 清单可枚举
  - 聚合回归入口可复用
- testcase 执行必须走“仿真运行 + 输出比对”两步，不能只看 sim 退出码或日志无报错。
- 每个 testcase 的 run 目录都必须保留本次直接证据，至少包括：
  - 仿真日志
  - DUT 实际输出
  - compare 依赖的 expected/actual 关联关系
- testcase 的输出文件格式如果发生变化，生成脚本、TB 解析逻辑、compare 脚本必须同步更新。
- 每次较大 RTL 或模型改动后，必须先跑最小必要范围验证，再扩到全回归。

### 9. Coverage 统计口径

- coverage 统计口径只看 DUT RTL，不包含 testbench、wrapper、checker、vector loader 或其他验证侧代码。
- coverage closure 必须基于 RTL 模块定义和 RTL 实例层级，不允许把 TB coverage 混入正式 closure 指标。
- 如果工具默认把 TB 纳入统计，必须通过 filelist 过滤、层级过滤或报表读取规则把 TB coverage 排除。
- 对外汇报 coverage 时，默认报告 `dut` 及其 RTL 子模块，不报告 testbench 模块分数。

## 推荐工作流

1. 先审计目标功能。
   - 功能目标
   - 触发条件
   - 关键窗口
   - 外部证据
   - FAIL 条件
2. 再定义刺激。
   - 为什么这个刺激能命中目标
   - 是否会误命中别的路径
3. 再定义检查。
   - 强检查优先于弱检查
   - 数据结果优先于计数结果
4. 再补工程规范。
   - 宏定义
   - 注释
   - task 命名
   - TB 内部 compare 收口
   - 日志与文本产物
5. 最后跑回归并按 testcase 结论收口。

## 审计清单

- testcase 是否真实触发了目标功能
- testcase 是否能区分正确实现和错误实现
- 是否存在 coverage-only 刺激
- 是否存在 tolerated pass
- 是否遗漏边界条件或尾部 flush 条件
- 是否把 model/testbench 的宽松行为误当成 DUT 正确
- 是否使用了立即数地址
- 是否遗漏寄存器访问注释
- 是否违反 task 前缀命名规则
- 是否把 PASS/FAIL 错误地下放给 Makefile 后处理
- `expected`/`actual`/`reference` 文本目录是否清晰且可追溯

## 完整提示词模板

```text
本任务必须以严格功能闭环为驱动，而不是以覆盖率为驱动。

要求：
1. 先阅读 RTL、testbench、model 和现有 testcase。编写代码之前，先输出功能审计表。每个功能条目必须包含：
   - 功能目标
   - 触发条件
   - 关键时序窗口或关键比特窗口
   - 外部可观测证据
   - 明确的失败条件
2. testcase 必须优先做黑盒功能验证，coverage 只能作为副产物。
3. 功能 testcase 中必须保持 test_mode/test_se/dmx_sram_bist_* 为失活。
4. 寄存器访问必须全部使用宏定义，且每条读写要写功能注释。
5. task 名称必须带 IP 前缀，通用 task 用 dmx_，明确 IP task 用 dmx1/dmx2/tsi/tsg 前缀。
6. TS/HMB 场景必须保存 HMB raw trace、actual packet 文本、expected packet 文本，并做严格比对。
7. packet 总数不一致、尾包未 flush、unknown、mismatch、写冲突都必须 FAIL。
8. `expected` golden 文件统一放在 tb/expected，actual/reference 文本统一落在 run 目录。
9. testcase 的最终 PASS/FAIL 必须在 TB 内部 task 中收口；如果调用外部脚本比较，脚本非零返回必须立刻转成 FAIL。
10. 如果有限流场景存在尾包 flush 依赖，必须显式建模并写清楚原因，不能默认容忍尾包丢失。
11. 如果现有 testcase 不是功能闭环，先指出并给出修复计划，未确认前不得计入有效回归。
12. 最终必须报告每个 testcase：
   - 验证了什么
   - 为什么算闭环
   - 关键观测点
   - 剩余风险

规则：
- 检查强度不足，就不能判 PASS。
- testbench 或 model 只能帮助观察，不能掩盖 DUT 问题。
- Makefile 只能负责组织编译/运行，不能单独决定 testcase 结论。
```

## 简短提示词

```text
本任务必须先按功能闭环，不按覆盖率闭环。先审计 testcase，再决定修改。禁止 coverage-only testcase，禁止 tolerated pass。功能 testcase 保持 BIST/DFT 失活。寄存器地址全部用宏，寄存器读写必须写功能注释，task 必须带 IP 前缀。TS/HMB 场景必须做严格 packet compare，packet 总数和尾包 flush 都纳入 FAIL；expected 放 tb/expected，actual/reference 放 run，最终 PASS/FAIL 由 TB task 收口，不交给 Makefile 后处理。
```

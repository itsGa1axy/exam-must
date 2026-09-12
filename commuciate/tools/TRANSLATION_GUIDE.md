# STM32 标准外设库注释汉化规范

## 输入 / 输出

- **输入**：`tools/_unique.txt` 中你负责的行区间。里面是去重后的英文注释原文，
  每条以 `<<<COMMENT n>>>` 单独一行开头，其后是该注释的完整原文。
- **输出**：写一个新文件到 `tools/zh/`（文件名见你的任务说明）。
  格式与输入严格一致：`<<<COMMENT n>>>` 单独一行，紧跟中文译文。
  只写你负责区间内的条目，**不要动其它条目**。

**不要运行 apply.py / verify.py / extract.py**，只写你的译文分块文件。

## 硬性规则

1. 译文必须是**完整可直接替换原文的注释块**，包含定界符本身：
   `/**`、`  * `、`  */`、`/*`、`*/`。原文是什么定界符，译文就用什么。
2. 保持原有的 `  * ` 前缀、缩进层级、空行位置。多行注释的每一行都要重新给出。
3. **保留所有 doxygen 标签名不译**：
   `@file @author @version @date @brief @param @retval @arg @note @attention
    @addtogroup @defgroup @code @endcode @{ @} @ref`
   只翻译标签后面的说明文字。
4. **代码标识符一律保留英文**：寄存器名（CR1/CR2/SQR1…）、位名（ADON/CONT/SCAN…）、
   宏名、函数名、类型名、枚举值（`ADC_Channel_0`、`NVIC_PriorityGroup_1`…）、
   文件名。它们是指代码，翻译会破坏可读性。
5. `****` 分隔线、`====` / `----` 表格线、ASCII 图表：**原样保留**，
   在图表前用中文说明其含义即可（中文是双宽字符，重排表格必然错位）。
6. **禁止**在译文里出现 `*/` 后紧跟 `/*` 的写法。译文中的 `*/` 只能作为注释结尾出现一次。
   违反此条会改变 C 注释的 token 结构，导致回写时破坏源码。
7. `@defgroup X` / `@addtogroup X` 这类只有分组标识符、没有说明文字的注释：
   在标识符后追加两个空格再加中文说明。例如
   `/** @defgroup ADC_Private_Defines` → `/** @defgroup ADC_Private_Defines   ADC 私有宏定义`
8. `@attention` 下的法律免责段落：译成中文，但
   `COPYRIGHT 2011 STMicroelectronics` 这一段保持英文原样。
9. **纯代码 / 纯标识符注释保留原样不译**，例如 `/* __MISC_H */`、
   `/* USE_FULL_ASSERT */`、`#endif` 后的 `/* FOO */` 标记注释。
   它们不含自然语言。但被注释掉的**代码**若原文有说明性文字，则翻译说明文字。

## 术语表（务必统一）

| 英文 | 中文 | 英文 | 中文 |
|---|---|---|---|
| register | 寄存器 | mask | 掩码 |
| bit | 位 | flag | 标志 |
| set / reset | 置位 / 清零 | pending | 挂起 |
| enable / disable | 使能 / 关闭 | configure | 配置 |
| initialize | 初始化 | deinitialize | 反初始化 |
| reset state | 复位状态 | release from reset | 从复位状态释放 |
| interrupt | 中断 | handler | 处理函数 |
| channel | 通道 | conversion | 转换 |
| regular group | 规则组 | injected group | 注入组 |
| sample time | 采样时间 | sequencer | 序列器 |
| external trigger | 外部触发 | discontinuous mode | 间断模式 |
| analog watchdog | 模拟看门狗 | independent watchdog | 独立看门狗 |
| window watchdog | 窗口看门狗 | calibration | 校准 |
| prescaler | 预分频器 | clock | 时钟 |
| baud rate | 波特率 | parity | 校验位 |
| stop bit | 停止位 | word length | 字长 |
| frame | 帧 | acknowledge | 应答 |
| transmit / receive | 发送 / 接收 | transfer | 传输 |
| peripheral | 外设 | firmware | 固件 |
| driver | 驱动 | library | 库 |
| specifies | 指定 | contains | 包含 |
| This parameter can be | 该参数可取 | where x can be | x 可取 |
| None | 无 | Return | 返回 |
| Gets | 获取 | Returns | 返回 |
| Fills each ... member with its default value | 将 ... 的每个成员填充为默认值 |
| This function handles ... | 本函数处理 ... |

## 语气

技术文档体，简洁准确。用「本函数」「该参数」「此寄存器」，不用「你」「我们」。

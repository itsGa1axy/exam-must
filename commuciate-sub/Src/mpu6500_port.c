/**
  ******************************************************************************
  * @file    mpu6500_port.c
  * @brief   MPU6500 驱动的 STM32F10x 移植层：I2C1 时序与毫秒时基。
  *
  *          本文件是驱动与单片机之间的适配层，实现两类内容：
  *
  *          一、时基
  *            SysTick 每 1 ms 中断一次，在中断中累加全局变量 system_millis。
  *            inv_delay_ms / inv_get_ms 是 InvenSense 官方库要求的回调，
  *            由它们把库内部的延时与时间戳请求转接到本时基上。
  *
  *          二、I2C 主机收发
  *            inv_i2c_write / inv_i2c_read 同样是官方库要求的回调，在本文件中
  *            用 STM32F10x 标准外设库实现 I2C1 的主机收发时序。
  *
  *          硬件连接（STM32F103）：
  *            PB6 —— I2C1_SCL，接 MPU6500 的 SCL
  *            PB7 —— I2C1_SDA，接 MPU6500 的 SDA
  *            两线均为开漏输出，需外接 4.7 kΩ 上拉电阻到 3.3 V。
  *            从机地址 0x68（AD0 接地）或 0x69（AD0 接高电平），
  *            地址由官方库的寄存器表给出，本层只需把 7 位地址左移后发送。
  *
  * @note    本项目在 CMakeLists.txt 中未定义 EMPL_TARGET_STM32F4 等目标宏，
  *          因此 inv_mpu.c 走的是「extern 回调」这条分支，
  *          即由本文件提供 inv_i2c_write / inv_i2c_read / inv_delay_ms / inv_get_ms
  *          四个函数；函数名与参数顺序必须与 inv_mpu.c 中的 extern 声明一致。
  ******************************************************************************
  */

#include "mpu6500_port.h"

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"

/** 本驱动使用的 I2C 外设。改用 I2C2 时只需修改此宏与下方引脚/时钟配置。 */
#define MPU6500_I2C             I2C1

/**
  * @brief  等待事件/标志时的循环次数上限。
  *
  *         本值不是时间单位而是循环计数：STM32F1 的 I2C 外设一旦因总线异常
  *         卡住，寄存器查询会永远等待下去，因此必须设上限以避免死锁。
  *         取值偏大是有意为之——正常时序下第一次循环就会命中，不会浪费执行时间；
  *         只有真出错时才会走满，此时宁可多等一会儿也不要误判为故障。
  */
#define MPU6500_I2C_TIMEOUT     1000000u

/**
  * @brief  全局毫秒时基，上电清零，由 SysTick 中断每毫秒递增。
  *
  *         加 volatile 是因为它在中断中被修改、在主程序中读取，
  *         防止编译器把循环里的读取优化进寄存器而读不到新值。
  */
static volatile uint32_t system_millis;

/**
  * @brief  检查 I2C 总线是否出现错误标志。
  *
  * @param  无
  * @retval  非 0  存在错误标志
  * @retval  0     总线无错误
  *
  * @note   检查的四个标志含义：
  *           AF   —— 应答失败，从机未响应或地址/寄存器不合法；
  *           BERR —— 总线错误，START/STOP 时序被干扰；
  *           ARLO —— 仲裁丢失，总线上出现另一个主机；
  *           OVR  —— 溢出，软件未能及时读写数据寄存器。
  *         这些标志硬件置位后不会自动清除，须由 i2c_recover_transfer 复位。
  */
static int i2c_error_pending(void)
{
    return (I2C_GetFlagStatus(MPU6500_I2C, I2C_FLAG_AF) != RESET) ||
           (I2C_GetFlagStatus(MPU6500_I2C, I2C_FLAG_BERR) != RESET) ||
           (I2C_GetFlagStatus(MPU6500_I2C, I2C_FLAG_ARLO) != RESET) ||
           (I2C_GetFlagStatus(MPU6500_I2C, I2C_FLAG_OVR) != RESET);
}

/**
  * @brief  阻塞等待 I2C 事件发生，带超时与错误检测。
  *
  * @param  event  期望的事件掩码，如 I2C_EVENT_MASTER_MODE_SELECT（即 EV5）。
  * @retval  0  事件已发生
  * @retval -1  超时，或等待期间检测到总线错误
  *
  * @note   I2C_CheckEvent 内部会先读 SR1 再读 SR2。这一点在从机地址应答之后
  *         尤其重要：读取 SR1 再读 SR2 正是硬件要求的「清除 ADDR 标志」动作，
  *         因此本函数在返回成功的同时也完成了 ADDR 的清除，
  *         无需再单独调用 I2C_ClearFlag。
  */
static int wait_event(uint32_t event)
{
    uint32_t timeout = MPU6500_I2C_TIMEOUT;

    while (timeout-- != 0u)
    {
        if (i2c_error_pending())
            return -1;

        if (I2C_CheckEvent(MPU6500_I2C, event) == SUCCESS)
            return 0;
    }

    return -1;
}

/**
  * @brief  阻塞等待某个 I2C 标志变为指定状态，带超时与错误检测。
  *
  * @param  flag   要等待的标志，如 I2C_FLAG_RXNE、I2C_FLAG_BTF。
  * @param  state  期望的状态，SET 或 RESET。
  * @retval  0  标志已到达期望状态
  * @retval -1  超时，或等待期间检测到总线错误
  *
  * @note   与 wait_event 的分工：wait_event 用于等待由多个标志组合而成的
  *         协议事件（EV5/EV6/E…），wait_flag 用于等待单个标志位。
  *         接收流程的最后几个字节必须按标志逐位控制，因此两者都存在。
  */
static int wait_flag(uint32_t flag, FlagStatus state)
{
    uint32_t timeout = MPU6500_I2C_TIMEOUT;

    while (timeout-- != 0u)
    {
        if (i2c_error_pending())
            return -1;

        if (I2C_GetFlagStatus(MPU6500_I2C, flag) == state)
            return 0;
    }

    return -1;
}

/**
  * @brief  把 I2C 外设从异常状态拉回空闲可用状态。
  *
  * @param  无
  * @retval 无
  *
  * @note   出错后必须复位四类状态，否则下一次传输会立即再次失败：
  *           1. 补发 STOP  —— 结束可能仍挂在线上的传输，释放总线；
  *           2. 重新使能应答、把 NACK 位置复原 —— 接收流程中途出错时
  *              可能残留「不应答」「NACK 提前」的设置，会污染后续传输；
  *           3. 清除 AF/BERR/ARLO/OVR —— 这些标志是硬件锁存的，
  *              不清除则 i2c_error_pending 永远报错。
  *
  * @note   本函数只复位 I2C 外设状态，不重置外设本身；
  *         若总线被从机拉死（SDA 一直为低），还需要额外的引脚翻转恢复手段。
  */
static void i2c_recover_transfer(void)
{
    I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
    I2C_AcknowledgeConfig(MPU6500_I2C, ENABLE);
    I2C_NACKPositionConfig(MPU6500_I2C, I2C_NACKPosition_Current);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_AF);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_BERR);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_ARLO);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_OVR);
}

/**
  * @brief  I2C 主机发送：先发送寄存器地址，再发送 length 个字节数据。
  *
  *         本函数是 InvenSense 官方库要求的回调，由 inv_mpu.c 通过宏
  *         i2c_write 调用，函数名与参数顺序不可更改。
  *
  *         总线时序：
  *           START → 从机地址(写) → 寄存器地址 → 数据0 … 数据n → STOP
  *
  * @param  slave_addr  7 位从机地址（不含读写位），函数内部会左移 1 位。
  * @param  reg_addr    要写入的寄存器地址。
  * @param  length      数据字节数，允许为 0（此时只发送寄存器地址）。
  * @param  data        待发送数据缓冲区；length 为 0 时允许为 NULL。
  * @retval  0  发送成功
  * @retval -1  参数非法、总线忙、等待超时或检测到总线错误
  *
  * @note   每一步等待的都是 I2C_EVENT_MASTER_BYTE_TRANSMITTED（即 EV8_2，
  *         由 TXE 与 BTF 同时置位构成）。等待 BTF 而非仅 TXE，是为了确保
  *         数据真正移出移位寄存器，这样下一次写入 DR 不会覆盖未发完的数据。
  */
int inv_i2c_write(uint8_t slave_addr,
                  uint8_t reg_addr,
                  uint8_t length,
                  uint8_t const *data)
{
    uint8_t index;

    /* length 非 0 却没有数据缓冲区，属于调用错误，直接拒绝。 */
    if ((data == 0) && (length != 0u))
        return -1;

    /* 总线忙说明上一次传输尚未结束（或总线被拉死），此时发 START 会被忽略。 */
    if (wait_flag(I2C_FLAG_BUSY, RESET) != 0)
        return -1;

    /* 产生起始条件，随后等待 EV5：主机模式已选中。 */
    I2C_GenerateSTART(MPU6500_I2C, ENABLE);
    if (wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0)
        goto error;

    /* 发送从机地址 + 写方向，随后等待 EV6：发送模式已选中（ADDR 已被清除）。 */
    I2C_Send7bitAddress(MPU6500_I2C, (uint8_t)(slave_addr << 1), I2C_Direction_Transmitter);
    if (wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0)
        goto error;

    /* 发送寄存器地址，MPU6500 的寄存器访问都以此开头（即所谓「写寄存器地址」阶段）。 */
    I2C_SendData(MPU6500_I2C, reg_addr);
    if (wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0)
        goto error;

    /* 逐字节发送数据。 */
    for (index = 0; index < length; ++index)
    {
        I2C_SendData(MPU6500_I2C, data[index]);
        if (wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0)
            goto error;
    }

    /* 发送正常结束，产生停止条件释放总线。 */
    I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
    return 0;

error:
    i2c_recover_transfer();
    return -1;
}

/**
  * @brief  I2C 主机接收：先写入寄存器地址，再重启总线读回 length 个字节。
  *
  *         本函数是 InvenSense 官方库要求的回调，由 inv_mpu.c 通过宏
  *         i2c_read 调用。
  *
  *         总线时序：
  *           START → 从机地址(写) → 寄存器地址 → 重复START → 从机地址(读)
  *                 → 读数据… → NACK → STOP
  *
  *         接收字节数决定了三种不同的收尾方式，这是因为 STM32F1 的 I2C
  *         必须在「最后一个字节到达之前」就决定是否应答、以及 STOP 的发出时机，
  *         一旦读到数据再补救就晚了。三种情况分别处理：
  *
  *           1 字节：进入接收模式前就关闭应答、STOP 提前发出，等 RXNE 后读取；
  *           2 字节：先在最后一个字节到达时不应答（NACK 位置设为 Next），
  *                   等 BTF 置位后关应答、发 STOP，再连续读两个字节；
  *           3 字节及以上：前 N-3 个字节正常读出；剩 3 个字节时关应答，
  *                   按 BTF 逐字节读出，并在倒数第二个字节后发 STOP。
  *
  * @param  slave_addr  7 位从机地址（不含读写位）。
  * @param  reg_addr    要读取的寄存器地址。
  * @param  length      要读取的字节数，必须大于 0。
  * @param  data        接收缓冲区，不可为 NULL。
  * @retval  0  接收成功
  * @retval -1  参数非法、总线忙、等待超时或检测到总线错误
  *
  * @note   从机地址左移 1 位是因为 7 位地址占字节的高 7 位，
  *         最低位是读写方向位，传入的地址不含该位。
  */
int inv_i2c_read(uint8_t slave_addr,
                 uint8_t reg_addr,
                 uint8_t length,
                 uint8_t *data)
{
    uint8_t remaining = length;   /* 剩余待接收字节数，收尾逻辑依赖它判断 */
    uint8_t *cursor = data;       /* 写入游标，随接收推进前移 */

    if ((data == 0) || (length == 0u))
        return -1;

    if (wait_flag(I2C_FLAG_BUSY, RESET) != 0)
        return -1;

    /* 第一阶段：以「写」方向发送寄存器地址，指明接下来要从哪个寄存器读。 */
    I2C_GenerateSTART(MPU6500_I2C, ENABLE);
    if (wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0)
        goto error;

    I2C_Send7bitAddress(MPU6500_I2C, (uint8_t)(slave_addr << 1), I2C_Direction_Transmitter);
    if (wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0)
        goto error;

    I2C_SendData(MPU6500_I2C, reg_addr);
    if (wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0)
        goto error;

    /*
     * 第二阶段：不发 STOP，直接再发一次 START（重复起始条件），
     * 把方向切换为「读」。这样寄存器地址与读操作保持在同一帧事务中，
     * 是 MPU6500 等器件的标准读时序。
     */
    I2C_GenerateSTART(MPU6500_I2C, ENABLE);
    if (wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0)
        goto error;

    /*
     * 在读地址发出之前就要确定收尾参数：
     *   收 1 字节：全程不应答，NACK 位置为 Current；
     *   收 2 字节：最后一个字节不应答（NACK 位置为 Next）；
     *   收 3 字节及以上：正常应答，NACK 位置为 Current，收尾时再临时关闭应答。
     */
    I2C_NACKPositionConfig(MPU6500_I2C,
                           (remaining == 2u) ? I2C_NACKPosition_Next : I2C_NACKPosition_Current);
    I2C_AcknowledgeConfig(MPU6500_I2C, (remaining == 1u) ? DISABLE : ENABLE);
    I2C_Send7bitAddress(MPU6500_I2C, (uint8_t)(slave_addr << 1), I2C_Direction_Receiver);
    if (wait_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != 0)
        goto error;

    if (remaining == 1u)
    {
        /* 单字节：先发 STOP 再等 RXNE，读到数据后总线事务即告完成。 */
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
        if (wait_flag(I2C_FLAG_RXNE, SET) != 0)
            goto error;
        cursor[0] = I2C_ReceiveData(MPU6500_I2C);
    }
    else if (remaining == 2u)
    {
        /*
         * 双字节：等 BTF 置位（说明第一个字节已搬到数据寄存器、第二个字节
         * 也已收完），此时关应答并发 STOP，然后连续读走两个字节。
         */
        if (wait_flag(I2C_FLAG_BTF, SET) != 0)
            goto error;
        I2C_AcknowledgeConfig(MPU6500_I2C, DISABLE);
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
        cursor[0] = I2C_ReceiveData(MPU6500_I2C);
        cursor[1] = I2C_ReceiveData(MPU6500_I2C);
    }
    else
    {
        /* 三字节及以上：先把前面 remaining-3 个字节按 RXNE 依次读走。 */
        while (remaining > 3u)
        {
            if (wait_flag(I2C_FLAG_RXNE, SET) != 0)
                goto error;
            *cursor++ = I2C_ReceiveData(MPU6500_I2C);
            --remaining;
        }

        /*
         * 剩余 3 个字节的特殊收尾，顺序不能调换：
         *   等 BTF → 关应答 → 读倒数第 3 个字节；
         *   等 BTF → 发 STOP → 读倒数第 2 个字节；
         *   直接读最后 1 个字节。
         * STOP 必须在最后一个字节读走之前发出，否则从机可能继续占用总线。
         */
        if (wait_flag(I2C_FLAG_BTF, SET) != 0)
            goto error;
        I2C_AcknowledgeConfig(MPU6500_I2C, DISABLE);
        *cursor++ = I2C_ReceiveData(MPU6500_I2C);
        if (wait_flag(I2C_FLAG_BTF, SET) != 0)
            goto error;
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
        *cursor++ = I2C_ReceiveData(MPU6500_I2C);
        *cursor = I2C_ReceiveData(MPU6500_I2C);
    }

    /* 恢复正常配置，避免影响下一次传输。 */
    I2C_AcknowledgeConfig(MPU6500_I2C, ENABLE);
    I2C_NACKPositionConfig(MPU6500_I2C, I2C_NACKPosition_Current);
    return 0;

error:
    i2c_recover_transfer();
    return -1;
}

/**
  * @brief  毫秒级阻塞延时，InvenSense 官方库要求的回调（对应库中的 delay_ms）。
  *
  * @param  num_ms  延时长度，单位毫秒。
  * @retval 无
  *
  * @note   实现方式是空转等待 SysTick 时基推进，全程占用 CPU，不会进入低功耗。
  *         官方库主要在芯片复位后调用它等待 100 ms 稳定，
  *         属于一次性开销，因此不必改成低功耗延时。
  *
  * @note   判断条件写成 (system_millis - start) < num_ms 的无符号相减形式，
  *         即使 system_millis 在等待期间发生回绕也能得到正确结果。
  */
void inv_delay_ms(uint32_t num_ms)
{
    uint32_t start = system_millis;

    while ((uint32_t)(system_millis - start) < num_ms)
    {
    }
}

/**
  * @brief  读取毫秒时基，InvenSense 官方库要求的回调（对应库中的 get_ms）。
  *
  * @param[out] count  输出毫秒计数值；允许传 NULL，此时只当空操作。
  * @retval 无
  *
  * @note   官方库多处调用本函数只为给返回值填时间戳，本项目在
  *         Mpu6500_Read 中统一使用 Mpu6500_PortGetMs，
  *         因此这里的写入对象通常是库内部的临时变量。
  */
void inv_get_ms(uint32_t *count)
{
    if (count != 0)
        *count = system_millis;
}

/**
  * @brief  移植层初始化：建立毫秒时基并配置 I2C1。
  *
  *         初始化步骤：
  *           1. SystemCoreClockUpdate  —— 由寄存器反推系统时钟频率，
  *              确保即使用户未在启动代码中更新 SystemCoreClock 也能算对分频；
  *           2. SysTick_Config         —— 配置 1 ms 周期的 SysTick 中断，
  *              中断服务函数在 Src/stm32f10x_it.c 的 SysTick_Handler 中，
  *              它调用 Mpu6500_PortTick1ms 累加时基；
  *           3. 使能 GPIOB 与 I2C1 时钟；
  *           4. 配置 PB6/PB7 为复用开漏输出（I2C 是开漏总线，必须用 AF_OD）；
  *           5. 配置 I2C1 为 400 kHz 快速模式并使能外设。
  *
  * @param  无
  * @retval  0  初始化成功
  * @retval -1  SysTick 配置失败（重装值超出 24 位，即 SystemCoreClock 异常偏大）
  *
  * @note   本函数会调用 I2C_DeInit 先复位外设。若在运行中途重复调用，
  *         已建立的传输会被打断，因此只应在初始化阶段调用一次。
  */
int Mpu6500_PortInit(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    /* 时基部分：1 ms 中断一次，为官方库的延时与时间戳提供基准。 */
    SystemCoreClockUpdate();
    system_millis = 0;
    if (SysTick_Config(SystemCoreClock / 1000u) != 0u)
        return -1;

    /* 开时钟：GPIOB 挂在 APB2，I2C1 挂在 APB1。 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /*
     * PB6/PB7 配为复用开漏。开漏是 I2C 的电气要求：
     * 总线上只能主动拉低、靠外部上拉电阻拉高，这样才能实现线与、避免推挽冲突。
     */
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &gpio);

    /* I2C 部分：复位后按默认值填充，再逐项覆盖需要修改的成员。 */
    I2C_DeInit(MPU6500_I2C);
    I2C_StructInit(&i2c);
    i2c.I2C_ClockSpeed = 400000u;                    /* 400 kHz 快速模式 */
    i2c.I2C_Mode = I2C_Mode_I2C;                     /* 标准 I2C 模式，非 SMBus */
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;             /* 快速模式下 Tlow/Thigh = 2 */
    i2c.I2C_OwnAddress1 = 0x00u;                     /* 仅作主机，本机地址无意义 */
    i2c.I2C_Ack = I2C_Ack_Enable;                    /* 默认应答，收尾时会临时关闭 */
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;  /* 7 位地址模式 */
    I2C_Init(MPU6500_I2C, &i2c);
    I2C_Cmd(MPU6500_I2C, ENABLE);

    return 0;
}

/**
  * @brief  毫秒时基递增，须在 SysTick 中断服务函数中调用。
  *
  * @param  无
  * @retval 无
  *
  * @note   本函数是 system_millis 的唯一写入者，因此无需临界区保护；
  *         主程序只读取它，32 位对齐变量的读取在 Cortex-M3 上是单条指令，天然原子。
  *         回绕由使用侧的无符号相减处理，见 inv_delay_ms 的说明。
  */
void Mpu6500_PortTick1ms(void)
{
    ++system_millis;
}

/**
  * @brief  获取当前毫秒时基。
  *
  * @param  无
  * @retval 上电以来经过的毫秒数
  */
uint32_t Mpu6500_PortGetMs(void)
{
    return system_millis;
}

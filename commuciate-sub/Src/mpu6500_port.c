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

/* 正常等待以 SysTick 计时；循环上限只在时基未前进时兜底，避免永久卡住。 */
#define MPU6500_I2C_WAIT_MS       5u
#define MPU6500_I2C_WAIT_SPINS    16384u
#define MPU6500_I2C_STOP_WAIT_MS  2u

/**
  * @brief  全局毫秒时基，上电清零，由 SysTick 中断每毫秒递增。
  *
  *         加 volatile 是因为它在中断中被修改、在主程序中读取，
  *         防止编译器把循环里的读取优化进寄存器而读不到新值。
  */
static volatile uint32_t system_millis;
static GPIO_TypeDef *bus_scl_port;
static GPIO_TypeDef *bus_sda_port;
static uint16_t bus_scl_pin;
static uint16_t bus_sda_pin;
static I2C_InitTypeDef bus_i2c_config;

static int i2c_clear_stuck_sda(void);
static int i2c_recover_bus(void);

/* bit0=SCL，bit1=SDA；空闲时两位都应为 1。 */
static uint8_t i2c_line_levels(void)
{
    uint8_t levels = 0u;
    if (bus_scl_port != 0 && GPIO_ReadInputDataBit(bus_scl_port, bus_scl_pin) != Bit_RESET)
        levels |= 1u;
    if (bus_sda_port != 0 && GPIO_ReadInputDataBit(bus_sda_port, bus_sda_pin) != Bit_RESET)
        levels |= 2u;
    return levels;
}

/* 收尾阶段需要连续操作 ADDR、ACK 和 STOP，保存原中断状态后短暂屏蔽中断。 */
static uint32_t i2c_enter_critical(void)
{
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask) : : "memory");
    __asm volatile ("cpsid i" : : : "memory");
    return primask;
}

static void i2c_exit_critical(uint32_t primask)
{
    __asm volatile ("msr primask, %0" : : "r" (primask) : "memory");
}

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
    /* 四个错误位都在 SR1；一次寄存器读取即可，减少 1 kHz 路径的开销。 */
    return (MPU6500_I2C->SR1 &
            (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)) != 0u;
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
    uint32_t start = system_millis;
    uint32_t spins = 0u;

    for (;;)
    {
        if (i2c_error_pending())
            return -1;

        if (I2C_CheckEvent(MPU6500_I2C, event) == SUCCESS)
            return 0;

        if ((uint32_t)(system_millis - start) >= MPU6500_I2C_WAIT_MS ||
            ++spins >= MPU6500_I2C_WAIT_SPINS)
        {
            return -1;
        }
    }
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
    uint32_t start = system_millis;
    uint32_t spins = 0u;

    for (;;)
    {
        if (i2c_error_pending())
            return -1;

        if (I2C_GetFlagStatus(MPU6500_I2C, flag) == state)
            return 0;

        if ((uint32_t)(system_millis - start) >= MPU6500_I2C_WAIT_MS ||
            ++spins >= MPU6500_I2C_WAIT_SPINS)
        {
            return -1;
        }
    }
}

/* 给 STOP 最多 2 ms 释放总线；迭代上限防止 SysTick 意外停摆。 */
static int i2c_wait_idle_after_stop(void)
{
    uint32_t start = system_millis;
    uint32_t spins = 0u;

    for (;;)
    {
        if ((MPU6500_I2C->SR2 & I2C_SR2_BUSY) == 0u && i2c_line_levels() == 3u)
            return 0;
        if ((uint32_t)(system_millis - start) >= MPU6500_I2C_STOP_WAIT_MS ||
            ++spins >= MPU6500_I2C_WAIT_SPINS)
            return -1;
    }
}

/* 先完成正常传输清理，仅在 STOP 后仍 BUSY/线低时尝试总线级恢复。 */
static void i2c_recover_transfer(void)
{
    /* 非主机状态不能乱发 STOP，否则可能触发 F1 的 misplaced STOP 勘误。 */
    if ((MPU6500_I2C->SR2 & I2C_SR2_MSL) != 0u)
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
    else
        I2C_GenerateSTART(MPU6500_I2C, DISABLE);
    I2C_AcknowledgeConfig(MPU6500_I2C, ENABLE);
    I2C_NACKPositionConfig(MPU6500_I2C, I2C_NACKPosition_Current);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_AF);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_BERR);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_ARLO);
    I2C_ClearFlag(MPU6500_I2C, I2C_FLAG_OVR);

    if (i2c_wait_idle_after_stop() != 0)
    {
        (void)i2c_recover_bus();
    }
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
        goto error;

    /* 产生起始条件，随后等待 EV5：主机模式已选中。 */
    I2C_GenerateSTART(MPU6500_I2C, ENABLE);
    if (wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0)
        goto error;

    /* 等待地址应答；只轮询 SR1.ADDR，避免事件查询提前读取 SR2 丢失 ADDR。 */
    I2C_Send7bitAddress(MPU6500_I2C, (uint8_t)(slave_addr << 1), I2C_Direction_Transmitter);
    if (wait_flag(I2C_FLAG_ADDR, SET) != 0)
        goto error;
    /* F1 要求见到 ADDR 后才顺序读取 SR1、SR2，完成 EV6 的清标志操作。 */
    (void)MPU6500_I2C->SR1;
    (void)MPU6500_I2C->SR2;

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
  *           1 字节：ADDR 置位后、清除前关闭应答；清除 ADDR 后发 STOP，等 RXNE 后读取；
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
    uint32_t primask;
    uint8_t remaining = length;   /* 剩余待接收字节数，收尾逻辑依赖它判断 */
    uint8_t *cursor = data;       /* 写入游标，随接收推进前移 */

    if ((data == 0) || (length == 0u))
        return -1;

    if (wait_flag(I2C_FLAG_BUSY, RESET) != 0)
        goto error;

    I2C_AcknowledgeConfig(MPU6500_I2C, ENABLE);
    I2C_NACKPositionConfig(MPU6500_I2C, I2C_NACKPosition_Current);

    /* 第一阶段：以「写」方向发送寄存器地址，指明接下来要从哪个寄存器读。 */
    I2C_GenerateSTART(MPU6500_I2C, ENABLE);
    if (wait_event(I2C_EVENT_MASTER_MODE_SELECT) != 0)
        goto error;

    I2C_Send7bitAddress(MPU6500_I2C, (uint8_t)(slave_addr << 1), I2C_Direction_Transmitter);
    if (wait_flag(I2C_FLAG_ADDR, SET) != 0)
        goto error;
    /* SPL 的 I2C_CheckEvent 会无条件读 SR2，可能在 ADDR 刚置位时误清；此处显式清除。 */
    (void)MPU6500_I2C->SR1;
    (void)MPU6500_I2C->SR2;

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

    /* 等待 ADDR 后再按读取长度处理 ACK/POS，并在清除 ADDR 后及时发送 STOP。 */
    I2C_Send7bitAddress(MPU6500_I2C, (uint8_t)(slave_addr << 1), I2C_Direction_Receiver);
    /* 不提前清除 ADDR；接收长度决定 ACK、POS 和 STOP 的精确顺序。 */
    if (wait_flag(I2C_FLAG_ADDR, SET) != 0)
        goto error;

    if (remaining == 1u)
    {
        primask = i2c_enter_critical();
        I2C_AcknowledgeConfig(MPU6500_I2C, DISABLE);
        (void)MPU6500_I2C->SR1;
        (void)MPU6500_I2C->SR2;
        /* 单字节：先发 STOP 再等 RXNE，读到数据后总线事务即告完成。 */
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
        i2c_exit_critical(primask);
        if (wait_flag(I2C_FLAG_RXNE, SET) != 0)
            goto error;
        cursor[0] = I2C_ReceiveData(MPU6500_I2C);
    }
    else if (remaining == 2u)
    {
        primask = i2c_enter_critical();
        I2C_NACKPositionConfig(MPU6500_I2C, I2C_NACKPosition_Next);
        (void)MPU6500_I2C->SR1;
        (void)MPU6500_I2C->SR2;
        I2C_AcknowledgeConfig(MPU6500_I2C, DISABLE);
        i2c_exit_critical(primask);
        /*
         * 双字节：等 BTF 置位（说明第一个字节已搬到数据寄存器、第二个字节
         * 也已收完），此时发 STOP，然后连续读走两个字节。
         */
        if (wait_flag(I2C_FLAG_BTF, SET) != 0)
            goto error;
        primask = i2c_enter_critical();
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
        cursor[0] = I2C_ReceiveData(MPU6500_I2C);
        cursor[1] = I2C_ReceiveData(MPU6500_I2C);
        i2c_exit_critical(primask);
    }
    else
    {
        (void)MPU6500_I2C->SR1;
        (void)MPU6500_I2C->SR2;
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
        primask = i2c_enter_critical();
        I2C_AcknowledgeConfig(MPU6500_I2C, DISABLE);
        *cursor++ = I2C_ReceiveData(MPU6500_I2C);
        i2c_exit_critical(primask);
        if (wait_flag(I2C_FLAG_BTF, SET) != 0)
            goto error;
        primask = i2c_enter_critical();
        I2C_GenerateSTOP(MPU6500_I2C, ENABLE);
        *cursor++ = I2C_ReceiveData(MPU6500_I2C);
        *cursor = I2C_ReceiveData(MPU6500_I2C);
        i2c_exit_critical(primask);
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
static uint32_t gpio_clock_for_port(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    return 0u;
}

/* 仅供故障恢复的短暂电平保持，不依赖可能停摆的 SysTick。 */
static void i2c_gpio_pause(void)
{
    volatile uint32_t spins = SystemCoreClock / 200000u;

    if (spins < 16u)
        spins = 16u;
    while (spins-- != 0u)
        __NOP();
}

static void i2c_set_gpio_mode(GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = mode;
    gpio.GPIO_Pin = bus_scl_pin;
    GPIO_Init(bus_scl_port, &gpio);
    gpio.GPIO_Pin = bus_sda_pin;
    GPIO_Init(bus_sda_port, &gpio);
}

/* 仅在 I2C 外设已关闭且 SCL 高、SDA 低时打时钟，最多九次。 */
static int i2c_clear_stuck_sda(void)
{
    uint8_t pulse;

    if (i2c_line_levels() == 3u)
        return 0;
    if (i2c_line_levels() != 1u)
        return -1; /* SCL 被外部拉低时不能强制打时钟。 */

    /* 开漏写 1 只是释放线路，不会向传感器强行输出高电平。 */
    GPIO_SetBits(bus_scl_port, bus_scl_pin);
    GPIO_SetBits(bus_sda_port, bus_sda_pin);
    i2c_set_gpio_mode(GPIO_Mode_Out_OD);
    i2c_gpio_pause();

    for (pulse = 0u; pulse < 9u && i2c_line_levels() == 1u; ++pulse)
    {
        GPIO_ResetBits(bus_scl_port, bus_scl_pin);
        i2c_gpio_pause();
        GPIO_SetBits(bus_scl_port, bus_scl_pin);
        i2c_gpio_pause();
    }

    if (i2c_line_levels() == 3u)
    {
        GPIO_ResetBits(bus_sda_port, bus_sda_pin);
        i2c_gpio_pause();
        GPIO_SetBits(bus_sda_port, bus_sda_pin);
        i2c_gpio_pause();
    }

    i2c_set_gpio_mode(GPIO_Mode_AF_OD);
    return i2c_line_levels() == 3u ? 0 : -1;
}

/* ES096 §2.8.7：线路均高但内部滤波器锁住 BUSY 时，先让两线产生受控跳变。 */
static int i2c_unlock_analog_filter(void)
{
    int result = -1;

    if (i2c_line_levels() != 3u)
        return -1;

    GPIO_SetBits(bus_scl_port, bus_scl_pin);
    GPIO_SetBits(bus_sda_port, bus_sda_pin);
    i2c_set_gpio_mode(GPIO_Mode_Out_OD);
    i2c_gpio_pause();
    if (i2c_line_levels() != 3u)
        goto restore;

    GPIO_ResetBits(bus_sda_port, bus_sda_pin);
    i2c_gpio_pause();
    if ((i2c_line_levels() & 2u) != 0u)
        goto restore;

    GPIO_ResetBits(bus_scl_port, bus_scl_pin);
    i2c_gpio_pause();
    if ((i2c_line_levels() & 1u) != 0u)
        goto restore;

    GPIO_SetBits(bus_scl_port, bus_scl_pin);
    i2c_gpio_pause();
    if ((i2c_line_levels() & 1u) == 0u)
        goto restore;

    GPIO_SetBits(bus_sda_port, bus_sda_pin);
    i2c_gpio_pause();
    if (i2c_line_levels() == 3u)
        result = 0;

restore:
    GPIO_SetBits(bus_scl_port, bus_scl_pin);
    GPIO_SetBits(bus_sda_port, bus_sda_pin);
    i2c_gpio_pause();
    i2c_set_gpio_mode(GPIO_Mode_AF_OD);
    return result;
}

static int i2c_recover_bus(void)
{
    /* 必须先关 PE，GPIO 才能临时接管开漏线路。 */
    I2C_Cmd(MPU6500_I2C, DISABLE);

    if (i2c_line_levels() == 1u)
        (void)i2c_clear_stuck_sda();

    if (i2c_line_levels() == 3u && i2c_unlock_analog_filter() == 0)
    {
        /* 仅在线路确认已释放并完成跳变后才做 SWRST。 */
        I2C_SoftwareResetCmd(MPU6500_I2C, ENABLE);
        I2C_SoftwareResetCmd(MPU6500_I2C, DISABLE);
        I2C_Init(MPU6500_I2C, &bus_i2c_config);
        I2C_Cmd(MPU6500_I2C, ENABLE);
        return i2c_wait_idle_after_stop();
    }

    /* 外部仍拉低线路：不盲目 SWRST，下次调用仍可重试。 */
    I2C_Cmd(MPU6500_I2C, ENABLE);
    return -1;
}

int Mpu6500_PortInit(GPIO_TypeDef *scl_port, uint16_t scl_pin,
                     GPIO_TypeDef *sda_port, uint16_t sda_pin,
                     FunctionalState remap_i2c1)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;
    uint32_t gpio_clocks;

    if (scl_port == 0 || sda_port == 0 || scl_pin == 0u || sda_pin == 0u)
        return -1;
    bus_scl_port = scl_port;
    bus_sda_port = sda_port;
    bus_scl_pin = scl_pin;
    bus_sda_pin = sda_pin;

    /* 时基部分：1 ms 中断一次，为官方库的延时与时间戳提供基准。 */
    SystemCoreClockUpdate();
    system_millis = 0;
    if (SysTick_Config(SystemCoreClock / 1000u) != 0u)
        return -1;

    /* 开时钟：GPIOB 挂在 APB2，I2C1 挂在 APB1。 */
    gpio_clocks = gpio_clock_for_port(scl_port) | gpio_clock_for_port(sda_port);
    if (gpio_clocks == 0u)
        return -1;
    RCC_APB2PeriphClockCmd(gpio_clocks, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_I2C1, remap_i2c1);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /*
     * PB6/PB7 配为复用开漏。开漏是 I2C 的电气要求：
     * 总线上只能主动拉低、靠外部上拉电阻拉高，这样才能实现线与、避免推挽冲突。
     */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Pin = scl_pin;
    GPIO_Init(scl_port, &gpio);
    gpio.GPIO_Pin = sda_pin;
    GPIO_Init(sda_port, &gpio);

    /* I2C 部分：复位后按默认值填充，再逐项覆盖需要修改的成员。 */
    I2C_DeInit(MPU6500_I2C);
    i2c_clear_stuck_sda();
    I2C_StructInit(&i2c);
    i2c.I2C_ClockSpeed = 400000u;                    /* 400 kHz 快速模式 */
    i2c.I2C_Mode = I2C_Mode_I2C;                     /* 标准 I2C 模式，非 SMBus */
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;             /* 快速模式下 Tlow/Thigh = 2 */
    i2c.I2C_OwnAddress1 = 0x00u;                     /* 仅作主机，本机地址无意义 */
    i2c.I2C_Ack = I2C_Ack_Enable;                    /* 默认应答，收尾时会临时关闭 */
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;  /* 7 位地址模式 */
    bus_i2c_config = i2c;                            /* 运行时恢复后重建相同配置 */
    I2C_Init(MPU6500_I2C, &i2c);
    I2C_Cmd(MPU6500_I2C, ENABLE);

    /* 若线路均高但 BUSY 锁住，执行 ES096 要求的线路跳变后再复位。 */
    inv_delay_ms(1u);
    if (I2C_GetFlagStatus(MPU6500_I2C, I2C_FLAG_BUSY) != RESET &&
        i2c_line_levels() == 3u)
    {
        (void)i2c_recover_bus();
    }

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

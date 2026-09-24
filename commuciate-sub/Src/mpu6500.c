/**
  ******************************************************************************
  * @file    mpu6500.c
  * @brief   MPU6500 六轴惯性测量单元驱动（业务层实现）。
  *
  *          本层不直接操作 I2C 寄存器，而是建立在 InvenSense 官方库
  *          Lib/InvenSense/inv_mpu.c 之上：由它完成寄存器读写与时序控制，
  *          本层负责「参数配置」与「原始值到物理量的换算」两件事。
  *
  *          典型调用顺序：
  *            Mpu6500_Init()            // 上电后调用一次，失败则不要继续
  *            Mpu6500_Read(&data)       // 循环调用，每次得到一组采样
  *
  * @note    本文件依赖的编译宏在 CMakeLists.txt 中定义：
  *            MPU6500        —— 让 inv_mpu.c 选择 MPU6500 的寄存器表
  *            REMOVE_LOGGING —— 关闭官方库的日志输出，避免依赖 printf
  ******************************************************************************
  */

#include "mpu6500.h"

#include "inv_mpu.h"
#include "mpu6500_port.h"

/**
  * @brief  陀螺仪灵敏度系数，单位 LSB/(°/s)。
  *
  *         本值由配置的满量程决定（±500 dps 对应 65.5），在 Mpu6500_Init 中
  *         通过 mpu_get_gyro_sens 向驱动库查询得到，因此不会与配置脱节。
  *         换算公式：角速度(°/s) = 原始值 / gyro_sensitivity
  */
static float gyro_sensitivity;

/**
  * @brief  加速度计灵敏度系数，单位 LSB/g。
  *
  *         本值由配置的满量程决定（±4 g 对应 8192），在 Mpu6500_Init 中
  *         通过 mpu_get_accel_sens 向驱动库查询得到。
  *         换算公式：加速度(g) = 原始值 / accel_sensitivity
  */
static unsigned short accel_sensitivity;

/**
  * @brief  初始化完成标志。
  *
  *         为 0 时 Mpu6500_Read 直接返回错误，避免在芯片未就绪时发起无意义的
  *         总线传输。初始化中途失败也会被清 0。
  */
static uint8_t initialized;

/**
  * @brief  初始化 MPU6500。
  *
  *         执行流程：
  *           1. Mpu6500_PortInit  —— 建立 SysTick 毫秒时基，配置 I2C1 引脚与外设；
  *           2. mpu_init(0)       —— 复位并唤醒芯片，参数必须传 NULL：
  *              官方库仅在没有定义 EMPL_TARGET_STM32F4 时才对非空指针调用
  *              reg_int_cb，本项目未使用中断引脚，传非空指针只会造成误解；
  *           3. 使能加速度计与陀螺仪，设置量程、采样率、低通滤波器；
  *           4. 查询并缓存两个灵敏度系数，供电给 Mpu6500_Read 做换算。
  *
  * @param  无
  * @retval  0  初始化成功
  * @retval -1  移植层初始化失败
  * @retval -2  mpu_init 失败，芯片无应答
  * @retval -3  寄存器配置失败
  */
int Mpu6500_Init(void)
{
    if (Mpu6500_PortInit() != 0)
        return -1;

    /* 上电复位并唤醒芯片；此处不注册中断回调，故参数为 NULL。 */
    if (mpu_init(0) != 0)
        return -2;

    /*
     * 一次性完成所有参数配置，任意一项失败都视为初始化失败。
     * 各项含义：
     *   INV_XYZ_ACCEL | INV_XYZ_GYRO  同时使能加速度计与陀螺仪的 X/Y/Z 三轴
     *   mpu_set_gyro_fsr(500)         陀螺仪满量程 ±500 °/s（兼顾精度与量程）
     *   mpu_set_accel_fsr(4)          加速度计满量程 ±4 g
     *   mpu_set_sample_rate(1000)     采样率 1 kHz，即内部采样周期 1 ms
     *   mpu_set_lpf(98)               数字低通滤波器带宽 98 Hz。注意调用顺序：
     *                                 mpu_set_sample_rate 内部会自动把带宽设为
     *                                 采样率的一半（500 Hz，向上取到 188 Hz 档），
     *                                 因此必须把 mpu_set_lpf(98) 放在它之后，
     *                                 才能覆盖为更窄的 98 Hz 以获得更干净的信号
     *                                 （可选档位只有 188/98/42/20/10/5 Hz 六种）
     *   mpu_get_gyro_sens             回读陀螺仪灵敏度系数（±500 dps → 65.5 LSB/(°/s)）
     *   mpu_get_accel_sens            回读加速度计灵敏度系数（±4 g → 8192 LSB/g）
     */
    if (mpu_set_sensors(INV_XYZ_ACCEL | INV_XYZ_GYRO) != 0 ||
        mpu_set_gyro_fsr(500) != 0 ||
        mpu_set_accel_fsr(4) != 0 ||
        mpu_set_sample_rate(1000) != 0 ||
        mpu_set_lpf(98) != 0 ||
        mpu_get_gyro_sens(&gyro_sensitivity) != 0 ||
        mpu_get_accel_sens(&accel_sensitivity) != 0)
    {
        initialized = 0;
        return -3;
    }

    initialized = 1;
    return 0;
}

/**
  * @brief  读取一次加速度、角速度与温度。
  *
  *         读取流程：
  *           1. mpu_get_gyro_reg    —— 读 6 字节原始陀螺仪数据，换算为 °/s；
  *           2. mpu_get_accel_reg   —— 读 6 字节原始加速度计数据，换算为 g；
  *           3. mpu_get_temperature —— 读 2 字节温度，结果已是 Q16 定点格式。
  *
  * @param[out] data  采样结果输出缓冲，不可为 NULL。
  * @retval  0  读取成功
  * @retval -1  参数无效或驱动尚未初始化
  * @retval -2  I2C 传输失败
  *
  * @note    一次调用包含 3 次独立的 I2C 传输，因此三个物理量并非严格同一时刻
  *          的采样结果，只是时间上紧邻（µs 量级）。对同步性要求高的场合
  *          应改用官方库的 FIFO 接口一次性读取。
  */
int Mpu6500_Read(Mpu6500_Data *data)
{
    short accel_raw[3];        /* 加速度计原始值，单位 LSB */
    short gyro_raw[3];         /* 陀螺仪原始值，单位 LSB */
    long temperature_q16;      /* 温度值，Q16 定点格式（高 16 位为整数部分） */
    Mpu6500_Data sample;       /* 先在临时变量中凑齐整组数据，最后整体拷贝输出 */
    uint8_t axis;

    if ((data == 0) || (initialized == 0))
        return -1;

    /*
     * 时间戳参数传 0：本驱动在最后统一取自己维护的毫秒时基，
     * 避免依赖官方库内部对 get_ms 的调用，也省去一次多余的接口往返。
     */
    if ((mpu_get_gyro_reg(gyro_raw, 0) != 0) ||
        (mpu_get_accel_reg(accel_raw, 0) != 0) ||
        (mpu_get_temperature(&temperature_q16, 0) != 0))
        return -2;

    /* 三轴依次换算：除以初始化时缓存的灵敏度系数即得物理量。 */
    for (axis = 0; axis < 3; ++axis)
    {
        sample.accel_g[axis] = (float)accel_raw[axis] / (float)accel_sensitivity;
        sample.gyro_dps[axis] = (float)gyro_raw[axis] / gyro_sensitivity;
    }

    /* 温度 Q16 定点数除以 65536（2^16）还原为摄氏度浮点值。 */
    sample.temperature_c = (float)temperature_q16 / 65536.0f;
    sample.timestamp_ms = Mpu6500_PortGetMs();
    *data = sample;

    return 0;
}

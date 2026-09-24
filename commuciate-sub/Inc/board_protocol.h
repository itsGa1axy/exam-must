#ifndef BOARD_PROTOCOL_H
#define BOARD_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* 串口建议参数：460800 波特率、8 数据位、无校验、1 停止位（8N1）。 */
#define BOARD_PROTOCOL_UART_BAUD             460800u

/* 公共帧格式：AA 55 | 版本:u8 | 类型:u8 | 序号:u16小端 | 长度:u8 |
 * 载荷:长度字节 | CRC16:u16小端。
 * CRC-16/CCITT-FALSE 覆盖“版本”至“载荷末尾”，不包含帧头和 CRC 字段。 */
#define BOARD_PROTOCOL_SOF0                  0xAAu
#define BOARD_PROTOCOL_SOF1                  0x55u
#define BOARD_PROTOCOL_VERSION_1             0x01u
#define BOARD_PROTOCOL_VERSION_2             0x02u
#define BOARD_PROTOCOL_TYPE_QUATERNION       0x01u
#define BOARD_PROTOCOL_TYPE_SET_FILTER_WEIGHT 0x02u
#define BOARD_PROTOCOL_TYPE_RETRANSMIT       0x81u

/*
 * 两版都发送单位四元数，分量顺序 w、x、y、z（实部在前）。
 * 四元数表示从 MPU6500 传感器坐标系到右手 ENU 导航坐标系的旋转；
 * 传感器轴映射固定为芯片 X/Y/Z。MPU6500 无磁力计，航向角以初始方向为参考，
 * 长时间使用可能漂移。
 *
 * V1 载荷：四个 IEEE-754 float32，小端序；无需时间戳，序号用于识别数据帧。
 */
#define BOARD_PROTOCOL_V1_PAYLOAD_SIZE       16u

/* V2 载荷：启动后毫秒时间戳:u32小端，随后是 w、x、y、z 四个有符号 Q30 分量；
 * Q30 解码方式为“有符号整数 / 2^30”。时间戳和序号均按无符号数自然回绕。 */
#define BOARD_PROTOCOL_V2_PAYLOAD_SIZE       20u
#define BOARD_PROTOCOL_RETRANSMIT_PAYLOAD_SIZE 2u
#define BOARD_PROTOCOL_FILTER_WEIGHT_PAYLOAD_SIZE 2u
#define BOARD_PROTOCOL_MAX_PAYLOAD_SIZE      BOARD_PROTOCOL_V2_PAYLOAD_SIZE
#define BOARD_PROTOCOL_MAX_FRAME_SIZE        (9u + BOARD_PROTOCOL_MAX_PAYLOAD_SIZE)

typedef struct
{
    /* 四元数必须归一化；编码器要求每个分量都在 [-1, +1] 范围内。 */
    float w;
    float x;
    float y;
    float z;
} BoardProtocol_Quaternion;

_Static_assert(sizeof(float) == 4u, "protocol V1 requires 32-bit float");

uint16_t BoardProtocol_Crc16CcittFalse(const uint8_t *data, uint16_t length);

/* 返回编码后的帧长；参数无效或缓冲区容量不足时返回 0。 */
size_t BoardProtocol_EncodeV1(uint16_t sequence,
                              const BoardProtocol_Quaternion *quaternion,
                              uint8_t *frame,
                              size_t capacity);

size_t BoardProtocol_EncodeV2(uint16_t sequence,
                              uint32_t sample_time_ms,
                              const BoardProtocol_Quaternion *quaternion,
                              uint8_t *frame,
                              size_t capacity);

/* 主机请求从机重传：类型 0x81；帧序号和 2 字节小端载荷都填写请求的序号。 */
size_t BoardProtocol_EncodeRetransmit(uint8_t version,
                                      uint16_t requested_sequence,
                                      uint8_t *frame,
                                      size_t capacity);
size_t BoardProtocol_EncodeFilterWeight(uint16_t sequence,
                                        uint16_t gyro_weight_q15,
                                        uint8_t *frame,
                                        size_t capacity);

#endif

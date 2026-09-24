#ifndef BOARD_PROTOCOL_H
#define BOARD_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* 通信帧：帧头、版本、类型、序号、载荷长度、载荷和 CRC16。 */
#define BOARD_PROTOCOL_SOF0                 0xAAu
#define BOARD_PROTOCOL_SOF1                 0x55u
#define BOARD_PROTOCOL_VERSION               0x02u
#define BOARD_PROTOCOL_TYPE_QUATERNION      0x01u
#define BOARD_PROTOCOL_TYPE_RETRANSMIT      0x81u

/* 唯一载荷格式：毫秒时间戳和四个有符号 Q30 四元数分量。 */
#define BOARD_PROTOCOL_QUATERNION_SIZE       20u
#define BOARD_PROTOCOL_RETRANSMIT_SIZE       2u
#define BOARD_PROTOCOL_MAX_PAYLOAD_SIZE      BOARD_PROTOCOL_QUATERNION_SIZE
#define BOARD_PROTOCOL_MAX_FRAME_SIZE        (9u + BOARD_PROTOCOL_MAX_PAYLOAD_SIZE)

typedef struct
{
    float w;
    float x;
    float y;
    float z;
} BoardProtocol_Quaternion;

uint16_t BoardProtocol_Crc16CcittFalse(const uint8_t *data, uint16_t length);
uint16_t BoardProtocol_ReadU16Le(const uint8_t *data);
uint32_t BoardProtocol_ReadU32Le(const uint8_t *data);
void BoardProtocol_WriteU16Le(uint8_t *data, uint16_t value);

/* 编码函数返回完整帧长度；参数无效或输出缓冲不足时返回 0。 */
size_t BoardProtocol_EncodeQuaternion(uint16_t sequence,
                                      uint32_t sample_time_ms,
                                      const BoardProtocol_Quaternion *quaternion,
                                      uint8_t *frame,
                                      size_t capacity);

/* 重传请求的帧序号和两字节载荷均为请求重传的序号。 */
size_t BoardProtocol_EncodeRetransmit(uint16_t requested_sequence,
                                      uint8_t *frame,
                                      size_t capacity);

#endif

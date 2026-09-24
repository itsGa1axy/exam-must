#include "board_protocol.h"

#include <string.h>

#define FRAME_OVERHEAD_SIZE 9u

/* 按小端序写入 16 位整数。 */
void BoardProtocol_WriteU16Le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

/* 按小端序读取整数，避免直接依赖结构体内存布局。 */
uint16_t BoardProtocol_ReadU16Le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

uint32_t BoardProtocol_ReadU32Le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

/* CRC-16/CCITT-FALSE：初值 0xFFFF、多项式 0x1021、无反射、无异或输出。 */
uint16_t BoardProtocol_Crc16CcittFalse(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFu;
    uint16_t index;
    uint8_t bit;

    if (data == 0)
        return (length == 0u) ? 0xFFFFu : 0u;

    for (index = 0u; index < length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0u; bit < 8u; ++bit)
        {
            crc = (crc & 0x8000u) != 0u
                      ? (uint16_t)((crc << 1) ^ 0x1021u)
                      : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static size_t encode_frame(uint8_t version, uint8_t type, uint16_t sequence,
                           const uint8_t *payload, uint8_t payload_length,
                           uint8_t *frame, size_t capacity)
{
    size_t frame_length = FRAME_OVERHEAD_SIZE + payload_length;
    uint16_t crc;

    if (frame == 0 || capacity < frame_length ||
        (payload == 0 && payload_length != 0u))
        return 0u;

    frame[0] = BOARD_PROTOCOL_SOF0;
    frame[1] = BOARD_PROTOCOL_SOF1;
    frame[2] = version;
    frame[3] = type;
    BoardProtocol_WriteU16Le(&frame[4], sequence);
    frame[6] = payload_length;
    if (payload_length != 0u)
        memcpy(&frame[7], payload, payload_length);

    /* CRC 覆盖版本到载荷，不包含帧头和 CRC 字段本身。 */
    crc = BoardProtocol_Crc16CcittFalse(&frame[2],
                                        (uint16_t)(5u + payload_length));
    BoardProtocol_WriteU16Le(&frame[7u + payload_length], crc);
    return frame_length;
}

size_t BoardProtocol_EncodeQuaternion(uint16_t sequence,
                                      uint32_t sample_time_ms,
                                      const BoardProtocol_Quaternion *quaternion,
                                      uint8_t *frame,
                                      size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_QUATERNION_SIZE];
    float values[4];
    uint8_t i;

    if (quaternion == 0)
        return 0u;
    values[0] = quaternion->w;
    values[1] = quaternion->x;
    values[2] = quaternion->y;
    values[3] = quaternion->z;
    write_u32_le(&payload[0], sample_time_ms);
    for (i = 0u; i < 4u; ++i)
    {
        int32_t q30;
        if (!(values[i] >= -1.0f && values[i] <= 1.0f))
            return 0u;
        q30 = (int32_t)(values[i] * 1073741824.0f);
        write_u32_le(&payload[4u + i * 4u], (uint32_t)q30);
    }
    return encode_frame(BOARD_PROTOCOL_VERSION,
                        BOARD_PROTOCOL_TYPE_QUATERNION, sequence,
                        payload, BOARD_PROTOCOL_QUATERNION_SIZE,
                        frame, capacity);
}

size_t BoardProtocol_EncodeRetransmit(uint16_t requested_sequence,
                                      uint8_t *frame,
                                      size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_RETRANSMIT_SIZE];
    BoardProtocol_WriteU16Le(payload, requested_sequence);
    return encode_frame(BOARD_PROTOCOL_VERSION, BOARD_PROTOCOL_TYPE_RETRANSMIT,
                        requested_sequence, payload,
                        BOARD_PROTOCOL_RETRANSMIT_SIZE, frame, capacity);
}

#include "board_protocol.h"

#include <string.h>

#define BOARD_PROTOCOL_FRAME_OVERHEAD 9u

/* 将 16 位整数按小端序写入字节数组。 */
static void write_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
}

/* 将 32 位整数按小端序写入字节数组。 */
static void write_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

/* 将 float32 的位模式按小端序写入，避免依赖结构体内存布局。 */
static void write_float_le(uint8_t *dst, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(dst, bits);
}

/* 组装公共帧头、载荷和 CRC。返回总帧长；参数无效时返回 0。 */
static size_t encode_frame(uint8_t version,
                           uint8_t type,
                           uint16_t sequence,
                           const uint8_t *payload,
                           uint8_t payload_length,
                           uint8_t *frame,
                           size_t capacity)
{
    uint16_t crc;
    size_t frame_length = BOARD_PROTOCOL_FRAME_OVERHEAD + payload_length;

    if ((frame == 0) || (capacity < frame_length) ||
        ((payload == 0) && (payload_length != 0u)))
        return 0u;

    frame[0] = BOARD_PROTOCOL_SOF0;
    frame[1] = BOARD_PROTOCOL_SOF1;
    frame[2] = version;
    frame[3] = type;
    write_u16_le(&frame[4], sequence);
    frame[6] = payload_length;
    if (payload_length != 0u)
        memcpy(&frame[7], payload, payload_length);

    /* CRC 覆盖 version、type、sequence、length 和 payload，不含帧头与 CRC 本身。 */
    crc = BoardProtocol_Crc16CcittFalse(&frame[2], (uint16_t)(5u + payload_length));
    write_u16_le(&frame[7u + payload_length], crc);

    return frame_length;
}

/* CRC-16/CCITT-FALSE：初值 0xFFFF，多项式 0x1021，不反射，无最终异或。 */
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

/* V1：载荷依次为 w、x、y、z 四个 float32，共 16 字节。 */
size_t BoardProtocol_EncodeV1(uint16_t sequence,
                              const BoardProtocol_Quaternion *quaternion,
                              uint8_t *frame,
                              size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_V1_PAYLOAD_SIZE];
    float components[4];
    uint8_t index;

    if (quaternion == 0)
        return 0u;

    components[0] = quaternion->w;
    components[1] = quaternion->x;
    components[2] = quaternion->y;
    components[3] = quaternion->z;

    for (index = 0u; index < 4u; ++index)
    {
        if (!(components[index] >= -1.0f && components[index] <= 1.0f))
            return 0u;
        write_float_le(&payload[index * 4u], components[index]);
    }

    return encode_frame(BOARD_PROTOCOL_VERSION_1,
                        BOARD_PROTOCOL_TYPE_QUATERNION,
                        sequence,
                        payload,
                        BOARD_PROTOCOL_V1_PAYLOAD_SIZE,
                        frame,
                        capacity);
}

/* V2：载荷为毫秒时间戳和四个 Q30 四元数分量，共 20 字节。 */
size_t BoardProtocol_EncodeV2(uint16_t sequence,
                              uint32_t sample_time_ms,
                              const BoardProtocol_Quaternion *quaternion,
                              uint8_t *frame,
                              size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_V2_PAYLOAD_SIZE];
    float components[4];
    uint8_t index;

    if (quaternion == 0)
        return 0u;

    components[0] = quaternion->w;
    components[1] = quaternion->x;
    components[2] = quaternion->y;
    components[3] = quaternion->z;
    write_u32_le(&payload[0], sample_time_ms);

    for (index = 0u; index < 4u; ++index)
    {
        int32_t q30;

        if (!(components[index] >= -1.0f && components[index] <= 1.0f))
            return 0u;
        q30 = (int32_t)(components[index] * 1073741824.0f);
        write_u32_le(&payload[4u + index * 4u], (uint32_t)q30);
    }

    return encode_frame(BOARD_PROTOCOL_VERSION_2,
                        BOARD_PROTOCOL_TYPE_QUATERNION,
                        sequence,
                        payload,
                        BOARD_PROTOCOL_V2_PAYLOAD_SIZE,
                        frame,
                        capacity);
}

/* 组装主机请求重传的控制帧；两处序号都携带待重传的帧序号。 */
size_t BoardProtocol_EncodeRetransmit(uint8_t version,
                                      uint16_t requested_sequence,
                                      uint8_t *frame,
                                      size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_RETRANSMIT_PAYLOAD_SIZE];

    if ((version != BOARD_PROTOCOL_VERSION_1) && (version != BOARD_PROTOCOL_VERSION_2))
        return 0u;

    write_u16_le(payload, requested_sequence);
    return encode_frame(version,
                        BOARD_PROTOCOL_TYPE_RETRANSMIT,
                        requested_sequence,
                        payload,
                        BOARD_PROTOCOL_RETRANSMIT_PAYLOAD_SIZE,
                        frame,
                        capacity);
}

/* 编码从机滤波陀螺仪权重设置指令，权重按 Q15 表示。 */
size_t BoardProtocol_EncodeFilterWeight(uint16_t sequence,
                                        uint16_t gyro_weight_q15,
                                        uint8_t *frame,
                                        size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_FILTER_WEIGHT_PAYLOAD_SIZE];
    if (gyro_weight_q15 > 32767u)
        return 0u;
    write_u16_le(payload, gyro_weight_q15);
    return encode_frame(BOARD_PROTOCOL_VERSION_2,
                        BOARD_PROTOCOL_TYPE_SET_FILTER_WEIGHT,
                        sequence, payload,
                        BOARD_PROTOCOL_FILTER_WEIGHT_PAYLOAD_SIZE,
                        frame, capacity);
}

#include "board_protocol.h"

#include <string.h>

#define BOARD_PROTOCOL_FRAME_OVERHEAD 9u

/* CRC-16/CCITT-FALSE 查表：表项由多项式 0x1021 生成，保持原协议校验结果。 */
static const uint16_t crc16_table[256] =
{
    0x0000u, 0x1021u, 0x2042u, 0x3063u, 0x4084u, 0x50A5u, 0x60C6u, 0x70E7u,
    0x8108u, 0x9129u, 0xA14Au, 0xB16Bu, 0xC18Cu, 0xD1ADu, 0xE1CEu, 0xF1EFu,
    0x1231u, 0x0210u, 0x3273u, 0x2252u, 0x52B5u, 0x4294u, 0x72F7u, 0x62D6u,
    0x9339u, 0x8318u, 0xB37Bu, 0xA35Au, 0xD3BDu, 0xC39Cu, 0xF3FFu, 0xE3DEu,
    0x2462u, 0x3443u, 0x0420u, 0x1401u, 0x64E6u, 0x74C7u, 0x44A4u, 0x5485u,
    0xA56Au, 0xB54Bu, 0x8528u, 0x9509u, 0xE5EEu, 0xF5CFu, 0xC5ACu, 0xD58Du,
    0x3653u, 0x2672u, 0x1611u, 0x0630u, 0x76D7u, 0x66F6u, 0x5695u, 0x46B4u,
    0xB75Bu, 0xA77Au, 0x9719u, 0x8738u, 0xF7DFu, 0xE7FEu, 0xD79Du, 0xC7BCu,
    0x48C4u, 0x58E5u, 0x6886u, 0x78A7u, 0x0840u, 0x1861u, 0x2802u, 0x3823u,
    0xC9CCu, 0xD9EDu, 0xE98Eu, 0xF9AFu, 0x8948u, 0x9969u, 0xA90Au, 0xB92Bu,
    0x5AF5u, 0x4AD4u, 0x7AB7u, 0x6A96u, 0x1A71u, 0x0A50u, 0x3A33u, 0x2A12u,
    0xDBFDu, 0xCBDCu, 0xFBBFu, 0xEB9Eu, 0x9B79u, 0x8B58u, 0xBB3Bu, 0xAB1Au,
    0x6CA6u, 0x7C87u, 0x4CE4u, 0x5CC5u, 0x2C22u, 0x3C03u, 0x0C60u, 0x1C41u,
    0xEDAEu, 0xFD8Fu, 0xCDECu, 0xDDCDu, 0xAD2Au, 0xBD0Bu, 0x8D68u, 0x9D49u,
    0x7E97u, 0x6EB6u, 0x5ED5u, 0x4EF4u, 0x3E13u, 0x2E32u, 0x1E51u, 0x0E70u,
    0xFF9Fu, 0xEFBEu, 0xDFDDu, 0xCFFCu, 0xBF1Bu, 0xAF3Au, 0x9F59u, 0x8F78u,
    0x9188u, 0x81A9u, 0xB1CAu, 0xA1EBu, 0xD10Cu, 0xC12Du, 0xF14Eu, 0xE16Fu,
    0x1080u, 0x00A1u, 0x30C2u, 0x20E3u, 0x5004u, 0x4025u, 0x7046u, 0x6067u,
    0x83B9u, 0x9398u, 0xA3FBu, 0xB3DAu, 0xC33Du, 0xD31Cu, 0xE37Fu, 0xF35Eu,
    0x02B1u, 0x1290u, 0x22F3u, 0x32D2u, 0x4235u, 0x5214u, 0x6277u, 0x7256u,
    0xB5EAu, 0xA5CBu, 0x95A8u, 0x8589u, 0xF56Eu, 0xE54Fu, 0xD52Cu, 0xC50Du,
    0x34E2u, 0x24C3u, 0x14A0u, 0x0481u, 0x7466u, 0x6447u, 0x5424u, 0x4405u,
    0xA7DBu, 0xB7FAu, 0x8799u, 0x97B8u, 0xE75Fu, 0xF77Eu, 0xC71Du, 0xD73Cu,
    0x26D3u, 0x36F2u, 0x0691u, 0x16B0u, 0x6657u, 0x7676u, 0x4615u, 0x5634u,
    0xD94Cu, 0xC96Du, 0xF90Eu, 0xE92Fu, 0x99C8u, 0x89E9u, 0xB98Au, 0xA9ABu,
    0x5844u, 0x4865u, 0x7806u, 0x6827u, 0x18C0u, 0x08E1u, 0x3882u, 0x28A3u,
    0xCB7Du, 0xDB5Cu, 0xEB3Fu, 0xFB1Eu, 0x8BF9u, 0x9BD8u, 0xABBBu, 0xBB9Au,
    0x4A75u, 0x5A54u, 0x6A37u, 0x7A16u, 0x0AF1u, 0x1AD0u, 0x2AB3u, 0x3A92u,
    0xFD2Eu, 0xED0Fu, 0xDD6Cu, 0xCD4Du, 0xBDAAu, 0xAD8Bu, 0x9DE8u, 0x8DC9u,
    0x7C26u, 0x6C07u, 0x5C64u, 0x4C45u, 0x3CA2u, 0x2C83u, 0x1CE0u, 0x0CC1u,
    0xEF1Fu, 0xFF3Eu, 0xCF5Du, 0xDF7Cu, 0xAF9Bu, 0xBFBAu, 0x8FD9u, 0x9FF8u,
    0x6E17u, 0x7E36u, 0x4E55u, 0x5E74u, 0x2E93u, 0x3EB2u, 0x0ED1u, 0x1EF0u
};

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

/* 组装公共帧头、载荷和 CRC。返回总帧长；参数无效时返回 0。 */
static size_t encode_frame(uint8_t type,
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
    frame[2] = BOARD_PROTOCOL_VERSION;
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

    if (data == 0)
        return (length == 0u) ? 0xFFFFu : 0u;

    for (index = 0u; index < length; ++index)
        crc = (uint16_t)((crc << 8) ^
                         crc16_table[((crc >> 8) ^ data[index]) & 0xFFu]);

    return crc;
}

/* 姿态载荷为毫秒时间戳和四个 Q30 四元数分量，共 20 字节。 */
size_t BoardProtocol_EncodeQuaternion(uint16_t sequence,
                                      uint32_t sample_time_ms,
                                      const BoardProtocol_Quaternion *quaternion,
                                      uint8_t *frame,
                                      size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_QUATERNION_SIZE];
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

    return encode_frame(BOARD_PROTOCOL_TYPE_QUATERNION,
                        sequence,
                        payload,
                        BOARD_PROTOCOL_QUATERNION_SIZE,
                        frame,
                        capacity);
}

/* 组装主机请求重传的控制帧；两处序号都携带待重传的帧序号。 */
size_t BoardProtocol_EncodeRetransmit(uint16_t requested_sequence,
                                      uint8_t *frame,
                                      size_t capacity)
{
    uint8_t payload[BOARD_PROTOCOL_RETRANSMIT_PAYLOAD_SIZE];

    write_u16_le(payload, requested_sequence);
    return encode_frame(BOARD_PROTOCOL_TYPE_RETRANSMIT,
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
    return encode_frame(BOARD_PROTOCOL_TYPE_SET_FILTER_WEIGHT,
                        sequence, payload,
                        BOARD_PROTOCOL_FILTER_WEIGHT_PAYLOAD_SIZE,
                        frame, capacity);
}

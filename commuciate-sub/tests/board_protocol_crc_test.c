#include "board_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* 独立的逐位实现，用于验证查表优化没有改变线上 CRC。 */
static uint16_t reference_crc(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t bit;

    for (i = 0u; i < length; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

int main(void)
{
    static const uint8_t check[] = "123456789";
    uint8_t data[32];
    uint8_t frame[32];
    BoardProtocol_Quaternion q = {1.0f, 0.0f, 0.0f, 0.0f};
    unsigned int seed;
    unsigned int length;
    unsigned int i;
    size_t frame_length;
    uint16_t frame_crc;

    assert(BoardProtocol_Crc16CcittFalse(check, 9u) == 0x29B1u);
    assert(BoardProtocol_Crc16CcittFalse(0, 0u) == 0xFFFFu);
    assert(BoardProtocol_Crc16CcittFalse(0, 1u) == 0u);

    for (seed = 0u; seed < 256u; ++seed)
    {
        for (i = 0u; i < sizeof(data); ++i)
            data[i] = (uint8_t)(seed + i * 37u);
        for (length = 0u; length <= sizeof(data); ++length)
            assert(BoardProtocol_Crc16CcittFalse(data, (uint16_t)length) ==
                   reference_crc(data, (uint16_t)length));
    }

    frame_length = BoardProtocol_EncodeQuaternion(0x1234u, 0x01020304u,
                                                  &q, frame, sizeof(frame));
    assert(frame_length == 29u);
    frame_crc = reference_crc(&frame[2], 25u);
    assert(frame[27] == (uint8_t)frame_crc);
    assert(frame[28] == (uint8_t)(frame_crc >> 8));

    frame_length = BoardProtocol_EncodeRetransmit(0x1234u,
                                                   frame, sizeof(frame));
    assert(frame_length == 11u);
    assert(frame[2] == BOARD_PROTOCOL_VERSION);
    assert(frame[3] == BOARD_PROTOCOL_TYPE_RETRANSMIT);
    assert(frame[4] == 0x34u && frame[5] == 0x12u);
    assert(frame[7] == 0x34u && frame[8] == 0x12u);
    frame_crc = reference_crc(&frame[2], 7u);
    assert(frame[9] == (uint8_t)frame_crc);
    assert(frame[10] == (uint8_t)(frame_crc >> 8));

    puts("board_protocol_crc_test: OK");
    return 0;
}

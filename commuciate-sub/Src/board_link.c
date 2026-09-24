#include "board_link.h"
#include "board_protocol.h"
#include "board_transport.h"
#include <string.h>

#define RETRY_CACHE_COUNT 8u
#define FRAME_SIZE 32u
#define FIXED_SIZE 5u

typedef struct
{
    uint16_t sequence;
    uint8_t length;
    uint8_t frame[FRAME_SIZE];
} CachedFrame;

static CachedFrame cache[RETRY_CACHE_COUNT];
static uint16_t next_sequence;
static uint8_t parser[FRAME_SIZE];
static uint8_t parser_index;
static uint8_t parser_state;
static uint8_t parser_length;

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* 收到主机 NACK 后，只重发缓存中的原始帧，序号和 CRC 均保持一致。 */
static void handle_request(void)
{
    uint16_t sequence = read_u16(&parser[4]);
    uint16_t payload_sequence = read_u16(&parser[7]);
    uint16_t crc = BoardProtocol_Crc16CcittFalse(&parser[2], 7u);
    uint8_t crc_offset = (uint8_t)(7u + parser_length);
    uint8_t i;

    if (parser[2] != BOARD_PROTOCOL_VERSION_2 ||
        parser[3] != BOARD_PROTOCOL_TYPE_RETRANSMIT || parser_length != 2u ||
        sequence != payload_sequence || read_u16(&parser[crc_offset]) != crc)
        return;
    for (i = 0u; i < RETRY_CACHE_COUNT; ++i)
    {
        if (cache[i].length != 0u && cache[i].sequence == sequence)
        {
            (void)BoardTransport_Send(cache[i].frame, cache[i].length);
            return;
        }
    }
}

void BoardLink_Init(void)
{
    memset(cache, 0, sizeof(cache));
    next_sequence = 0u;
    parser_state = parser_index = parser_length = 0u;
    BoardTransport_Init();
}

int BoardLink_SendAttitude(const Attitude_Result *attitude)
{
    BoardProtocol_Quaternion q;
    uint8_t frame[FRAME_SIZE];
    size_t length;
    uint16_t sequence;
    CachedFrame *slot;

    if (attitude == 0)
        return -1;
    q.w = attitude->quaternion_wxyz[0];
    q.x = attitude->quaternion_wxyz[1];
    q.y = attitude->quaternion_wxyz[2];
    q.z = attitude->quaternion_wxyz[3];
    sequence = next_sequence;
    length = BoardProtocol_EncodeV2(sequence, attitude->timestamp_ms, &q,
                                    frame, sizeof(frame));
    if (length == 0u)
        return -1;
    if (BoardTransport_Send(frame, length) != 0)
        return -2;
    slot = &cache[sequence % RETRY_CACHE_COUNT];
    slot->sequence = sequence;
    slot->length = (uint8_t)length;
    memcpy(slot->frame, frame, length);
    ++next_sequence;
    return 0;
}

static void parse_byte(uint8_t byte)
{
    if (parser_state == 0u)
    {
        if (byte == BOARD_PROTOCOL_SOF0)
        {
            parser[0] = byte;
            parser_state = 1u;
        }
        return;
    }
    if (parser_state == 1u)
    {
        if (byte == BOARD_PROTOCOL_SOF1)
        {
            parser[1] = byte;
            parser_state = 2u;
            parser_index = 2u;
        }
        else parser_state = (byte == BOARD_PROTOCOL_SOF0) ? 1u : 0u;
        return;
    }
    parser[parser_index++] = byte;
    if (parser_index == 7u)
    {
        parser_length = parser[6];
        if (parser_length > 20u)
        {
            parser_state = 0u;
            return;
        }
    }
    if (parser_index == (uint8_t)(9u + parser_length))
    {
        handle_request();
        parser_state = 0u;
        parser_index = 0u;
    }
}

void BoardLink_Process(void)
{
    uint8_t byte;
    while (BoardTransport_ReadByte(&byte) != 0)
        parse_byte(byte);
}

void BoardLink_RxIrqHandler(void)
{
    BoardTransport_RxIrqHandler();
}

void BoardLink_TxDmaIrqHandler(void)
{
    BoardTransport_TxDmaIrqHandler();
}

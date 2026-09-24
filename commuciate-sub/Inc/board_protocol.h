#ifndef BOARD_PROTOCOL_H
#define BOARD_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* UART transport: 460800 baud, 8 data bits, no parity, 1 stop bit (8N1). */
#define BOARD_PROTOCOL_UART_BAUD             460800u

/* Frame: AA 55 | version:u8 | type:u8 | sequence:u16-LE | length:u8 |
 * payload:length bytes | CRC16:u16-LE.
 * CRC-16/CCITT-FALSE covers version through the last payload byte. */
#define BOARD_PROTOCOL_SOF0                  0xAAu
#define BOARD_PROTOCOL_SOF1                  0x55u
#define BOARD_PROTOCOL_VERSION_1             0x01u
#define BOARD_PROTOCOL_VERSION_2             0x02u
#define BOARD_PROTOCOL_TYPE_QUATERNION       0x01u
#define BOARD_PROTOCOL_TYPE_RETRANSMIT       0x81u

/* V1: four IEEE-754 float32 values, w,x,y,z, in little-endian byte order. */
#define BOARD_PROTOCOL_V1_PAYLOAD_SIZE       16u

/* V2: timestamp_ms:u32-LE followed by four signed Q30 values w,x,y,z. */
#define BOARD_PROTOCOL_V2_PAYLOAD_SIZE       20u
#define BOARD_PROTOCOL_RETRANSMIT_PAYLOAD_SIZE 2u
#define BOARD_PROTOCOL_MAX_PAYLOAD_SIZE      BOARD_PROTOCOL_V2_PAYLOAD_SIZE
#define BOARD_PROTOCOL_MAX_FRAME_SIZE        (9u + BOARD_PROTOCOL_MAX_PAYLOAD_SIZE)

typedef struct
{
    float w;
    float x;
    float y;
    float z;
} BoardProtocol_Quaternion;

_Static_assert(sizeof(float) == 4u, "protocol V1 requires 32-bit float");

uint16_t BoardProtocol_Crc16CcittFalse(const uint8_t *data, uint16_t length);

/* Returns encoded frame length, or 0 for an invalid argument/capacity. */
size_t BoardProtocol_EncodeV1(uint16_t sequence,
                              const BoardProtocol_Quaternion *quaternion,
                              uint8_t *frame,
                              size_t capacity);

size_t BoardProtocol_EncodeV2(uint16_t sequence,
                              uint32_t sample_time_ms,
                              const BoardProtocol_Quaternion *quaternion,
                              uint8_t *frame,
                              size_t capacity);

/* Host-to-slave NACK: type 0x81; requested sequence appears both in the
 * frame sequence field and as a 2-byte little-endian payload. */
size_t BoardProtocol_EncodeRetransmit(uint8_t version,
                                      uint16_t requested_sequence,
                                      uint8_t *frame,
                                      size_t capacity);

#endif

#include "board_link.h"
#include <string.h>

#define RX_RING_SIZE 128u
#define FIXED_HEADER_SIZE 5u

typedef struct
{
    uint8_t frame[BOARD_PROTOCOL_MAX_FRAME_SIZE];
    uint8_t index;
    uint8_t expected_length;
} FrameParser;

static volatile uint8_t rx_ring[RX_RING_SIZE];
static volatile uint8_t rx_write;
static volatile uint8_t rx_read;
static FrameParser parser;
static BoardLink_Attitude latest_attitude;
static bool latest_ready;
static uint16_t control_sequence;

static uint32_t gpio_clock_for_port(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    return 0u;
}

static void usart1_init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                        GPIO_TypeDef *rx_port, uint16_t rx_pin,
                        FunctionalState remap_usart1)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;
    uint32_t gpio_clocks;

    gpio_clocks = gpio_clock_for_port(tx_port) | gpio_clock_for_port(rx_port);
    RCC_APB2PeriphClockCmd(gpio_clocks | RCC_APB2Periph_USART1 |
                           RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART1, remap_usart1);

    gpio.GPIO_Pin = tx_pin;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(tx_port, &gpio);
    gpio.GPIO_Pin = rx_pin;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(rx_port, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = 460800u;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

/* 发送重传请求；调用发生在主循环，不在中断服务程序中等待发送。 */
static void request_retransmit(uint16_t sequence)
{
    uint8_t frame[BOARD_PROTOCOL_MAX_FRAME_SIZE];
    size_t length;
    size_t i;

    length = BoardProtocol_EncodeRetransmit(sequence, frame, sizeof(frame));
    if (length == 0u)
        return;
    for (i = 0u; i < length; ++i)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) { }
        USART_SendData(USART1, frame[i]);
    }
}

static void reset_parser(void)
{
    parser.index = 0u;
    parser.expected_length = 0u;
}

static void accept_frame(void)
{
    const uint8_t *frame = parser.frame;
    uint8_t payload_length = frame[6];
    uint16_t sequence = BoardProtocol_ReadU16Le(&frame[4]);
    uint32_t component;
    float quaternion[4];
    uint8_t i;

    if (frame[2] != BOARD_PROTOCOL_VERSION ||
        frame[3] != BOARD_PROTOCOL_TYPE_QUATERNION ||
        payload_length != BOARD_PROTOCOL_QUATERNION_SIZE)
    {
        return;
    }

    latest_attitude.sequence = sequence;
    latest_attitude.sample_time_ms = BoardProtocol_ReadU32Le(&frame[7]);
    for (i = 0u; i < 4u; ++i)
    {
        component = BoardProtocol_ReadU32Le(&frame[11u + 4u * i]);
        quaternion[i] = (float)(int32_t)component / 1073741824.0f;
    }
    latest_attitude.quaternion.w = quaternion[0];
    latest_attitude.quaternion.x = quaternion[1];
    latest_attitude.quaternion.y = quaternion[2];
    latest_attitude.quaternion.z = quaternion[3];
    memcpy(latest_attitude.raw_frame, frame, BOARD_PROTOCOL_MAX_FRAME_SIZE);
    latest_ready = true;
}

static void parse_byte(uint8_t byte)
{
    uint8_t payload_length;
    uint16_t calculated_crc;
    uint16_t received_crc;

    if (parser.index == 0u)
    {
        if (byte == BOARD_PROTOCOL_SOF0)
            parser.frame[parser.index++] = byte;
        return;
    }
    if (parser.index == 1u)
    {
        if (byte == BOARD_PROTOCOL_SOF1)
            parser.frame[parser.index++] = byte;
        else
        {
            reset_parser();
            if (byte == BOARD_PROTOCOL_SOF0)
                parser.frame[parser.index++] = byte;
        }
        return;
    }

    parser.frame[parser.index++] = byte;
    if (parser.index == 7u)
    {
        payload_length = parser.frame[6];
        if (payload_length > BOARD_PROTOCOL_MAX_PAYLOAD_SIZE)
        {
            reset_parser();
            return;
        }
        parser.expected_length = (uint8_t)(9u + payload_length);
    }

    if (parser.expected_length != 0u && parser.index == parser.expected_length)
    {
        payload_length = parser.frame[6];
        calculated_crc = BoardProtocol_Crc16CcittFalse(
            &parser.frame[2], (uint16_t)(FIXED_HEADER_SIZE + payload_length));
        received_crc = BoardProtocol_ReadU16Le(&parser.frame[7u + payload_length]);
        if (calculated_crc == received_crc)
        {
            accept_frame();
        }
        else
        {
            if (parser.frame[2] == BOARD_PROTOCOL_VERSION &&
                parser.frame[3] == BOARD_PROTOCOL_TYPE_QUATERNION &&
                payload_length == BOARD_PROTOCOL_QUATERNION_SIZE)
                request_retransmit(BoardProtocol_ReadU16Le(&parser.frame[4]));
        }
        reset_parser();
    }
}

void BoardLink_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart1)
{
    rx_write = 0u;
    rx_read = 0u;
    latest_ready = false;
    control_sequence = 0u;
    reset_parser();
    usart1_init(tx_port, tx_pin, rx_port, rx_pin, remap_usart1);
}

int BoardLink_SendFilterWeight(uint16_t gyro_weight_q15)
{
    uint8_t frame[BOARD_PROTOCOL_MAX_FRAME_SIZE];
    size_t length;
    size_t i;

    length = BoardProtocol_EncodeFilterWeight(control_sequence++,
                                               gyro_weight_q15,
                                               frame, sizeof(frame));
    if (length == 0u)
        return -1;
    for (i = 0u; i < length; ++i)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) { }
        USART_SendData(USART1, frame[i]);
    }
    return 0;
}

void BoardLink_Process(void)
{
    uint8_t byte;
    /* 每次最多交付一帧，避免连收两帧时后一帧覆盖尚未转发的前一帧。 */
    while (rx_read != rx_write && !latest_ready)
    {
        byte = rx_ring[rx_read];
        rx_read = (uint8_t)((rx_read + 1u) % RX_RING_SIZE);
        parse_byte(byte);
    }
}

bool BoardLink_GetLatest(BoardLink_Attitude *attitude)
{
    if (attitude == 0 || !latest_ready)
        return false;
    *attitude = latest_attitude;
    latest_ready = false;
    return true;
}

void BoardLink_RxIrqHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
        uint8_t next = (uint8_t)((rx_write + 1u) % RX_RING_SIZE);
        if (next != rx_read)
        {
            rx_ring[rx_write] = byte;
            rx_write = next;
        }
    }
}

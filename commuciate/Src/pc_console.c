#include "pc_console.h"
#include <stddef.h>

#define PC_RX_RING_SIZE 64u
#define PC_COMMAND_SIZE 24u

static volatile uint8_t rx_ring[PC_RX_RING_SIZE];
static volatile uint8_t rx_write;
static volatile uint8_t rx_read;
static char command[PC_COMMAND_SIZE];
static uint8_t command_length;
static uint16_t pending_weight_q15;
static bool weight_pending;

static uint32_t gpio_clock_for_port(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    return 0u;
}

static void usart2_init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                        GPIO_TypeDef *rx_port, uint16_t rx_pin,
                        FunctionalState remap_usart2)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;
    uint32_t gpio_clocks = gpio_clock_for_port(tx_port) |
                           gpio_clock_for_port(rx_port);

    RCC_APB2PeriphClockCmd(gpio_clocks | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART2, remap_usart2);

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
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &usart);

    nvic.NVIC_IRQChannel = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

static bool parse_filter_command(const char *text, uint16_t *weight_q15)
{
    uint32_t whole = 0u;
    uint32_t fraction = 0u;
    uint32_t scale = 1u;
    uint8_t index = 2u;
    uint8_t fraction_digits = 0u;
    bool has_digit = false;

    if (text[0] != 'K' || (text[1] != ',' && text[1] != '='))
        return false;

    if (text[index] < '0' || text[index] > '1')
        return false;
    whole = (uint32_t)(text[index++] - '0');
    if (text[index] == '.')
    {
        ++index;
        while (text[index] >= '0' && text[index] <= '9')
        {
            if (fraction_digits >= 4u)
                return false;
            fraction = fraction * 10u + (uint32_t)(text[index++] - '0');
            scale *= 10u;
            ++fraction_digits;
            has_digit = true;
        }
        if (!has_digit)
            return false;
    }
    if (text[index] != '\0' || (whole == 1u && fraction != 0u))
        return false;

    *weight_q15 = (uint16_t)((whole * scale + fraction) * 32767u / scale);
    return true;
}

static void handle_command(void)
{
    uint16_t weight;
    command[command_length] = '\0';
    if (parse_filter_command(command, &weight))
    {
        pending_weight_q15 = weight;
        weight_pending = true;
    }
    command_length = 0u;
}

void PcConsole_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart2)
{
    rx_write = 0u;
    rx_read = 0u;
    command_length = 0u;
    weight_pending = false;
    usart2_init(tx_port, tx_pin, rx_port, rx_pin, remap_usart2);
}

void PcConsole_SendAttitude(uint16_t sequence,
                            uint32_t timestamp_ms,
                            const BoardProtocol_Quaternion *quaternion)
{
    uint8_t frame[BOARD_PROTOCOL_MAX_FRAME_SIZE];
    size_t length;
    size_t i;

    length = BoardProtocol_EncodeQuaternion(sequence, timestamp_ms,
                                             quaternion, frame, sizeof(frame));
    if (length == 0u)
        return;
    for (i = 0u; i < length; ++i)
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) { }
        USART_SendData(USART2, frame[i]);
    }
}

void PcConsole_Process(void)
{
    while (rx_read != rx_write)
    {
        char byte = (char)rx_ring[rx_read];
        rx_read = (uint8_t)((rx_read + 1u) % PC_RX_RING_SIZE);
        if (byte == '\r')
            continue;
        if (byte == '\n')
        {
            handle_command();
            continue;
        }
        if (command_length < (PC_COMMAND_SIZE - 1u))
            command[command_length++] = byte;
        else
            command_length = 0u;
    }
}

bool PcConsole_TakeFilterWeight(uint16_t *gyro_weight_q15)
{
    if (gyro_weight_q15 == 0 || !weight_pending)
        return false;
    *gyro_weight_q15 = pending_weight_q15;
    weight_pending = false;
    return true;
}

void PcConsole_RxIrqHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART2);
        uint8_t next = (uint8_t)((rx_write + 1u) % PC_RX_RING_SIZE);
        if (next != rx_read)
        {
            rx_ring[rx_write] = byte;
            rx_write = next;
        }
    }
}

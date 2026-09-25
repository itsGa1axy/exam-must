#include "host_log.h"
#include "pc_console.h"
#include <stdio.h>

static volatile uint32_t uptime_ms;
static uint32_t last_report_ms;
static uint32_t last_frame_ms;
static uint32_t previous_valid_frames;
static uint16_t last_sequence;
static uint8_t has_frame;

void HostLog_Init(void)
{
    uptime_ms = 0u;
    last_report_ms = 0u;
    last_frame_ms = 0u;
    previous_valid_frames = 0u;
    last_sequence = 0u;
    has_frame = 0u;

    if (SysTick_Config(SystemCoreClock / 1000u) != 0u)
    {
        PcConsole_WriteLine("ERR,SYSTICK_INIT");
        return;
    }
    PcConsole_WriteLine("BOOT,HOST_OK,USART1=460800,USART2=460800,MODE=LOG");
}

void HostLog_Tick1ms(void)
{
    ++uptime_ms;
}

void HostLog_OnAttitude(const BoardLink_Attitude *attitude)
{
    if (attitude == 0)
        return;
    last_sequence = attitude->sequence;
    last_frame_ms = uptime_ms;
    has_frame = 1u;
}

void HostLog_Process(void)
{
    BoardLink_Stats stats;
    uint32_t now = uptime_ms;
    char line[160];
    int length;

    if ((uint32_t)(now - last_report_ms) < 1000u)
        return;
    last_report_ms = now;
    BoardLink_GetStats(&stats);

    /* rx 是累计原始字节数；hz 是本次统计间隔内通过 CRC 的帧数。 */
    length = snprintf(line, sizeof(line),
                      "STAT,ms=%lu,rx=%lu,ok=%lu,hz=%lu,crc=%lu,bad=%lu,ovf=%lu,retry=%lu,seen=%u,seq=%u,age=%lu",
                      (unsigned long)now,
                      (unsigned long)stats.rx_bytes,
                      (unsigned long)stats.valid_frames,
                      (unsigned long)(stats.valid_frames - previous_valid_frames),
                      (unsigned long)stats.crc_errors,
                      (unsigned long)stats.invalid_frames,
                      (unsigned long)stats.rx_overflows,
                      (unsigned long)stats.retransmit_requests,
                      (unsigned int)has_frame,
                      (unsigned int)last_sequence,
                      (unsigned long)(has_frame ? now - last_frame_ms : 0u));
    previous_valid_frames = stats.valid_frames;
    if (length > 0 && (size_t)length < sizeof(line))
        PcConsole_WriteLine(line);
}

void HostLog_FilterWeight(uint16_t weight_q15, int send_result)
{
    char line[48];
    int length = snprintf(line, sizeof(line), "CMD,K_Q15=%u,send=%s",
                          (unsigned int)weight_q15,
                          send_result == 0 ? "OK" : "ERR");
    if (length > 0 && (size_t)length < sizeof(line))
        PcConsole_WriteLine(line);
}

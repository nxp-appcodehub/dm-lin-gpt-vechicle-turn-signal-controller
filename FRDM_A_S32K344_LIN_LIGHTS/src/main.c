/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.1
* Build Version : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
* Copyright 2026 NXP
*
*   NXP Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 *                                        INCLUDE FILES
 ==================================================================================================*/
#include "Mcal.h"
#include "Mcu.h"
#include "Mcl.h"
#include "Icu.h"
#include "Port.h"
#include "Dio.h"
#include "CDD_Uart.h"
#include "Lpuart_Uart_Ip_Irq.h"
#include "uart_comm.h"
#include "Gpt.h"
#include "Lin_43_LPUART_FLEXIO.h"
#include "Platform.h"
#include "IntCtrl_Ip_Cfg.h"
#include "Lpuart_Uart_Ip.h"
#include "stdbool.h"
#include "OsIf.h"
#include "lin_stack_cfg.h"
#include "lin_common_api.h"

/*==================================================================================================
 *                                      DEFINES AND MACROS
 ==================================================================================================*/
#define T_LinChannel_0  ((uint8)0)

/* GPT timeout period - 500 us tick, used as the LIN stack timeout-service / schedule tick base. */
#define PIT_PERIOD      15000U

/* Command byte values sent to the slave via the MasterCommand signal. */
#define CMD_NONE   ((uint8)0x00U)
#define CMD_LEFT   ((uint8)0x01U)
#define CMD_RIGHT  ((uint8)0x02U)
#define CMD_HAZARD ((uint8)0x03U)

/* Window (ms) within which both buttons must be seen pressed to trigger hazard mode. */
#define BOTH_PRESSED_WINDOW_MS  150U

/*==================================================================================================
 *                                      GLOBAL VARIABLES
 ==================================================================================================*/
volatile int exit_code = 0;

/* Set by the WKPU/EIRQ hardware-interrupt notification (SW3, PTB19) on a button press. */
volatile boolean g_sw2Pressed = FALSE;
volatile boolean g_sw3Pressed = FALSE;

/*==================================================================================================
 *                                   LOCAL FUNCTION PROTOTYPES
 ==================================================================================================*/
static void MCU_Setup(void);
static void TMR_Init(void);
static void Lin_PHY_Init(void);
static void DelayMs(uint32 ms);
static void print_msg(const char *msg);
void        TMR_ISR(void);
void        lin_master_task(void);
void        sw2Pressed(void);
void        sw3Pressed(void);

/*==================================================================================================
 *                                       MAIN FUNCTION
 ==================================================================================================*/
int main(void)
{
    MCU_Setup();
    Uart_Init(NULL_PTR);
    TMR_Init();
    Lin_PHY_Init();
    lin_master_task();

    for (;;)
    {
        if (exit_code != 0)
        {
            break;
        }
    }
    return exit_code;
}

/*==================================================================================================
 *                                       LOCAL FUNCTIONS
 ==================================================================================================*/

/**
 * @brief Initialize the system clock and install platform interrupt handlers.
 */
static void MCU_Setup(void)
{
    Mcu_Init(&Mcu_PreCompileConfig);
    Mcu_InitClock(McuClockSettingConfig_0);
    while (Mcu_GetPllStatus() != MCU_PLL_LOCKED) {};
    Mcu_DistributePllClock();
    Mcu_SetMode(McuModeSettingConf_0);
    Port_Init(NULL_PTR);
    Platform_Init(NULL_PTR);
    Platform_InstallIrqHandler(LPUART5_IRQn, &LPUART5_LIN_IP_RxTx_IRQHandler, NULL_PTR);
    Platform_InstallIrqHandler(LPUART6_IRQn, &LPUART_UART_IP_6_IRQHandler, NULL_PTR);
    Platform_SetIrqPriority(LPUART6_IRQn, 12U);
    Platform_InstallIrqHandler(SIUL_1_IRQn, &SIUL2_EXT_IRQ_8_15_ISR, NULL_PTR);
    Platform_InstallIrqHandler(WKPU_IRQn, &WKPU_EXT_IRQ_SINGLE_ISR, NULL_PTR);
    Platform_SetIrq(LPUART6_IRQn, TRUE);
    Platform_InstallIrqHandler(PIT0_IRQn, &PIT_0_ISR, NULL_PTR);
    OsIf_Init(NULL_PTR);
    Icu_Init(&Icu_Config_VS_0);
    Icu_EnableEdgeDetection(IcuConf_IcuChannel_IcuChannel_0);
    Icu_EnableEdgeDetection(IcuConf_IcuChannel_IcuChannel_1);
    Icu_EnableNotification(IcuConf_IcuChannel_IcuChannel_0);
    Icu_EnableNotification(IcuConf_IcuChannel_IcuChannel_1);
}

/**
 * @brief Initialize the GPT timer (500 us periodic tick).
 */
static void TMR_Init(void)
{
    Gpt_Init(&Gpt_Config_VS_0);
    Gpt_StartTimer(GptConf_GptChannelConfiguration_GptChannelConfiguration_0, PIT_PERIOD);
    Gpt_EnableNotification(GptConf_GptChannelConfiguration_GptChannelConfiguration_0);
}

/**
 * @brief Initialize the LIN physical interface (MCAL driver only) and issue a wakeup pulse.
 */
static void Lin_PHY_Init(void)
{
    Lin_43_LPUART_FLEXIO_Init(&Lin_43_LPUART_FLEXIO_xConfig_VS_0);
    Lin_43_LPUART_FLEXIO_WakeupInternal(T_LinChannel_0);
    Lin_43_LPUART_FLEXIO_Wakeup(T_LinChannel_0);
}

void sw3Pressed(void)
{
	DelayMs(0);
	g_sw3Pressed = !g_sw3Pressed;
}

/**
 * @brief GPT notification callback - invoked every 500 us.
 * @details Services the LIN stack timeout counter and, every 10th tick (5 ms), advances the
 *          LIN master schedule table (NormalTable), which transmits CommMasterCmd and then
 *          requests CommSlaveStatus from the slave.
 */
void TMR_ISR(void)
{
    static uint32_t interruptCount = 0UL;

    lin_dal_timeout_service(LI0);

    if (++interruptCount > 9UL)
    {
        l_sch_tick(LI0);
        interruptCount = 0UL;
    }
}

/**
 * @brief ICU channel notification callback for the SW3 (PTB19) push button.
 * @details Registered as the "Icu_ChannelNotification" for IcuChannel_0. Sets a flag consumed
 *          by the master polling loop; the WKPU/EIRQ hardware interrupt path handles the
 *          edge detection and debounce timing entirely in hardware/ICU driver.
 */
void sw2Pressed(void)
{
    g_sw2Pressed = !g_sw2Pressed;
}

/**
 * @brief Send a null-terminated string over UART (blocking).
 */
static void print_msg(const char *msg)
{
    SendString(msg);
}

/**
 * @brief LIN master application loop - signal-based communication.
 * @details Reads two push buttons (SW2 via DIO/ICU polling, SW3 via ICU/WKPU hardware interrupt),
 *          computes a turn-signal command byte, and publishes it into the MasterCommand signal of
 *          the CommMasterCmd frame (ID 0x30). The LIN schedule (l_sch_tick in TMR_ISR) transmits
 *          CommMasterCmd and then requests CommSlaveStatus (ID 0x33) from the slave. Command
 *          encoding: 0x00 = none, 0x01 = left (green), 0x02 = right (green), 0x03 = hazard (red, both).
 */
void lin_master_task(void)
{
    boolean sw2State;
    boolean sw3State;
    uint8 cmd;
    uint8 prevCmd = 0xFFU;

    l_sys_init();
    l_ifc_init(LI0);
    l_sch_set(LI0, LI0_NormalTable, 0u);

    for (;;)
    {
        sw2State = g_sw2Pressed;
        sw3State = g_sw3Pressed;

        if (sw2State && sw3State)
        {
            cmd = CMD_HAZARD;
        }
        else if (sw2State)
        {
            cmd = CMD_LEFT;
        }
        else if (sw3State)
        {
            cmd = CMD_RIGHT;
        }
        else
        {
            cmd = CMD_NONE;
        }

        /* Publish the turn-signal command into the MasterCommand signal. The LIN schedule
         * (l_sch_tick in TMR_ISR) transmits CommMasterCmd on its next slot. */
        l_u8_wr(LI0_MasterCommand, cmd);

        if (cmd != prevCmd)
        {
            switch (cmd)
            {
                case CMD_LEFT:
                    print_msg("FRDM-A-S32K344: LEFT turn signal (green)\r\n");
                    break;
                case CMD_RIGHT:
                    print_msg("FRDM-A-S32K344: RIGHT turn signal (green)\r\n");
                    break;
                case CMD_HAZARD:
                    print_msg("FRDM-A-S32K344: HAZARD (both, red)\r\n");
                    break;
                default:
                    break;
            }
            prevCmd = cmd;
        }

        /* Consume the CommSlaveStatus response (if any) so the flag does not accumulate;
         * the slave status/error reporting is not used by this demo. */
        if (0U != (l_u8)l_flg_tst(LI0_CommSlaveStatus_flag))
        {
            l_flg_clr(LI0_CommSlaveStatus_flag);
        }

        DelayMs(100);
    }
}

/**
 * @brief Millisecond delay function using OsIf timer.
 */
static void DelayMs(uint32 ui32TimeoutMs) {
    uint32 ui32CurTime = OsIf_GetCounter(OSIF_COUNTER_SYSTEM);
    uint32 ui32ElapsedTicks = 0U;
    uint32 ui32TimeoutTicks = OsIf_MicrosToTicks(ui32TimeoutMs * 1000U,
            OSIF_COUNTER_SYSTEM);

    while (ui32ElapsedTicks < ui32TimeoutTicks) {
        ui32ElapsedTicks += OsIf_GetElapsed(&ui32CurTime, OSIF_COUNTER_SYSTEM);
    }
}

#ifdef __cplusplus
}
#endif

/** @} */

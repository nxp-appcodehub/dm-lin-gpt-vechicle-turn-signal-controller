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
#include "Port.h"
#include "Gpt.h"
#include "Lin_43_LPUART_FLEXIO.h"
#include "Platform.h"
#include "CDD_Uart.h"
#include "uart_comm.h"
#include "Lpuart_Uart_Ip.h"
#include "IntCtrl_Ip_Cfg.h"
#include "OsIf.h"
#include "Dio.h"
#include "stdbool.h"
#include "turn_signal.h"
#include "lin_stack_cfg.h"
#include "lin_common_api.h"

/*==================================================================================================
 *                                      DEFINES AND MACROS
 ==================================================================================================*/
#define T_LinChannel_0              ((uint8)0)

/* GPT timeout period - 20000 ticks =~ 500 us at configured clock; used as LED blink base tick
 * and as the LIN stack timeout-service tick. */
#define PIT_PERIOD                  20000

/* Number of 500 us ticks per blink half-period (~400 ms toggle => ~1.25 Hz blink). */
#define BLINK_HALF_PERIOD_TICKS     800U

/*==================================================================================================
 *                                      GLOBAL VARIABLES
 ==================================================================================================*/
volatile int exit_code = 0;

/*==================================================================================================
 *                                   LOCAL FUNCTION PROTOTYPES
 ==================================================================================================*/
static void MCU_Setup(void);
static void TMR_Init(void);
static void Lin_PHY_Init(void);
static void SetLed(Dio_ChannelType redCh, Dio_ChannelType greenCh, boolean red, boolean green);
void        TMR_ISR(void);
void        lin_slave_task(void);

/*==================================================================================================
 *                                       MAIN FUNCTION
 ==================================================================================================*/
int main(void)
{
    MCU_Setup();
    Uart_Init(NULL_PTR);
    TMR_Init();
    Lin_PHY_Init();

    SendString("FRDM-A-S32K358: RX ready\r\n");

    lin_slave_task();

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
    Mcu_Init(NULL_PTR);
    Mcu_InitClock(McuClockSettingConfig_0);

    while (Mcu_GetPllStatus() != MCU_PLL_LOCKED) {};

    Mcu_DistributePllClock();
    Mcu_SetMode(McuModeSettingConf_0);
    Port_Init(NULL_PTR);
    Platform_Init(NULL_PTR);
    Platform_InstallIrqHandler(LPUART12_IRQn, &LPUART12_LIN_IP_RxTx_IRQHandler, NULL_PTR);
    Platform_InstallIrqHandler(LPUART6_IRQn, &LPUART_UART_IP_6_IRQHandler, NULL_PTR);
    Platform_SetIrqPriority(LPUART6_IRQn, 12U);
    Platform_SetIrq(LPUART6_IRQn, TRUE);
    Platform_InstallIrqHandler(PIT0_IRQn, &PIT_0_ISR, NULL_PTR);
    OsIf_Init(NULL_PTR);
}

/**
 * @brief Initialize the GPT timer and enable the periodic LED-blink timebase notification.
 */
static void TMR_Init(void)
{
    Gpt_Init(&Gpt_Config_VS_0);
    Gpt_StartTimer(GptConf_GptChannelConfiguration_GptChannelConfiguration_0, PIT_PERIOD);
    Gpt_EnableNotification(GptConf_GptChannelConfiguration_GptChannelConfiguration_0);
}

/**
 * @brief Initialize the LIN physical interface (MCAL driver only, slave node) and issue a
 *        wakeup pulse.
 */
static void Lin_PHY_Init(void)
{
    Lin_43_LPUART_FLEXIO_Init(NULL_PTR);
    Lin_43_LPUART_FLEXIO_WakeupInternal(T_LinChannel_0);
    Lin_43_LPUART_FLEXIO_Wakeup(T_LinChannel_0);
}

/**
 * @brief Drive a single RGB LED's red and green channels.
 */
static void SetLed(Dio_ChannelType redCh, Dio_ChannelType greenCh, boolean red, boolean green)
{
    Dio_WriteChannel(redCh, red ? STD_HIGH : STD_LOW);
    Dio_WriteChannel(greenCh, green ? STD_HIGH : STD_LOW);
}

/**
 * @brief GPT notification callback - invoked every ~500 us.
 * @details Services the LIN stack timeout counter and acts as the LED blink timebase. Reads the
 *          turn-signal command latched by lin_slave_task() (sourced from the MasterCommand signal
 *          via the LIN stack's slave-node callbacks) and drives LED0/LED1 accordingly:
 *            NONE   -> both LEDs off
 *            LEFT   -> LED0 blinks green
 *            RIGHT  -> LED1 blinks green
 *            HAZARD -> LED0 and LED1 blink red together
 */
void TMR_ISR(void)
{
    static uint32_t tickCount = 0UL;
    static boolean blinkOn = FALSE;
    uint8 cmd;

    lin_dal_timeout_service(LI0);

    cmd = TurnSignal_GetCommand();

    tickCount++;
    if (tickCount >= BLINK_HALF_PERIOD_TICKS)
    {
        tickCount = 0UL;
        blinkOn = (blinkOn == TRUE) ? FALSE : TRUE;
    }

    switch (cmd)
    {
        case TURN_SIGNAL_CMD_LEFT:
            SetLed(DioConf_DioChannel_USRLED0_RED, DioConf_DioChannel_USRLED0_GREEN, FALSE, blinkOn);
            SetLed(DioConf_DioChannel_USRLED1_RED, DioConf_DioChannel_USRLED1_GREEN, FALSE, FALSE);
            break;

        case TURN_SIGNAL_CMD_RIGHT:
            SetLed(DioConf_DioChannel_USRLED0_RED, DioConf_DioChannel_USRLED0_GREEN, FALSE, FALSE);
            SetLed(DioConf_DioChannel_USRLED1_RED, DioConf_DioChannel_USRLED1_GREEN, FALSE, blinkOn);
            break;

        case TURN_SIGNAL_CMD_HAZARD:
            SetLed(DioConf_DioChannel_USRLED0_RED, DioConf_DioChannel_USRLED0_GREEN, blinkOn, FALSE);
            SetLed(DioConf_DioChannel_USRLED1_RED, DioConf_DioChannel_USRLED1_GREEN, blinkOn, FALSE);
            break;

        case TURN_SIGNAL_CMD_NONE:
	default:
			SetLed(DioConf_DioChannel_USRLED0_RED,
					DioConf_DioChannel_USRLED0_GREEN, FALSE, FALSE);
			SetLed(DioConf_DioChannel_USRLED1_RED,
					DioConf_DioChannel_USRLED1_GREEN, FALSE, FALSE);
		break;
	}
}

/**
 * @brief LIN slave application loop - signal-based communication.
 * @details Initializes the LIN interface, pre-loads an initial SlaveStatus value, then loops
 *          forever:
 *            1. Waits for the CommMasterCmd frame-received flag. The LIN stack's slave-node
 *               callbacks (RTD/src/LinIf.c) set LI0_CommMasterCmd_flag each time the master
 *               transmits the CommMasterCmd frame (ID 0x30) and it is successfully received.
 *            2. Reads the MasterCommand signal and latches it via TurnSignal_SetCommand() for
 *               the GPT-based LED blink task (TMR_ISR).
 *            3. Publishes a SlaveStatus reply (echoes the received command) via the SlaveStatus
 *               signal of the CommSlaveStatus frame (ID 0x33), which the stack transmits the next
 *               time the master requests it.
 */
void lin_slave_task(void)
{
    uint8 rx_cmd;

    l_sys_init();
    l_ifc_init(LI0);

    /* Pre-load a valid initial status so the first CommSlaveStatus request from the
     * master gets a defined value even before the first MasterCommand is processed. */
    l_u8_wr(LI0_SlaveStatus, TURN_SIGNAL_CMD_NONE);
    l_bool_wr(LI0_SlaveCommError, 0);

    for (;;)
    {
        /* Wait for the master to transmit CommMasterCmd and the stack to receive it.
         * The stack sets LI0_CommMasterCmd_flag on successful frame reception. */
        if (0U != (l_u8)l_flg_tst(LI0_CommMasterCmd_flag))
        {
            l_flg_clr(LI0_CommMasterCmd_flag);

            rx_cmd = l_u8_rd(LI0_MasterCommand);

            /* Latch the turn-signal command byte for the LED blink task. */
            TurnSignal_SetCommand(rx_cmd);

            /* Echo the received command back as SlaveStatus; the stack will transmit this
             * value when the master next issues a CommSlaveStatus header (ID 0x33). */
            l_u8_wr(LI0_SlaveStatus, rx_cmd);
            l_bool_wr(LI0_SlaveCommError, 0);
        }
    }
}

#ifdef __cplusplus
}
#endif

/** @} */

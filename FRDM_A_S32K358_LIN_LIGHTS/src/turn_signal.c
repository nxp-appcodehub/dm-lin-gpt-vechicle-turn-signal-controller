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
#include "turn_signal.h"

/*==================================================================================================
 *                                      GLOBAL VARIABLES
 ==================================================================================================*/

/* Latest command byte received from the LIN master. Written from the LIN driver's slave-node
 * callback context (LinIf_RxIndication) and read from the GPT notification (TMR_ISR). Both
 * contexts are interrupt handlers at the same or compatible priority in this example, and the
 * value is a single byte, so no additional locking is required. */
static volatile uint8 g_turnSignalCmd = TURN_SIGNAL_CMD_NONE;

/*==================================================================================================
 *                                       GLOBAL FUNCTIONS
 ==================================================================================================*/

void TurnSignal_SetCommand(uint8 cmd)
{
    g_turnSignalCmd = cmd;
}

uint8 TurnSignal_GetCommand(void)
{
    return g_turnSignalCmd;
}

#ifdef __cplusplus
}
#endif

/** @} */

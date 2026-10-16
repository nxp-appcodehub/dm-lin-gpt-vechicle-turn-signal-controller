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

#ifndef TURN_SIGNAL_H
#define TURN_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
 *                                        INCLUDE FILES
 ==================================================================================================*/
#include "Std_Types.h"

/*==================================================================================================
 *                                      DEFINES AND MACROS
 ==================================================================================================*/

/* Protected ID of the turn-signal command frame - must match the master's TURN_SIGNAL_PID.
 * 0x1A is used because its LIN parity bits (P0/P1) both compute to 0, so the raw ID and the
 * protected PID are the same value. */
#define TURN_SIGNAL_PID   ((uint8)0x1AU)


/* Command byte values received from the LIN master. */
#define TURN_SIGNAL_CMD_NONE   ((uint8)0x00U)
#define TURN_SIGNAL_CMD_LEFT   ((uint8)0x01U)
#define TURN_SIGNAL_CMD_RIGHT  ((uint8)0x02U)
#define TURN_SIGNAL_CMD_HAZARD ((uint8)0x03U)

/*==================================================================================================
 *                                   FUNCTION PROTOTYPES
 ==================================================================================================*/

/**
 * @brief Store the latest turn-signal command byte received from the LIN master.
 * @param cmd One of TURN_SIGNAL_CMD_NONE / _LEFT / _RIGHT / _HAZARD.
 */
void TurnSignal_SetCommand(uint8 cmd);

/**
 * @brief Retrieve the latest turn-signal command byte.
 * @return The last command set via TurnSignal_SetCommand(), or TURN_SIGNAL_CMD_NONE initially.
 */
uint8 TurnSignal_GetCommand(void);

#ifdef __cplusplus
}
#endif

#endif /* TURN_SIGNAL_H */

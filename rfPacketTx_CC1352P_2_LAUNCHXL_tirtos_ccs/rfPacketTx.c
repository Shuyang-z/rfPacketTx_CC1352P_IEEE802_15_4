/*
 * Copyright (c) 2019, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/***** Includes *****/
/* Standard C Libraries */
#include <stdlib.h>
#include <unistd.h>

/* TI Drivers */
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

/* Driverlib Header files */
#include DeviceFamily_constructPath(driverlib/rf_ieee_mailbox.h)

/* Board Header files */
#include "ti_drivers_config.h"
#include <ti_radio_config.h>

/***** Defines *****/

/* Do power measurement */
//#define POWER_MEASUREMENT

/* Packet TX Configuration */
#define PAYLOAD_LENGTH      30
#ifdef POWER_MEASUREMENT
#define PACKET_INTERVAL     5  /* For power measurement set packet interval to 5s */
#else
#define PACKET_INTERVAL     200000  /* Set packet interval to 500000us or 500ms */
#endif

/***** Prototypes *****/

/***** Variable declarations *****/
static RF_Object rfObject;
static RF_Handle rfHandle;

/* Pin driver handle */
static PIN_Handle ledPinHandle;
static PIN_State ledPinState;
static PIN_Handle buttonPinHandle;
static PIN_State buttonPinState;

static uint8_t packet[PAYLOAD_LENGTH];
static uint16_t seqNumber;

/*
 * Initial LED pin configuration table
 *   - LEDs CONFIG_PIN_RLED is off.
 *   - LEDs CONFIG_PIN_GLED is off.
 */
PIN_Config ledPinTable[] = {
    CONFIG_PIN_RLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    CONFIG_PIN_GLED | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

/*
 * Application button pin configuration table:
 */
PIN_Config buttonPinTable[] = {
    CONFIG_PIN_BUTTON_0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_DIS,
    CONFIG_PIN_BUTTON_1  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_DIS,
    PIN_TERMINATE
};

/***** Function definitions *****/

void *mainThread(void *arg0)
{
    RF_Params rfParams;
    RF_Params_init(&rfParams);

    RF_ScheduleCmdParams scheduleParams;
    RF_ScheduleCmdParams_init(&scheduleParams);

    /* Open LED pins */
    ledPinHandle = PIN_open(&ledPinState, ledPinTable);
    if (ledPinHandle == NULL)
    {
        while(1);
    }

    buttonPinHandle = PIN_open(&buttonPinState, buttonPinTable);
    if(!buttonPinHandle) {
        /* Error initializing button pins */
        while(1);
    }

    /* =========== Populated parameters =========== */
    scheduleParams.startTime    = 0;
    scheduleParams.startType    = RF_StartNotSpecified;
    scheduleParams.allowDelay   = RF_AllowDelayAny;
    scheduleParams.duration     = ~(0); // The CMD_FS will run until done
    scheduleParams.endTime      = ~(0); // The CMD_FS will run until done
    scheduleParams.endType      = RF_EndNotSpecified;
    /* ============================================= */

    RF_cmdIeeeTx_ieee154.payloadLen = PAYLOAD_LENGTH;
    RF_cmdIeeeTx_ieee154.pPayload = packet;
    RF_cmdIeeeTx_ieee154.startTrigger.triggerType = TRIG_NOW;

    /* Set Tx Power: 0dBm - 20dBm */
    int8_t txPower = 0;
    uint8_t leftButtonPressed = 0;
    uint8_t rightButtonPressed = 0;

    while(1)
    {
        /* Detect button state */
        while(PIN_getInputValue(CONFIG_PIN_BUTTON_0) && PIN_getInputValue(CONFIG_PIN_BUTTON_1)) ;  // Wait for press button
        usleep(10000);  // Debounce

        while((PIN_getInputValue(CONFIG_PIN_BUTTON_0) == 0) || (PIN_getInputValue(CONFIG_PIN_BUTTON_1) == 0))  // Wait for release
        {
            if(PIN_getInputValue(CONFIG_PIN_BUTTON_0) == 0)  // Left button pressed
            {
                leftButtonPressed = 1;
                PIN_setOutputValue(ledPinHandle, CONFIG_PIN_RLED, 1);
            }
            if(PIN_getInputValue(CONFIG_PIN_BUTTON_1) == 0)  // Right button pressed
            {
                rightButtonPressed = 1;
                PIN_setOutputValue(ledPinHandle, CONFIG_PIN_GLED, 1);
            }
        }

        if(leftButtonPressed)
        {
            if(txPower > 0)
            {
                if(txPower == 14)
                {
                    txPower = 10;
                }
                else
                {
                    txPower = txPower - 1;
                }
            }
        }
        else if(rightButtonPressed)
        {
            if(txPower < 20)
            {
                if(txPower == 10)
                {
                    txPower = 14;
                }
                else
                {
                    txPower = txPower + 1;
                }
            }
        }

        leftButtonPressed = 0;
        rightButtonPressed = 0;
        PIN_setOutputValue(ledPinHandle, CONFIG_PIN_RLED, 0);
        PIN_setOutputValue(ledPinHandle, CONFIG_PIN_GLED, 0);

        /* Request access to the radio */
        rfHandle = RF_open(&rfObject, &RF_prop_ieee154, (RF_RadioSetup*)&RF_cmdRadioSetup_ieee154, &rfParams);

        RF_setTxPower(rfHandle, RF_TxPowerTable_findValue(txPowerTable_2400_pa5_20, txPower));

        /* Set the frequency */
        RF_runScheduleCmd(rfHandle, (RF_Op*)&RF_cmdFs_ieee154, &scheduleParams, NULL, 0);

        uint8_t packetIdx;
        for(packetIdx = 0; packetIdx < 10; packetIdx++)
        {
            /* Create packet with incrementing sequence number and random payload */
            packet[0] = (uint8_t)(seqNumber >> 8);
            packet[1] = (uint8_t)(seqNumber++);
            uint8_t i;
            for (i = 2; i < PAYLOAD_LENGTH; i++)
            {
                packet[i] = (uint8_t)txPower;
            }

            /* Send packet */
            RF_EventMask terminationReason = RF_runScheduleCmd(rfHandle, (RF_Op*)&RF_cmdIeeeTx_ieee154,
                                                               &scheduleParams, NULL, 0);


            switch(terminationReason)
            {
                case RF_EventLastCmdDone:
                    // A stand-alone radio operation command or the last radio
                    // operation command in a chain finished.
                    break;
                case RF_EventCmdCancelled:
                    // Command cancelled before it was started; it can be caused
                // by RF_cancelCmd() or RF_flushCmd().
                    break;
                case RF_EventCmdAborted:
                    // Abrupt command termination caused by RF_cancelCmd() or
                    // RF_flushCmd().
                    break;
                case RF_EventCmdStopped:
                    // Graceful command termination caused by RF_cancelCmd() or
                    // RF_flushCmd().
                    break;
                case RF_EventLastFGCmdDone:
                    // Graceful command termination caused by RF_cancelCmd() or
                    // RF_flushCmd().
                    break;
                default:
                    // Uncaught error event
                    while(1);
            }

            uint32_t cmdStatus = ((volatile RF_Op*)&RF_cmdIeeeTx_ieee154)->status;
            switch(cmdStatus)
            {
                case IEEE_DONE_OK:
                    // Packet transmitted successfully
                    break;
                case IEEE_DONE_STOPPED:
                    // received CMD_STOP while transmitting packet and finished
                    // transmitting packet
                    break;
                case IEEE_DONE_ABORT:
                    // Received CMD_ABORT while transmitting packet
                    break;
                case IEEE_ERROR_PAR:
                    // Observed illegal parameter
                    break;
                case IEEE_ERROR_NO_SETUP:
                    // Command sent without setting up the radio in a supported
                    // mode using CMD_PROP_RADIO_SETUP or CMD_RADIO_SETUP
                    break;
                case IEEE_ERROR_NO_FS:
                    // Command sent without the synthesizer being programmed
                    break;
                case IEEE_ERROR_TXUNF:
                    // TX underflow observed during operation
                    break;
                default:
                    // Uncaught error event - these could come from the
                    // pool of states defined in rf_mailbox.h
                    while(1);
            }

    #ifndef POWER_MEASUREMENT
            PIN_setOutputValue(ledPinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));
    #endif
            /* Power down the radio */
            RF_yield(rfHandle);

    #ifdef POWER_MEASUREMENT
            /* Sleep for PACKET_INTERVAL s */
            sleep(PACKET_INTERVAL);
    #else
            /* Sleep for PACKET_INTERVAL us */
            usleep(PACKET_INTERVAL);
    #endif
        }
        RF_close(rfHandle);
    }
}

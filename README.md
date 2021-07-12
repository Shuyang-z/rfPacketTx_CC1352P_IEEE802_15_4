# rfPacketTx_CC1352P_IEEE802_15_4
Send IEEE802.15.4 packets with selected TX power

This project is based on rfPacketTx_CC1352P_2_LAUNCHXL_tirtos_ccs example from simplelink_cc13x2_26x2_sdk_5_10_00_48

## Usage:
- RF driver is initialized as IEEE 802.15.4 mode and TX power = 0dBm, default channel is 13
- Use two buttons on LaunchPad to switch TX power, left button is decreasing by -1dBm, right button is increasing by +1dBm
- By pressing the button, the LaunchPad will send 10 IEEE 802.15 packets with the payload displaying the TX power:
11:42:15.872 | 1e | 0000 | 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01  |  -59
- TX power is limited by the power table in ti_drivers_config.c
- Using button to switch TX power only supports 0 - 20dBm now

## Modifications:
- Modify RF driver to send IEEE 802.15.4 (Zigbee) packets
- Use RF_setTxPower() API to set TX power
- Use buttons on LaunchPad to switch TX power from 0-20dBm (below 0dBm is not supported yet)
- syscfg is disabled to make changes in ti_drivers_config.c/.h

## Changes from original project:
- ti_drivers_config.c/.h: Initilize RF driver for IEEE 802.15.4 packets and TX power = 20dBm
- ti_drivers_config.h: Add LED and button initilization
- rfPacketTx.c: use LaunchPad buttons to switch TX power and send IEEE 802.15.4 packets

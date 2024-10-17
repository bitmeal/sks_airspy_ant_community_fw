#pragma once

int start_ant_device(void);

#define HRM_TX_CHANNEL_NUM 0
#define HRM_TX_NETWORK_NUM 0
#define HRM_TX_CHAN_ID_DEV_NUM 49
#define HRM_TX_CHAN_ID_TRANS_TYPE 1
#define HRM_TX_HW_VERSION 5
#define HRM_TX_MFG_ID 2
#define HRM_TX_MODEL_NUM 2
#define HRM_TX_SW_VERSION 0
#define HRM_TX_SERIAL_NUM 43981

/* 
config HRM_TX_CHANNEL_NUM
	int "Channel number assigned to HRM profile"
	default 0

config HRM_TX_NETWORK_NUM
	int "ANT+ Network number"
	default 0
	help
	  Selects the network number to associate with the channel.

config HRM_TX_CHAN_ID_DEV_NUM
	int "Channel ID: Device number"
	default 49
	range 0 65535

config HRM_TX_CHAN_ID_TRANS_TYPE
	int "Channel ID: Transmission type"
	default 1
	range 0 255

endmenu

menu "Product information"

config HRM_TX_HW_VERSION
	int "Hardware revision"
	default 5
	range 0 255

config HRM_TX_MFG_ID
	int "Manufacturer ID"
	default 2
	range 0 255

config HRM_TX_MODEL_NUM
	int "Model number"
	default 2
	range 0 255

config HRM_TX_SW_VERSION
	int "Software version"
	default 0
	range 0 255

config HRM_TX_SERIAL_NUM
	int "Serial number"
	default 43981
	range 0 65535
 */

#define CONFIG_SIMULATOR_MIN 60
#define CONFIG_SIMULATOR_MAX 180
#define CONFIG_SIMULATOR_INCREMENT 2

/* 
config SIMULATOR_MIN
	int "Minimal heart rate"
	default 60
	range 0 220

config SIMULATOR_MAX
	int "Maximal heart rate"
	default 180
	range 0 220

config SIMULATOR_INCREMENT
	int "Heart rate increment"
	default 2
	range 1 5
 */
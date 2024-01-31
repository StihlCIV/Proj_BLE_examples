/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/settings/settings.h>


/* Maximum supported AD data length, use a value supported by the Controller,
 * Bluetooth Core Specification define minimum of 31 bytes will be supported by
 * all Controllers, can be a maximum of 1650 bytes when supported.
 */
#if defined(CONFIG_BT_CTLR_ADV_DATA_LEN_MAX)
#define BT_AD_DATA_LEN_MAX CONFIG_BT_CTLR_ADV_DATA_LEN_MAX
#else
#define BT_AD_DATA_LEN_MAX 31U
#endif


#define APPLICATION_ADV_MANU_COMPANY_ID             0x03DD /**< STIHL company ID */ 
#define APPLICATION_ADV_INCOMP_16B_UUID             0xFE43 /**< STIHL SUOTA service */ 
#define APPLICATION_SW_VERSION_LOW                   06 
#define APPLICATION_SW_VERSION_HIGH                  01 
#define APPLICATION_PROTOCOL_ID                      0x06
#define APPLICATION_PRODUCT_ID                       0x05
#define ADV_PROTOCOL_ID_POS                          11
#define ADV_PRODUCT_ID_POS                           12
#define ADV_SERIAL_NUMBER_LOW_POS                    13
#define ADV_BMS_INFO_1_POS                           18
#define ADV_BMS_INFO_2_POS                           19
#define ADV_RUNTIME_DISCHARGE_COUNTER_LOW_POS        20
#define ADV_STORAGE_INFO_POS                         24
#define ADV_BATTERY_CONNECTOR_INFO_1_POS             25
#define ADV_LATEST_TOOL_ID_1_POS                     26
#define ADV_LATEST_TOOL_ID_2_POS                     27
#define ADV_SOC_POS                                  28
#define ADV_BC_SW_VERSION_LOW_POS                    29
#define ADV_BC_SW_VERSION_HIGH_POS                   30

/* Static advertising data */
#define ADV_PROTOCOL_ID_DAT                          0x06
#define ADV_BC_SW_VERSION_LOW_DAT                    APPLICATION_SW_VERSION_LOW
#define ADV_BC_SW_VERSION_HIGH_DAT                   APPLICATION_SW_VERSION_HIGH
#define ADV_INTERVAL_FAST                            APPLICATION_ADV_INTERVAL_FAST
#define ADV_INTERVAL_SLOW                            APPLICATION_ADV_INTERVAL_SLOW
#define SHORT_KEY_EVENT_TIMEOUT_S                    15U

#define FLASH_HW_VERSION_DATA                        0x10001084  /**< Address of HW version data 32bit aligned */
#define HW_VERSION_LOW_BYTE_POS                      2U
#define HW_VERSION_HIGH_BYTE_POS                     3U


/* Size of AD data format length field in octets */
#define BT_AD_DATA_FORMAT_LEN_SIZE 1U

/* Size of AD data format type field in octets */
#define BT_AD_DATA_FORMAT_TYPE_SIZE 1U

/* Maximum value of AD data format length field (8-bit) */
#define BT_AD_DATA_FORMAT_LEN_MAX 255U

/* Device name length, size minus one null character */
#define BT_DEVICE_NAME_LEN (sizeof(CONFIG_BT_DEVICE_NAME) - 1U)

/* Device name length in AD data format, 2 bytes for length and type overhead */
#define BT_DEVICE_NAME_AD_DATA_LEN (BT_AD_DATA_FORMAT_LEN_SIZE + BT_AD_DATA_FORMAT_TYPE_SIZE + BT_DEVICE_NAME_LEN)

/* Maximum manufacturer data length, considering ad data format overhead and
 * the included device name in ad data format.
 */
#define BT_MFG_DATA_LEN_MAX       (MIN((BT_AD_DATA_FORMAT_LEN_MAX - BT_AD_DATA_FORMAT_TYPE_SIZE), (BT_AD_DATA_LEN_MAX -	BT_AD_DATA_FORMAT_LEN_SIZE - BT_AD_DATA_FORMAT_TYPE_SIZE)))
#define BT_MFG_DATA_LEN           (MIN(BT_MFG_DATA_LEN_MAX, (BT_AD_DATA_LEN_MAX - BT_AD_DATA_FORMAT_LEN_SIZE - BT_AD_DATA_FORMAT_TYPE_SIZE - BT_DEVICE_NAME_AD_DATA_LEN)))


static uint8_t mfg_data[BT_MFG_DATA_LEN] = { 0xFF, 0xEE, 0xf1,0xef,0xf2,0xf5,0xf9,0xfd,0x00,0x04,0x09,
											0x0b,0x0f,0x12,0x15,0x17,0x18,0x1a,0x1a,0x1a,0x19,0x17,0x16,
											0x14,0x11,0x10,0x0e,0x0c,0x0a,0x08,0x06,0x05,0x03,0x01,0x00,
											0xfd,0xfc,0xfa,0xf9,0xf7,0xf5,0xf4,0xf3,0xf1,0xef,0xf3,0xf6,
											0xfa,0xfd,0x02,0x06,0x09,0x0e,0x10,0x14,0x14,0x17,0x19,0x19,
											0x1a,0x1a,0x19,0x17,0x15,0x13,0x11,0x0f,0x0d,0x0c,0x09,0x08,
											0x06,0x03,0x02,0x00,0xff,0xfd,0xfb,0xf9,0xf8,0xf6,0xf5,0xf3,
											0xf2,0xf0,0xf0,0xf4,0xf8,0xfc,0x00,0x04,0x07,0x0b,0x0e,0x11,
											0x13,0x16,0x18,0x19,0x1a,0x1b,0x19,0x18,0x17,0x15,0x13,0x11,
											0x0f,0x0d,0x0b,0x09,0x07,0x05,0x03,0x02,0x00,0xfe,0xfd,0xfb,
											0xf9,0xf8,0xf6,0xf5,0xf3,0xf1,0xf0,0xf2,0xf5,0xf9,0xfd,0x00,
											0x05,0x08,0x0b,0x10,0x12,0x15,0x17,0x19,0x19,0x1a,0x1a,0x19,
											0x18,0x16,0x14,0x12,0x10,0x0e,0x0c,0x0a,0x09,0x07};

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

static uint8_t manufacture_data[40] = {0};

static void Adv_handler_set_mdata(void);

static struct bt_le_ext_adv *adv;

int broadcaster_multiple(void)
{
	struct bt_le_adv_param adv_param = {
		.id = BT_ID_DEFAULT,
		.sid = 0U, /* Supply unique SID when creating advertising set */
		.secondary_max_skip = 0U,
		.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME),
		.interval_min = 1600,
		.interval_max = 1602,
		.peer = NULL,
	};
    Adv_handler_set_mdata();
	/* Initialize the Bluetooth Subsystem */
    memcpy(mfg_data, manufacture_data, 40);
	bt_enable(NULL);

	settings_load();
	/* Create a non-connectable non-scannable advertising set */
	bt_le_ext_adv_create(&adv_param, NULL, &adv);

	/* Set extended advertising data */
	bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);

	/* Start extended advertising set */
	bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);

	printk("Started Extended Advertising.\n");

	return 0;
}


static void Adv_handler_set_mdata(void)
{
    printk("Adv_handler_set_mdata entered..\n");
    bt_addr_le_t macAddr[CONFIG_BT_ID_MAX];
    size_t count = ARRAY_SIZE(macAddr);

    /* getting MAC addr. */
    bt_id_get(macAddr, &count);


    /* Hardcoding manufacture Data to the Adv data structure */
    manufacture_data[0] = (uint8_t) (APPLICATION_ADV_MANU_COMPANY_ID);      //§Byte 9  //  company identifier low byte
    manufacture_data[1] = (uint8_t) (APPLICATION_ADV_MANU_COMPANY_ID >> 8); //§Byte 10 //  company identifier high byte

    manufacture_data[2] = (uint8_t) APPLICATION_PROTOCOL_ID;                //§Byte 11 // protocol identifier
    manufacture_data[3] = (uint8_t) APPLICATION_PRODUCT_ID;                 //§Byte 12 // product  identifier

    /* Battery Pack Serial Number */ //914701445--> 0x36853C85 --> 0x85, 0x3C, 0x85, 0x36
    /* fake Battery Pack Serial Number */ //914701445--> 0x368591A2 --> 0xA2, 0x91, 0x85, 0x36
    
    manufacture_data[4] = (uint8_t) 0x85;                                   //§Byte 13 // serial number - lowest byte
    manufacture_data[5] = (uint8_t) 0x3C;                                   //§Byte 14 // serial number 
    manufacture_data[6] = (uint8_t) 0x85;                                   //§Byte 15 // serial number
    manufacture_data[7] = (uint8_t) 0x36;                                   //§Byte 16 // serial number
    manufacture_data[8] = (uint8_t) 0x00;                                   //§Byte 17 // serial number - highest byte

    // Test function to change and test BMS Operation Mode using Button-4
    // uint8_t bms_mode = Test_getBMSMode();
    // Test bms different modes of operation set via Button-4 press
    // manufacture_data[9]  = (uint8_t) bms_mode;                            //§Byte 18 // BMS Operation Mode 

    manufacture_data[9]  = (uint8_t) 0x02;                                   //§Byte 18 // BMS Operation Mode 
    manufacture_data[10] = (uint8_t) 0x60;                                   //§Byte 19 // State of Health (set to 100% for test purposes)
    manufacture_data[11] = (uint8_t) 0x00;                                   //§Byte 20 // |
    manufacture_data[12] = (uint8_t) 0x00;                                   //§Byte 21 // | BATTERY Runtime
    manufacture_data[13] = (uint8_t) 0x00;                                   //§Byte 22 // | discharge counter value
    manufacture_data[14] = (uint8_t) 0x00;                                   //§Byte 23 // | 

    manufacture_data[15] = (uint8_t) 0x00;                                   //§Byte 24 // Battery History data
    manufacture_data[16] = (uint8_t) 0x00;                                   //§Byte 25 // Battery Connector Status Data
    manufacture_data[17] = (uint8_t) 0xB4;                                   //§Byte 26 // Latest Tool ID 1
    manufacture_data[18] = (uint8_t) 0x00;                                   //§Byte 27 // Latest Tool ID 2
    manufacture_data[19] = (uint8_t) 0x64;                                   //§Byte 28 // SoC - State of Charge ( set to 100% for test purposes)
    manufacture_data[20] = (uint8_t) 0xff;             //§Byte 29 // BC SW Version Low  Byte
    manufacture_data[21] = (uint8_t) 0xdd;            //§Byte 30 // BC SW Version High Byte
}
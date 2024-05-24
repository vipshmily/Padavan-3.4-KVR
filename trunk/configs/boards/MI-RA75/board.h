/* Xiaomi Mi Range Extender AC1200 (RA75)*/

#define BOARD_PID		"MI-RA75"
#define BOARD_NAME		"MI-RA75"
#define BOARD_DESC		"Xiaomi Mi Range Extender AC1200"
#define BOARD_VENDOR_NAME	"Beijing Xiaomi Technology Co., Ltd."
#define BOARD_VENDOR_URL	"https://www.mi.com/"
#define BOARD_MODEL_URL		"https://www.mi.com/global/product/mi-wifi-range-extender-ac1200"
#define BOARD_BOOT_TIME		25
#define BOARD_FLASH_TIME	120
#define BOARD_GPIO_BTN_RESET	38
#define BOARD_GPIO_BTN_WPS	67
#undef  BOARD_GPIO_LED_ALL
#define BOARD_GPIO_LED_WIFI 	44	/* 44: blue, 37: amber, 46: red */
#define BOARD_GPIO_LED_POWER 	0 	/* 0: blue, 2: amber */
#undef  BOARD_GPIO_LED_LAN
#undef  BOARD_GPIO_LED_WAN
#define BOARD_HAS_5G_11AC	1
#define BOARD_NUM_ANT_5G_TX	2
#define BOARD_NUM_ANT_5G_RX	2
#define BOARD_NUM_ANT_2G_TX	2
#define BOARD_NUM_ANT_2G_RX	2
#define BOARD_NUM_ETH_LEDS	0
#define BOARD_NUM_ETH_EPHY	1
#define BOARD_HAS_EPHY_L1000	0
#define BOARD_HAS_EPHY_W1000	0
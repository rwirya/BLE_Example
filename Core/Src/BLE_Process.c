
/**
  **************************************************************************************************
  * @file       : BLE_Process.c
  * @brief      : This file contains all functions related to initializing the Bluetooth Low
	*								Energy (BLE) Stack of the BlueNRG. Refer to the BlueNRGx Programming Manual found
	*								in ST's website. 
  * @author			: 
  **************************************************************************************************
  */
  
  
/* Includes --------------------------------------------------------------------------------------*/
#include "hci.h"
#include "hci_tl.h"
#include "BLE_Process.h"

#include "bluenrg1_hal_aci.h"
#include "bluenrg1_gatt_aci.h"
#include "bluenrg1_gap_aci.h"
#include "bluenrg1_hci_le.h"


/* Private includes ------------------------------------------------------------------------------*/
#include "bluenrg_conf.h"				/* Contains configured Bluetooth Parameters in CubeMX */


/* External variables ----------------------------------------------------------------------------*/
extern UART_HandleTypeDef huart1;


/* Private typedef -------------------------------------------------------------------------------*/
typedef struct 
{
	uint8_t BLE_Client_Addr[6];
	uint8_t deviceRole;						// deviceRole should be 0x01 when connected (indicating slave)
	uint16_t connectionhandle;		// Handle to connection between the GATT central and GATT peripheral
	uint16_t BLE_ConnInterval;				// Timing parameters of BLE 
	uint16_t BLE_ConnLatency;					// Timing parameters of BLE 
	uint16_t BLE_SupervisionTimeout;	// Timing parameters of BLE 
	BLE_State_t ConnectionStatus;	// Connection status, will be used in FSM
} connectionStatus_t;


/* Private define --------------------------------------------------------------------------------*/


/* Private variables -----------------------------------------------------------------------------*/
uint16_t discovery_time 			= 0;

static uint16_t hGAPService;
static uint16_t hDevNameChar;
static uint16_t hAppearanceChar;

/* Variables that will hold service and characteristic UUIDs */
Service_UUID_t suuid_object;
Char_UUID_t char_obj_1, char_obj_2, char_obj_3, char_obj_4;
	
/* Handle to services and associated characteristics */
static uint16_t hService;
static uint16_t hClientIndicate;
static uint16_t hClientNotification;
static uint16_t hClientREAD;
static uint16_t hClientWRITE;

/* Handle to associated characteristic descriptors */
static uint16_t hFirstCharDesc;
static uint16_t hSecondCharDesc;
static uint16_t hThirdCharDesc;
static uint16_t hFourthCharDesc;

/* DISCOVERY/CONNECTIVITY DETAILS */
static connectionStatus_t Conn_Details;


/* Private macro ---------------------------------------------------------------------------------*/


/* Private function prototypes -------------------------------------------------------------------*/
static void Setup_DeviceAddress(void);
static void GAP_Peripheral_ConfigService(void);
static void Server_ResetConnectionStatus(void);


/***************************** BLE Stack and Interface Initialization  **********************************/

/**
  * @brief	Main initialization function. To be called at system startup
  * @note		Initializes BlueNRG-2 SPI Interface, HCI application, GAP and GATT layers
  */
void BlueNRG_Init(void)
{
	uint8_t ret;
	
	/* Initialize SPI1 Peripheral and Bluetooth Host Controller Interface */
	hci_init(APP_UserEvtRx, NULL);
	
	/* SAMPLEAPP EXAMPLE - Software reset of device and required 2 second delay */
	hci_reset();
	HAL_Delay(2000);
	
	/* Configure transmit power to high power at -2dBm */
	ret = aci_hal_set_tx_power_level(1, 4);
	if(ret != BLE_STATUS_SUCCESS)
	{
		(void)strncpy(pText, "Error at Power Level Config\r\n", TEXTSIZE);
		HAL_UART_Transmit(&huart1, (uint8_t*)pText, TEXTSIZE, UART_TIMEOUT);
		while(1);
	}
	
	/* Configure BLE device public address if it will be used */
	Setup_DeviceAddress();
	
	/* Initialize BLE GATT layer */
	ret = aci_gatt_init();
	if(ret != BLE_STATUS_SUCCESS)
	{
		(void)strncpy(pText, "Error at GATT init\r\n", TEXTSIZE);
		HAL_UART_Transmit(&huart1, (uint8_t*)pText, TEXTSIZE, UART_TIMEOUT);
		while(1);
	}
	
#if defined(ENABLE_SM)
	
	/* Configure proper security I/O capability and authentication requirement */
	aci_gap_set_io_capability();
	aci_gap_set_authentication_requirement();
	
#endif
	
#if defined(DEVICE_TYPE_GAP_PERIPHERAL)

	/* Initialize BLE GAP layer with the following characteristics:
	 * 		+ role = 0x01 -> Eyewear is Peripheral device
	 *		+ privacy_enabled = 0x00 -> Privacy disabled
	 *		+ device_name_char_len = 
   */
	aci_gap_init(GAP_PERIPHERAL_ROLE, GAP_PRIVACY_DISABLED, 0x08, &hGAPService, &hDevNameChar, &hAppearanceChar);
	
	/* Configure further the services and characteristics to be included in the GATT database */
	GAP_Peripheral_ConfigService();
	
	Server_ResetConnectionStatus();
	
#elif defined(DEVICE_TYPE_GAP_CENTRAL)
	
	/* Initialize BLE GAP layer with the following characteristics:
	 * 		+ role = 0x04 -> Dongle is Central device
	 *		+ privacy_enabled = 0x00 -> Privacy disabled
	 *		+ device_name_char_len = 
   */	
	aci_gap_init(GAP_CENTRAL_ROLE, GAP_PRIVACY_DISABLED, 0x08, &hGAPService, &hDevNameChar, &hAppearanceChar);
	
#endif

}

/**
  * @brief 	Sets up the device MAC address (first 3 bytes are fixed, while the last 3 bytes are randomized).
  * @note		This MAC address will only be used to connect with other (Central devices). Central devices 
  *					will see this MAC address and use it to connect with this peripheral device. Peripheral will
  *					include the MAC address in the advertisement data.
  */
static void Setup_DeviceAddress(void)
{
	tBleStatus ret;
  uint8_t bdaddr[] = {0x00, 0x00, 0x00, 0xE1, 0x80, 0x02};
  uint8_t random_number[8];

  /* get a random number from BlueNRG */
  ret = hci_le_rand(random_number);
  if(ret != BLE_STATUS_SUCCESS)
  {
    PRINT_DBG("hci_le_rand() call failed: 0x%02x\r\n", ret);
  }

  discovery_time = 3000; /* at least 3 seconds */
  /* setup discovery time with random number */
  for (uint8_t i=0; i<8; i++)
  {
    discovery_time += (2*random_number[i]);
  }

  /* Setup last 3 bytes of public address with random number */
  bdaddr[0] = (uint8_t) (random_number[0]);
  bdaddr[1] = (uint8_t) (random_number[3]);
  bdaddr[2] = (uint8_t) (random_number[6]);

	/* Configure public MAC address (bdaddr[3:5] is company specific, while bdaddr[0:2] is device specific) */
  ret = aci_hal_write_config_data(CONFIG_DATA_PUBADDR_OFFSET, CONFIG_DATA_PUBADDR_LEN, bdaddr);
  if(ret != BLE_STATUS_SUCCESS)
  {
      PRINT_DBG("Setting BD_ADDR failed 0x%02x\r\n", ret);
  }
  else
  {
    PRINT_DBG("Public address: ");
    for (uint8_t i=5; i>0; i--)
    {
      PRINT_DBG("%02X-", bdaddr[i]);
    }
    PRINT_DBG("%02X\r\n", bdaddr[0]);
  }
	
}

/**
  * @brief	Configure Services and associated Characteristics in GATT Server
  * @note		Must be called as these characteristics are involved with data flow and data
  *					transfer via BLE protocol. Values will be exchanged through characteristics.
  */
static void GAP_Peripheral_ConfigService(void)
{
	/* 128-bit UUID Declarations for 1 Service and the 4 Characteristics underneath that Service */
		
	/* Configure 128-bit Service UUID since Sciton does not have dedicated 16-bit Service
	   UUID with Bluetooth SIG. Service UUID obtained through UUID generator.
	   UUID (uuidgenerator.net): a898328b-03f9-4d63-b11d-51505ae1ce5d */
	const uint8_t service_uuid[16] = 
	{0x5D,0xCE,0xE1,0x5A,0x50,0x51,0x1D,0xB1,0x63,0x4D,0xF9,0x03,0x8B,0x32,0x98,0xA8};
	const uint8_t char1_uuid[16] = 
	{0x96,0xF7,0x4E,0xBF,0xB3,0x8E,0xB7,0x82,0x36,0x4B,0x7E,0x8B,0x80,0xEA,0x25,0x9B};
	const uint8_t char2_uuid[16] = 
	{0x96,0xF7,0x4E,0xBF,0xB3,0x8E,0xB7,0x82,0x36,0x4B,0x7E,0x8B,0x81,0xEA,0x25,0x9B};
	const uint8_t char3_uuid[16] = 
	{0x96,0xF7,0x4E,0xBF,0xB3,0x8E,0xB7,0x82,0x36,0x4B,0x7E,0x8B,0x82,0xEA,0x25,0x9B};
	const uint8_t char4_uuid[16] = 
	{0x96,0xF7,0x4E,0xBF,0xB3,0x8E,0xB7,0x82,0x36,0x4B,0x7E,0x8B,0x83,0xEA,0x25,0x9B};	
	
	BLUENRG_memcpy(&suuid_object.Service_UUID_128, service_uuid, 16);
	
	/* Add the Bluetooth Service based on the configuration above */
	aci_gatt_add_service(UUID_TYPE_128, &suuid_object, PRIMARY_SERVICE, 20, &hService);
	
	/* Variables that will hold the four characteristics' 128-bit UUID number. 
	   The first characteristic's UUID was generated with a UUID random number generator,
	   and the subsequent characteristics' UUID were derived from that first char UUID. */
	
	
	/**
	  * @brief First Characteristic (...TBD...)
		*
		* Handle 																		: Service handle to associate it with
		* UUID type																	:	128-bits																
		* Maximum length of the characteristic value: x bytes 
		* Characteristic Properties									: CHAR_PROP_INDICATE (...TBD...)
		* Security Permissions											: None
		* GATT event mask flags											: GATT_DONT_NOTIFY_EVENTS
		* Enc_Key_Size: 0x07 - the minimum encryption key size required to read the characteristic
		* Fixed characteristic value length					: FIXED_LENGTH
		*
		* This characteristic will be used (...TBD...)
		*/	
	/* First characteristic's UUID */
	BLUENRG_memcpy(&char_obj_1.Char_UUID_128, char1_uuid, 16);
	
	/**
	  * @brief Second Characteristic (...TBD...)
		*
		* Handle 																		: Service handle to associate it with
		* UUID type																	:	128-bits																
		* Maximum length of the characteristic value: x bytes 
		* Characteristic Properties									: CHAR_PROP_WRITE (...TBD...)
		* Security Permissions											: None
		* GATT event mask flags											: GATT_DONT_NOTIFY_EVENTS
		* Enc_Key_Size: 0x07 - the minimum encryption key size required to read the characteristic
		* Fixed characteristic value length					: FIXED_LENGTH
		*
		* This characteristic will be used (...TBD...)
		*/	
	/* Second characteristic's UUID */
	BLUENRG_memcpy(&char_obj_2.Char_UUID_128, char2_uuid, 16);
	
	/**
	  * @brief Third Characteristic (...TBD...)
		*
		* Handle 																		: Service handle to associate it with
		* UUID type																	:	128-bits																
		* Maximum length of the characteristic value: x bytes 
		* Characteristic Properties									: CHAR_PROP_READ (...TBD...)
		* Security Permissions											: None
		* GATT event mask flags											: GATT_DONT_NOTIFY_EVENTS
		* Enc_Key_Size: 0x07 - the minimum encryption key size required to read the characteristic
		* Fixed characteristic value length					: FIXED_LENGTH
		*
		* This characteristic will be used (...TBD...)
		*/	
	/* Third characteristic's UUID */
	BLUENRG_memcpy(&char_obj_3.Char_UUID_128, char3_uuid, 16);
	
	/**
	  * @brief Fourth Characteristic (...TBD...)
		*
		* Handle 																		: Service handle to associate it with
		* UUID type																	:	128-bits																
		* Maximum length of the characteristic value: x bytes 
		* Characteristic Properties									: CHAR_PROP_WRITE_WITHOUT_RESP (...TBD...)
		* Security Permissions											: None
		* GATT event mask flags											: GATT_DONT_NOTIFY_EVENTS
		* Enc_Key_Size: 0x07 - the minimum encryption key size required to read the characteristic
		* Fixed characteristic value length					: FIXED_LENGTH
		*
		* This characteristic will be used (...TBD...)
		*/	
	/* Fourth characteristic's UUID */	
	BLUENRG_memcpy(&char_obj_4.Char_UUID_128, char4_uuid, 16);
	
	/* Configure the four characteristic defined above for the GATT server (peripheral) */
	aci_gatt_add_char(hService, UUID_TYPE_128, &char_obj_1, 20, CHAR_PROP_INDICATE, 
											ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS, 
											0x07, CHAR_VALUE_LEN_CONSTANT, &hClientIndicate);
	aci_gatt_add_char(hService, UUID_TYPE_128, &char_obj_2, 20, CHAR_PROP_NOTIFY, 
											ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS,
											0x07, CHAR_VALUE_LEN_CONSTANT, &hClientNotification);
	aci_gatt_add_char(hService, UUID_TYPE_128, &char_obj_3, 20, CHAR_PROP_READ, 
											ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS, 
											0x07, CHAR_VALUE_LEN_CONSTANT, &hClientREAD);
	aci_gatt_add_char(hService, UUID_TYPE_128, &char_obj_4, 20, CHAR_PROP_WRITE|CHAR_PROP_WRITE_WITHOUT_RESP, 
											ATTR_PERMISSION_NONE, GATT_NOTIFY_ATTRIBUTE_WRITE,
											0x07, CHAR_VALUE_LEN_CONSTANT, &hClientWRITE);
	
	/* CCCD value */
	Char_Desc_Uuid_t DescriptorProperty;
	DescriptorProperty.Char_UUID_16 = CHAR_USER_DESC_UUID;
	
	const char char1name[] = {'T','E','S','T','_','O','N','E'};
	const char char2name[] = {'T','E','S','T','_','T','W','O'};
	const char char3name[] = {'T','E','S','T','_','T','H','R','E','E'};
	const char char4name[] = {'T','E','S','T','_','F','O','U','R'};
	
	/* Configure CCCD for the characteristics above (associated with characteristic UUIDs). The CCCD's
     might only be necessary for indicate/notify related events, as the CCCD feature in the GATT server
     allows client to configure server to automatically push new data to the client in the notify/indicate
     characteristics.	*/

	aci_gatt_add_char_desc(hService, hClientIndicate, UUID_TYPE_16, &DescriptorProperty,
														128, 9, (uint8_t*)char1name, ATTR_PERMISSION_NONE, ATTR_ACCESS_READ_ONLY, 
														GATT_DONT_NOTIFY_EVENTS, 7, CHAR_VALUE_LEN_CONSTANT, &hFirstCharDesc);
	aci_gatt_add_char_desc(hService, hClientNotification, UUID_TYPE_16, &DescriptorProperty,
														128, 9, (uint8_t*)char2name, ATTR_PERMISSION_NONE, ATTR_ACCESS_READ_ONLY, 
														GATT_NOTIFY_ATTRIBUTE_WRITE, 7, CHAR_VALUE_LEN_CONSTANT, &hSecondCharDesc);
	aci_gatt_add_char_desc(hService, hClientREAD, UUID_TYPE_16, &DescriptorProperty,
														128, 11, (uint8_t*)char3name, ATTR_PERMISSION_NONE, ATTR_ACCESS_READ_ONLY, 
														GATT_DONT_NOTIFY_EVENTS, 7, CHAR_VALUE_LEN_CONSTANT, &hThirdCharDesc);	
	aci_gatt_add_char_desc(hService, hClientWRITE, UUID_TYPE_16, &DescriptorProperty,
														128, 10, (uint8_t*)char4name, ATTR_PERMISSION_NONE, ATTR_ACCESS_READ_WRITE, 
														GATT_DONT_NOTIFY_EVENTS, 7, CHAR_VALUE_LEN_CONSTANT, &hFourthCharDesc);		
	
	
	/*
	Char_Desc_Uuid_t DescriptorProperty;
	DescriptorProperty.Char_UUID_16 = CHAR_CLIENT_CONFIG_DESC_UUID;
	uint16_t char_desc_value = 0;
	aci_gatt_add_char_desc(hService, hClientWRITE, UUID_TYPE_16, &DescriptorProperty, 128, 9,
													(uint8_t*)&char_desc_value, ATTR_PERMISSION_NONE, ATTR_ACCESS_READ_WRITE,
													GATT_NOTIFY_ATTRIBUTE_WRITE, 7, CHAR_VALUE_LEN_CONSTANT, &hFourthCharDesc);
	*/
}


/**
  * @brief	Resets/Deletes the entries of the variable holding the details of the connection with
  *					the GATT client
  */
static void Server_ResetConnectionStatus(void)
{
	/* Set to unknown/unregistered device role */
	Conn_Details.deviceRole = 0xFF;
	
	/* Set all fields to MAX_UINT16_T */
	Conn_Details.connectionhandle = 0xFFFF;
	Conn_Details.BLE_ConnInterval = 0xFFFF;				
	Conn_Details.BLE_ConnLatency = 0xFFFF;					
	Conn_Details.BLE_SupervisionTimeout = 0xFFFF;	
	
	/* Set status to not connected */
	Conn_Details.ConnectionStatus = STATE_NOT_CONNECTED;
	
	/* Reset 6-byte MAC address */
	BLUENRG_memset(&Conn_Details.BLE_Client_Addr[0], 0, 6);
}

/**
  * @brief	Enables BLE Peripheral device to be discoverable by advertising (with certain parameters)
  * @note		When BLE Peripheral adverises, it does so periodically at certain intervals. At these times
  *					power consumption of device will be high. 
  */
void BlueNRG_MakeDeviceDiscoverable(void)
{
	uint8_t ret;
	
	/* Name that will be broadcasted to Central Devices scanning */
	const char local_name[] = {AD_TYPE_COMPLETE_LOCAL_NAME, 'E','y','e','w','e','a','r','B','L','E'};
	
	/* Disable scan response: passive scan */
	hci_le_set_scan_response_data(0, NULL);
	
	/* Put the GAP peripheral in general discoverable mode:
			Advertising_Type: ADV_IND(undirected scannable and connectable);
			Advertising_Interval_Min: 100;
			Advertising_Interval_Max: 100;
			Own_Address_Type: PUBLIC_ADDR (public address: 0x00);
			Adv_Filter_Policy: NO_WHITE_LIST_USE (no whit list is used);
			Local_Name_Lenght: 13
			Local_Name: BlueNRG1Test;
			Service_Uuid_Length: 0 (no service to be advertised); Service_Uuid_List: NULL;
			Slave_Conn_Interval_Min: 0 (Slave connection internal minimum value);
			Slave_Conn_Interval_Max: 0 (Slave connection internal maximum value).
	*/	

	/* Configure scan response packet to be sent when GAP peripheral receives scan requests from GAP
     central performing general discovery procedure (active scan).
		 Scan Response Message must contain the following in this specific order:
				Length = 0x11
				Service UUID Type = 0x06 (128-bits Service UUID)
				Service UUID = (UUID taken from above)
	 */
	uint8_t uuidscanresponse[18] =
				{0x11,0x06,0x5D,0xCE,0xE1,0x5A,0x50,0x51,0x1D,0xB1,0x63,0x4D,0xF9,0x03,0x8B,0x32,0x98,0xA8};
	hci_le_set_scan_response_data(18, uuidscanresponse);
	
	/* Place Bluetooth Peripheral Device in Advertising State */
	ret = aci_gap_set_discoverable(ADV_IND, ADV_INTERV_MIN, ADV_INTERV_MAX, PUBLIC_ADDR,
																	NO_WHITE_LIST_USE, sizeof(local_name), (uint8_t*)local_name,
																	0, NULL, 0, 0);
	
	if (ret != BLE_STATUS_SUCCESS)
	{
		(void)strncpy(pText, "Error at Discoverable Mode\r\n", TEXTSIZE);
		HAL_UART_Transmit(&huart1, (uint8_t*)pText, TEXTSIZE, UART_TIMEOUT);
		while(1);	
	}
	
	// ret = hci_le_set_advertising_data();
	
	Conn_Details.ConnectionStatus = STATE_AWAITING_CONNECTION;
}

/********************** BLE HCI related events and event callbacks in Stack *****************************/

/**
  * @brief	Function taken from STM32Cube\Repository\Packs\STMicroelectronics\X-CUBE-BLE2\3.2.0\Projects\...
	*					...\Nucleo-L476RG\Application\SampleApp\...\gatt_db.c
  *					To be used with hci_init() that must be called before using BLE stack
  */
void APP_UserEvtRx(void *pData)
{
  uint32_t i;

  hci_spi_pckt *hci_pckt = (hci_spi_pckt *)pData;

  if(hci_pckt->type == HCI_EVENT_PKT)
  {
    hci_event_pckt *event_pckt = (hci_event_pckt*)hci_pckt->data;

    if(event_pckt->evt == EVT_LE_META_EVENT)
    {
      evt_le_meta_event *evt = (void *)event_pckt->data;

      for (i = 0; i < (sizeof(hci_le_meta_events_table)/sizeof(hci_le_meta_events_table_type)); i++)
      {
        if (evt->subevent == hci_le_meta_events_table[i].evt_code)
        {
          hci_le_meta_events_table[i].process((void *)evt->data);
        }
      }
    }
    else if(event_pckt->evt == EVT_VENDOR)
    {
      evt_blue_aci *blue_evt = (void*)event_pckt->data;

      for (i = 0; i < (sizeof(hci_vendor_specific_events_table)/sizeof(hci_vendor_specific_events_table_type)); i++)
      {
        if (blue_evt->ecode == hci_vendor_specific_events_table[i].evt_code)
        {
          hci_vendor_specific_events_table[i].process((void *)blue_evt->data);
        }
      }
    }
    else
    {
      for (i = 0; i < (sizeof(hci_events_table)/sizeof(hci_events_table_type)); i++)
      {
        if (event_pckt->evt == hci_events_table[i].evt_code)
        {
          hci_events_table[i].process((void *)event_pckt->data);
        }
      }
    }
  }
}

/*******************************************************************************
 * Function Name  : hci_le_connection_complete_event.
 * Description    : This event indicates the end of a connection procedure.
 * Input          : See file bluenrg1_events.h
 * Output         : See file bluenrg1_events.h
 * Return         : See file bluenrg1_events.h
 *******************************************************************************/
void hci_le_connection_complete_event(uint8_t Status,
                                      uint16_t Connection_Handle,
                                      uint8_t Role,
                                      uint8_t Peer_Address_Type,
                                      uint8_t Peer_Address[6],
                                      uint16_t Conn_Interval,
                                      uint16_t Conn_Latency,
                                      uint16_t Supervision_Timeout,
                                      uint8_t Master_Clock_Accuracy)

{ 
	/* This callback function/event only saves connection handle */
	Conn_Details.connectionhandle = Connection_Handle;
		
	/* Role should be slave: 0x01 (if 0x00, it is master and incorrect in this example project) */
	Conn_Details.deviceRole = Role;
	
	/* Save connection details in memory */
	BLUENRG_memcpy(&Conn_Details.BLE_Client_Addr, Peer_Address, 6);
	Conn_Details.BLE_ConnInterval = Conn_Interval;
	Conn_Details.BLE_ConnLatency = Conn_Latency;
	Conn_Details.BLE_SupervisionTimeout = Supervision_Timeout;
	
	/* Update connection status to connected */
	Conn_Details.ConnectionStatus = STATE_CONNECTED;
	
} /* end hci_le_connection_complete_event() */

/*******************************************************************************
 * Function Name  : hci_disconnection_complete_event.
 * Description    : This event indicates the end of a disconnection procedure.
 * Input          : See file bluenrg1_events.h
 * Output         : See file bluenrg1_events.h
 * Return         : See file bluenrg1_events.h
 *******************************************************************************/
void hci_disconnection_complete_event(uint8_t Status,
                                      uint16_t Connection_Handle,
                                      uint8_t Reason)
{
	/* Resets all connectivity status details */
	Server_ResetConnectionStatus();
	
} /* end hci_disconnection_complete_event() */

/*******************************************************************************
 * Function Name  : aci_gatt_notification_event.
 * Description    : Callback function triggered at client when GATT server does 
										a NOTIFY.
 * Input          : See file bluenrg1_events.h
 * Output         : See file bluenrg1_events.h
 * Return         : See file bluenrg1_events.h
 *******************************************************************************/
void aci_gatt_notification_event(uint16_t Connection_Handle,
                                 uint16_t Attribute_Handle,
                                 uint8_t Attribute_Value_Length,
                                 uint8_t Attribute_Value[])
{ 

	
} /* end aci_gatt_notification_event() */

/*******************************************************************************
 * Function Name  : aci_gatt_notification_event.
 * Description    : Callback function triggered when a WRITE is executed.
 * Input          : See file bluenrg1_events.h
 * Output         : See file bluenrg1_events.h
 * Return         : See file bluenrg1_events.h
 *******************************************************************************/
void aci_gatt_attribute_modified_event(uint16_t Connection_Handle,
                                       uint16_t Attr_Handle,
                                       uint16_t Offset,
                                       uint16_t Attr_Data_Length,
                                       uint8_t Attr_Data[])
{

	/* Determine which characteristic was modified by Client (Indicate and Notify characteristics
	   are modified by Client only if Client acknowledges these features on Server) */
	if(Attr_Handle == hClientWRITE+1)
	{
		if(Attr_Data_Length >= 2)
		{
			if(((Attr_Data[0] == 0x6F)||(Attr_Data[0] == 0x4F)) &&
						((Attr_Data[1] == 0x6E)||(Attr_Data[1] == 0x4E)))
			{
				/* If ASCII translation received is 'ON' not case sensitive */
				HAL_GPIO_WritePin(NUCLEO_LED_GPIO_Port, NUCLEO_LED_Pin, GPIO_PIN_SET);
				
				/* Notify ACK to master */
				uint8_t buff[6] = {0x4F, 0x4E, 0x41, 0x43, 0x4B, 0x00};
				aci_gatt_update_char_value(hService, hClientNotification, 0, 6, buff);
			}
			else if(((Attr_Data[0] == 0x6F)||(Attr_Data[0] == 0x4F)) &&
								((Attr_Data[1] == 0x66)||(Attr_Data[1] == 0x46)) && 
								((Attr_Data[1] == 0x66)||(Attr_Data[1] == 0x46)))
			{
				/* If ASCII translation received is 'OFF' not case sensitive */
				HAL_GPIO_WritePin(NUCLEO_LED_GPIO_Port, NUCLEO_LED_Pin, GPIO_PIN_RESET);
				
				/* Notify ACK to master */
				uint8_t buff[6] = {0x4F, 0x46, 0x46, 0x41, 0x43, 0x4B};
				aci_gatt_update_char_value(hService, hClientNotification, 0, 6, buff);
			}
		}
	}
	else if(Attr_Handle == hClientIndicate+2)
	{
		
		
		
	}
	else if(Attr_Handle == hClientNotification+2)
	{
		
		
		
	}
	
} /* end aci_gatt_attribute_modified_event() */

/********************** User Application related functions/events/processes *****************************/

/**
  * @brief 	Updates the BLE events list. This will also contain FSM for BLE communication protocol
	* @note		hci_user_evt_proc() must be called after an event is received from the HCI interface. This 
	*					function will call the appropriate event callback functions related to BLE write/read/indicate/
	*					notify events. Must be called outside an ISR.
  * 
  */
void BlueNRG_Loop(void)
{
	hci_user_evt_proc();
	
	/* FSM to handle device connectivity */
	switch(Conn_Details.ConnectionStatus)
	{
		case STATE_NOT_CONNECTED:
		{
			BlueNRG_MakeDeviceDiscoverable();
			Conn_Details.ConnectionStatus = STATE_AWAITING_CONNECTION;
			break;
		}
		
		case STATE_AWAITING_CONNECTION:
		{
			/* Implement counter to turn off device when it doesn't connect successfully */
			Conn_Details.ConnectionStatus = STATE_AWAITING_CONNECTION;
			break;
		}
		
		case STATE_CONNECTED:
		{
			Conn_Details.ConnectionStatus = STATE_CONNECTED;
			break;
		}
		
	}
}

/**
  * @brief 	Event performed when triggered by the NUCLEO_PB
  */
void TestUpdateCharacteristic(void)
{
	static uint32_t counter = 0;
	if(counter%2 == 0)
	{
		uint8_t buff[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
												0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
		aci_gatt_update_char_value(hService, hClientREAD, 0, 16, buff);
		aci_gatt_update_char_value(hService, hClientIndicate, 0, 16, buff);
	}
	else
	{
		uint8_t buff[16] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66,
												0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
		aci_gatt_update_char_value(hService, hClientREAD, 0, 16, buff);
		aci_gatt_update_char_value(hService, hClientIndicate, 0, 16, buff);												
	}
	counter++;
}

/******************************************* END OF FILE *******************************************/


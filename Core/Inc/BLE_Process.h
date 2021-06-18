
/**
  **************************************************************************************************
  * @file           : BLE_Process.h
  * @brief          : Header for BLE_Process.c file
  * @author         :
  **************************************************************************************************
  */


/* Define to prevent recursive inclusion ---------------------------------------------------------*/
#ifndef __BLE_PROCESS_H
#define __BLE_PROCESS_H


#ifdef __cplusplus
extern "C" {
#endif


/* Includes --------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "bluenrg_conf.h"


/* Exported defines ------------------------------------------------------------------------------*/
#define DEVICE_TYPE_GAP_PERIPHERAL
#define TEXTSIZE											50
#define UART_TIMEOUT									1000

/**
  * @brief GAP Roles
	*
	* Use Peripheral for slave devices to allow master to connect to it, and central for master 
	* devices to allow connecting to slave devices. For this project, the eyewear/glasses are peripheral
	* devices, and the dongle are central devices.
	*/
#define GAP_ROLE_PERIPHERAL								((uint8_t)0x01)
#define GAP_ROLE_BROADCASTER							((uint8_t)0x02)
#define GAP_ROLE_CENTRAL									((uint8_t)0x04)
#define GAP_ROLE_OBSERVER									((uint8_t)0x08)

/**
  * @brief GAP Privacy/Security
	*
	* Use Peripheral for slave devices to allow master to connect to it, and central for master 
	* devices to allow connecting to slave devices. For this project, the eyewear/glasses are peripheral
	* devices, and the dongle are central devices.
	*/
#define GAP_PRIVACY_DISABLED							((uint8_t)0x00)
#define GAP_PRIVACY_HOST_ENABLED					((uint8_t)0x01)
#define GAP_PRIVACY_CONTROLLER_ENABLED		((uint8_t)0x02)


/* Exported variables ----------------------------------------------------------------------------*/
extern char pText[TEXTSIZE];


/* Exported types --------------------------------------------------------------------------------*/
typedef enum
{
	STATE_NOT_CONNECTED = 0x00,
	STATE_AWAITING_CONNECTION = 0x11,
	
	STATE_CONNECTED = 0x99,
	
} BLE_State_t;

/* Exported constants ----------------------------------------------------------------------------*/


/* Exported macro --------------------------------------------------------------------------------*/


/* Exported Functions ----------------------------------------------------------------------------*/
/*** BLE Stack and System Init ***/
void BlueNRG_Init(void);
void BlueNRG_MakeDeviceDiscoverable(void);

/*** Custom BLE HCI Functions and Events ***/
void APP_UserEvtRx(void *pData);

/*** User Application Related Routines/Functions ***/
void BlueNRG_Loop(void);
void TestUpdateCharacteristic(void);



#ifdef __cplusplus 
}
#endif



#endif  /* __BLE_PROCESS_H */


/******************************************* END OF FILE *******************************************/


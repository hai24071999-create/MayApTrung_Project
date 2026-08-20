/*
 * mykeypad.h
 *
 *  Created on: Jun 26, 2026
 *      Author: user1
 */

#ifndef MYKEYPAD_H_
#define MYKEYPAD_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>

#define KEY_PAD_ROW_COUNT     (4)
#define KEY_PAD_COLUMN_COUNT  (4)

typedef enum {
    KEY_PAD_OK             = 0U,  // successful
    KEY_PAD_NO_KEY         = 1U,  // No key pressed
    KEY_PAD_ERROR          = 2U,  // General error
} KeyPad_Error_t;

typedef struct _PORT_PIN_KEYPAD
{
	GPIO_TypeDef *port;
	uint16_t pin;
}PORT_PIN_KEYPAD;

typedef struct _CONFIG_KEYPAD
{
	PORT_PIN_KEYPAD rowPins[KEY_PAD_ROW_COUNT];
	PORT_PIN_KEYPAD colPins[KEY_PAD_COLUMN_COUNT];
	char mapKeys[KEY_PAD_ROW_COUNT][KEY_PAD_COLUMN_COUNT];
}CONFIG_KEYPAD;

typedef struct _DATA_KEYPAD
{
	uint32_t previousScanMillis;
	KeyPad_Error_t lastError;

	uint16_t lastIntPin;
	char lastKey;
}DATA_KEYPAD;

// Initialize key pad and data key pad
void keyPad_init(CONFIG_KEYPAD *lpKeyPadConfig, DATA_KEYPAD *lpDataKeyPad);

// scan keypad to detect pressed key with interruption
void keyPad_int_scan(DATA_KEYPAD *dataKeyPad, uint16_t intPin);


// Example:
//
// CONFIG_KEYPAD keyPadConfig = {
//		  {			// row keypad to GPIO Pin (set interruption GPIO_EXTI)
//				  {GPIOB, GPIO_PIN_9},
//				  {GPIOB, GPIO_PIN_8},
//				  {GPIOB, GPIO_PIN_7},
//				  {GPIOB, GPIO_PIN_6}
//		  },
//		  {		  // column keypad to GPIO Pin (set GPIO_Output)
//				  {GPIOB, GPIO_PIN_3},
//				  {GPIOB, GPIO_PIN_4},
//				  {GPIOB, GPIO_PIN_5}
//		  },
//		  {		// key map
//		       {'3', '2', '1'},
//		       {'6', '5', '4'},
//		       {'9', '8', '7'},
//			   {'#', '0', '*'},
//		  }
//  };
//
//  DATA_KEYPAD dataKeyPad = {};
//
//  keyPad_init(&keyPadConfig, &dataKeyPad);
//
//  // Handle keypress interruption
//	// The keypress data will be stored into dataKeyPad
// void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
// {
//	 keyPad_int_scan(&dataKeyPad, GPIO_Pin);
// }
//
//	// get pressed key
//  char preSelectedKeyChar = ' ';
//	if(dataKeyPad.lastError == KEY_PAD_OK &&
//			  dataKeyPad.lastKey != preSelectedKeyChar)
//	  {
//		  preSelectedKeyChar = dataKeyPad.lastKey;
//		  dataKeyPad.lastError = KEY_PAD_NO_KEY;
//
//		  //Processing pressed key ...
//		  //preSelectedKeyChar
//	  }
#endif /* MYKEYPAD_H_ */

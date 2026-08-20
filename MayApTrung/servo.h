#ifndef SERVO_H
#define SERVO_H

#include "stm32f1xx_hal.h"
//#include "tim.h"

/*======================== Define ========================*/



/*======================== Function ========================*/

/* Kh?i t?o Servo */
void SERVO_Init(void);

/* Quay Servo d?n góc (0 - 180 d?) */
void SERVO_SetAngle(uint8_t Angle);

/* Quay Servo sang trái */
void SERVO_Left(void);

/* Quay Servo sang ph?i */
void SERVO_Right(void);

#endif

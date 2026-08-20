#include "SERVO.h"

extern TIM_HandleTypeDef htim2;

#define SERVO_TIMER    (&htim2)
#define SERVO_CHANNEL  TIM_CHANNEL_1

//#include "systick_delay.h"

/*======================== Private Function ========================*/

/* Chuy?n góc (0 - 180) sang giá tr? CCR */
static uint16_t SERVO_AngleToCCR(uint8_t Angle)
{
    if (Angle > 180)
        Angle = 180;

    return 500 + ((uint32_t)Angle * 2000) / 180;
}

/*======================== Function ========================*/

/* Kh?i t?o Servo */
void SERVO_Init(void)
{
    HAL_TIM_PWM_Start(SERVO_TIMER, SERVO_CHANNEL);
    SERVO_SetAngle(0);      // hoặc 0 tùy ý
    HAL_Delay(500);
}

/* Quay Servo d?n góc b?t k? */
void SERVO_SetAngle(uint8_t Angle)
{
    __HAL_TIM_SET_COMPARE(SERVO_TIMER, SERVO_CHANNEL, SERVO_AngleToCCR(Angle));
}

/* Quay Servo sang trái */
void SERVO_Right(void)
{
	for(uint16_t angle = 0; angle <= 90; angle++)
	    {
	        SERVO_SetAngle(angle);
	        HAL_Delay(56);
	    }
}

/* Quay Servo sang phai */
void SERVO_Left(void)
{
	for(uint16_t angle = 90; angle > 0; angle--)
	    {
	        SERVO_SetAngle(angle);
	        HAL_Delay(56);
	    }
}

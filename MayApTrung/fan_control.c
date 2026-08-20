#include "fan_control.h"

#define FAN_OFF 0
#define FAN_ON  1

typedef enum {
    BTN_IDLE,
    BTN_WAIT_RELEASE_1,
    BTN_WAIT_PRESS_2,
    BTN_WAIT_RELEASE_2
} BtnState_t;

static uint8_t fan_state = FAN_OFF;
static BtnState_t btn_state = BTN_IDLE;

static uint32_t first_press_time = 0;
static uint32_t debounce_timer = 0;
static uint8_t btn_stable_state = 0;
static uint8_t last_raw_state = 0;

static uint8_t ReadButton(void) {
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
        return 1;
    }
    return 0;
}

static void SetFan(uint8_t state) {
    if (state == FAN_ON) {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    }
}

void Fan_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure PA3 as Input with Pull-up
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Configure PA2 as Output for Fan
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    fan_state = FAN_OFF;
    SetFan(fan_state);
}

void Fan_Process(void) {
    uint32_t current_time = HAL_GetTick();
    uint8_t raw_state = ReadButton();
    
    // Debounce
    if (raw_state != last_raw_state) {
        debounce_timer = current_time;
    }
    if ((current_time - debounce_timer) > 50) { 
        if (raw_state != btn_stable_state) {
            btn_stable_state = raw_state;
        }
    }
    last_raw_state = raw_state;
    
    // Logic state machine
    switch (btn_state) {
        case BTN_IDLE:
            if (btn_stable_state == 1) { 
                btn_state = BTN_WAIT_RELEASE_1;
                first_press_time = current_time;
                
                // Turn on fan
                fan_state = FAN_ON;
                SetFan(fan_state);
            }
            break;
            
        case BTN_WAIT_RELEASE_1:
            if (btn_stable_state == 0) {
                btn_state = BTN_WAIT_PRESS_2;
            }
            break;
            
        case BTN_WAIT_PRESS_2:
            if (btn_stable_state == 1) {
                if ((current_time - first_press_time) <= 1000) {
                    // Turn off fan on double press
                    fan_state = FAN_OFF;
                    SetFan(fan_state);
                    btn_state = BTN_WAIT_RELEASE_2;
                }
            } else if ((current_time - first_press_time) > 1000) {
                btn_state = BTN_IDLE;
            }
            break;
            
        case BTN_WAIT_RELEASE_2:
            if (btn_stable_state == 0) {
                btn_state = BTN_IDLE;
            }
            break;
    }
}

// ==========================================
// LED Status Indicator (PC13)
// ==========================================
static uint32_t led_timer = 0;
static uint8_t led_state = 0;

void LED_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Bật clock cho bọt C
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // Cấu hình PC13 là Output
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // Tắt LED mặc định (Mức 1 là tắt trên Bluepill)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

void LED_Process(void) {
    uint32_t current_time = HAL_GetTick();
    
    // Quy luật nháy (Heartbeat): Nháy 2 lần nhanh (báo còn sống) rồi nghỉ
    // Chu kỳ 1000ms chia làm 10 nhịp (mỗi nhịp 100ms)
    // Nhịp 0: Sáng
    // Nhịp 1: Tắt
    // Nhịp 2: Sáng
    // Nhịp 3-9: Tắt
    if (current_time - led_timer >= 100) {
        led_timer = current_time;
        
        led_state++;
        if (led_state >= 10) {
            led_state = 0;
        }
        
        if (led_state == 0 || led_state == 2) {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // Bật LED
        } else {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // Tắt LED
        }
    }
}

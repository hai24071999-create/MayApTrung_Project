#ifndef INC_MY_FLASH_H_
#define INC_MY_FLASH_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>

// max pages count for stm32f103c8t6 is 64 pages (64 KB)
// 1 page => 1KB
//
#define FLASH_TOTAL_PAGE  64   // STM32F103C8T6 có 64KB Flash

// Some begin flash pages are used for storing program code
//
#define FLASH_BEGIN_PAGE_0	0x08000000
#define FLASH_END_PAGE_0	0x080003FF

#define FLASH_BASE_ADDR 0x08000000

// The program configuration data need to store at end pages
#define FLASH_BEGIN_PAGE_ADDR_54	0x0800D800
#define FLASH_END_PAGE_ADDR_54	0x0800DBFF

#define FLASH_BEGIN_PAGE_ADDR_62	0x0800F800
#define FLASH_END_PAGE_ADDR_62	0x0800FBFF

#define FLASH_BEGIN_PAGE_ADDR_63	0x0800FC00
#define FLASH_END_PAGE_ADDR_63	0x0800FFFF

#define PAGE_DATA_NUM	63
#define BEGIN_PAGE_DATA_ADDRESS	FLASH_BEGIN_PAGE_ADDR_63

// range page for writing data (54, 63)
#define FIND_REVERSE_PAGE_FROM 63
#define FIND_REVERSE_PAGE_TO 54

// The mark values for page status
#define PAGE_STATUS_GOOD  0xFFFFFFFF
#define PAGE_STATUS_BAD   0x00000FF7

typedef enum {
    PAGE_FOUND_NONE = 0,   // not found valid page
    PAGE_FOUND_EMPTY,      // page empty
    PAGE_FOUND_GOOD        // page found good
} PageFoundStatus;

//------------------------------------------------------------------------
// Functions support for 1 page with marking status (GOOD, BAD) at first word

uint32_t My_Flash_FindLatestGoodOrEmptyPage(int startPage, int stopPage, PageFoundStatus *status, bool usingFlagStatus);

bool My_Flash_ReadPage(uint32_t pageAddr, uint32_t *buffer, uint32_t word_count, bool usingFlagStatus);
bool My_Flash_WritePage(uint32_t pageAddr, uint32_t *word_buffer, uint32_t word_count, bool usingFlagStatus);
void My_Flash_MarkPageBad(uint32_t pageAddr, bool needLock);

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// Function support multi pages without status marking
uint32_t My_Flash_Write_Data (uint32_t StartPageAddress, uint32_t *Data, uint16_t numberofwords);

void My_Flash_Read_Data (uint32_t StartPageAddress, uint32_t *RxBuf, uint16_t numberofwords);
//------------------------------------------------------------------------

#endif /* INC_MY_FLASH_H_ */

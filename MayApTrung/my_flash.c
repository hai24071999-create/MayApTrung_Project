#include "my_flash.h"

bool My_Flash_IsPageBad(uint32_t pageAddr) {
    return (*(__IO uint32_t*)pageAddr == PAGE_STATUS_BAD);
}

bool My_Flash_IsPageEmpty(uint32_t pageAddr) {

	uint32_t first_word = *(__IO uint32_t*)pageAddr;

	// (0xFFFFFFFF on real device, 0x00000000 on Proteus)
	uint32_t empty_value = (first_word == 0x00000000) ? 0x00000000 : 0xFFFFFFFF;

    for (uint32_t addr = pageAddr; addr < pageAddr + FLASH_PAGE_SIZE; addr += 4) {
        if (*(__IO uint32_t*)addr != empty_value) {
            return false; // have data
        }
    }
    return true; // all values 0xFFFFFFFF => empty
}

// Found page GOOD or EMPTY, ignore BAD
// startPage: last page (example: 63)
// stopPage: stop limit page (example: 54)
// Return page address and status
uint32_t My_Flash_FindLatestGoodOrEmptyPage(int startPage, int stopPage, PageFoundStatus *status, bool usingFlagStatus) {

	uint32_t emptyCandidate = 0;

	for (int page = startPage; page >= stopPage; page--) {

        uint32_t pageAddr = FLASH_BASE_ADDR + page * FLASH_PAGE_SIZE;


        if (usingFlagStatus && My_Flash_IsPageBad(pageAddr)) {

            continue; // ignore page BAD
        }

        if (My_Flash_IsPageEmpty(pageAddr)) {
            // store first found page empty
            emptyCandidate = pageAddr;
            *status = PAGE_FOUND_EMPTY;
            break;
        } else {
            // found page GOOD having data
            *status = PAGE_FOUND_GOOD;
            return pageAddr;
        }
    }

    if (emptyCandidate != 0) {
        return emptyCandidate; // return page EMPTY
    }

    *status = PAGE_FOUND_NONE;
    return 0;
}

static uint32_t My_GetPage(uint32_t Address)
{
  for (int indx = 0; indx < FLASH_TOTAL_PAGE; indx++)
  {
	  if((Address < (0x08000000 + (FLASH_PAGE_SIZE *(indx+1))) ) && (Address >= (0x08000000 + FLASH_PAGE_SIZE*indx)))
	  {
		  return (0x08000000 + FLASH_PAGE_SIZE*indx);
	  }
  }

  return 0;
}

uint32_t My_Flash_Write_Data (uint32_t StartPageAddress, uint32_t *Data, uint16_t numberofwords)
{
	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t PAGEError;
	int sofar = 0;

	/* Erase the user Flash area*/

	uint32_t StartPage = My_GetPage(StartPageAddress);
	if (StartPage <= 0)
	{
		// map address error
		return 1;
	}

	uint32_t EndPageAdress = StartPageAddress + numberofwords*4;
	uint32_t EndPage = My_GetPage(EndPageAdress);
	if (EndPage <= 0)
	{
		// map address error
		return 1;
	}

	/* Unlock the Flash to enable the flash control register access *************/
	HAL_FLASH_Unlock();

	/* Fill EraseInit structure*/
	EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.PageAddress = StartPage;
	EraseInitStruct.NbPages     = ((EndPage - StartPage)/FLASH_PAGE_SIZE) + 1;

	if (HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError) != HAL_OK)
	{
	 /*Error occurred while page erase.*/
	  return HAL_FLASH_GetError ();
	}

	/* Program the user Flash area word by word*/

	while (sofar < numberofwords)
	{
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, StartPageAddress, Data[sofar]) == HAL_OK)
		{
			 StartPageAddress += 4;  // use StartPageAddress += 2 for half word and 8 for double word
			 sofar++;
		}
		else
		{
			/* Error occurred while writing data in Flash memory*/
			 return HAL_FLASH_GetError ();
		}
	}

	/* Lock the Flash to disable the flash control register access (recommended
	  to protect the FLASH memory against possible unwanted operation) *********/
	HAL_FLASH_Lock();

	return 0;
}

void My_Flash_Read_Data (uint32_t StartPageAddress, uint32_t *RxBuf, uint16_t numberofwords)
{
	while (numberofwords > 0)
	{
		*RxBuf = *(__IO uint32_t *)StartPageAddress;
		StartPageAddress += 4;
		RxBuf++;
		numberofwords--;
	}
}

//
// Read data from page, ignore BAD page
// buffer: storing output data
// word_count: number of word need to be read
// Return true if success
//
bool My_Flash_ReadPage(uint32_t pageAddr, uint32_t *buffer, uint32_t word_count, bool usingFlagStatus) {

    if (usingFlagStatus && My_Flash_IsPageBad(pageAddr)) {
        return false; // ignore page BAD
    }

    if (My_Flash_IsPageEmpty(pageAddr)) {
        return false; // ignore page empty
    }

    if(usingFlagStatus)
    {
    	// ignore first 4 marked bytes
    	pageAddr += 4;
    }

    for (uint32_t i = 0; i < word_count; i++) {
        buffer[i] = *(__IO uint32_t*)(pageAddr);
        pageAddr += 4;
    }

    return true;
}

uint32_t My_Flash_GetPageState(uint32_t pageAddr) {

	uint32_t marker = *(__IO uint32_t*)pageAddr;

    return marker;
}

void My_Flash_SetPageStatus(uint32_t pageAddr, uint32_t statusValue, bool needLock) {

	if (needLock)
	{
		HAL_FLASH_Unlock();
	}

    // Read current value
    uint32_t currentValue = *(__IO uint32_t*)pageAddr;

    // Only write when status changed
    if (currentValue != statusValue) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, pageAddr, statusValue);
    }

	if (needLock)
	{
		HAL_FLASH_Lock();
	}
}

// Mark page BAD
void My_Flash_MarkPageBad(uint32_t pageAddr, bool needLock) {
    My_Flash_SetPageStatus(pageAddr, PAGE_STATUS_BAD, needLock);
}

bool My_Flash_WritePage(uint32_t pageAddr, uint32_t *word_buffer, uint32_t word_count, bool usingFlagStatus) {
    HAL_FLASH_Unlock();

    // Erase page
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = pageAddr;
    eraseInit.NbPages = 1;

    if (HAL_FLASHEx_Erase(&eraseInit, &pageError) != HAL_OK) {
    	if (usingFlagStatus)
    		My_Flash_MarkPageBad(pageAddr, false);

        HAL_FLASH_Lock();
        return false;
    }

    // checking after erase: all data must be 0xFFFFFFFF (real device) or 0x0 (proteus)
    uint32_t first_word = *(__IO uint32_t*)pageAddr;
    uint32_t empty_value = (first_word == 0x00000000) ? 0x00000000 : 0xFFFFFFFF;

	for (uint32_t addr = pageAddr; addr < pageAddr + FLASH_PAGE_SIZE; addr += 4) {
		if (*(__IO uint32_t*)addr != empty_value) {

			if (usingFlagStatus)
				My_Flash_MarkPageBad(pageAddr, false);

			HAL_FLASH_Lock();
			return false;
		}
	}

    // write data
    //
    if (usingFlagStatus)
    {
		My_Flash_SetPageStatus(pageAddr, PAGE_STATUS_GOOD, false);

		pageAddr += 4;
    }

    for (uint32_t i = 0; i < word_count; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, pageAddr, word_buffer[i]) != HAL_OK) {

        	if (usingFlagStatus)
        		My_Flash_MarkPageBad(pageAddr, false);

            HAL_FLASH_Lock();
            return false;
        }

        pageAddr += 4;
    }

    HAL_FLASH_Lock();
    return true;
}

/************************************************************************************//**
* \file         Demo\ARMCM3_STM32_Olimex_STM32P103_GCC\Boot\main.c
* \brief        Bootloader application source file.
* \ingroup      Boot_ARMCM3_STM32_Olimex_STM32P103_GCC
* \internal
*----------------------------------------------------------------------------------------
*                          C O P Y R I G H T
*----------------------------------------------------------------------------------------
*   Copyright (c) 2012  by Feaser    http://www.feaser.com    All rights reserved
*
*----------------------------------------------------------------------------------------
*                            L I C E N S E
*----------------------------------------------------------------------------------------
* This file is part of OpenBLT. OpenBLT is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published by the Free
* Software Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* OpenBLT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with OpenBLT.
* If not, see <http://www.gnu.org/licenses/>.
*
* A special exception to the GPL is included to allow you to distribute a combined work 
* that includes OpenBLT without being obliged to provide the source code for any 
* proprietary components. The exception text is included at the bottom of the license
* file <license.html>.
* 
* \endinternal
****************************************************************************************/

/****************************************************************************************
* Include files
****************************************************************************************/
#include "boot.h"                                /* bootloader generic header          */
#include "stm32f10x.h"                           /* microcontroller registers          */
#if (BOOT_FILE_LOGGING_ENABLE > 0) && (BOOT_COM_UART_ENABLE == 0)
#include "stm32f10x_conf.h"                      /* STM32 peripheral drivers           */
#endif


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static void Init(void);


/************************************************************************************//**
** \brief     This is the entry point for the bootloader application and is called 
**            by the reset interrupt vector after the C-startup routines executed.
** \return    Program return code.
**
****************************************************************************************/
int main(void)
{
  /* initialize the microcontroller */
  Init();
  /* initialize the bootloader */
  BootInit();

  /* start the infinite program loop */
  while (1)
  {
    /* run the bootloader task */
    BootTask();
  }

  /* program should never get here */
  return 0;
} /*** end of main ***/


/************************************************************************************//**
** \brief     Initializes the microcontroller. 
** \return    none.
**
****************************************************************************************/
static void Init(void)
{
  SystemInit();

	RCC_DeInit();
	//FLASH_HalfCycleAccessCmd(FLASH_HalfCycleAccess_Disable);
	RCC_ClockSecuritySystemCmd(DISABLE);

	//FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
  FLASH->ACR |= FLASH_ACR_PRFTBE;

	RCC_HCLKConfig(RCC_SYSCLK_Div1); 	/* HCLK = SYSCLK = 64MHZ */
	RCC_PCLK2Config(RCC_HCLK_Div1); 	/* PCLK2 = HCLK/1 = 64MHz */
	RCC_PCLK1Config(RCC_HCLK_Div2); 	/* PCLK1 = HCLK/2 = 32MHz */
	RCC_ADCCLKConfig(RCC_PCLK2_Div8);	/* ADCCLK = HCLK/8 = 8MHz */

  /* Flash 2 wait state */
	//FLASH_SetLatency(FLASH_Latency_2);
  FLASH->ACR &= ~FLASH_ACR_LATENCY;
  FLASH->ACR |= FLASH_ACR_LATENCY_2;

	/* Select the PLL as system clock source */
	RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_16);
	RCC_PLLCmd(ENABLE);

	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

  RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
  while(RCC_GetSYSCLKSource() != 0x08);
}

/*********************************** end of main.c *************************************/

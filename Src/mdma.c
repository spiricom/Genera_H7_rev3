/**
  ******************************************************************************
  * File Name          : mdma.c
  * Description        : This file provides code for the configuration
  *                      of all the requested global MDMA transfers.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mdma.h"

/* USER CODE BEGIN 0 */

#include "ui.h"

uint8_t TransferCompleteDetected = 0;
uint8_t TransferErrorDetected = 1;

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure MDMA                                                              */
/*----------------------------------------------------------------------------*/

/* USER CODE BEGIN 1 */
static void MDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma);

static void MDMA_TransferErrorCallback(MDMA_HandleTypeDef *hmdma);

/* USER CODE END 1 */
MDMA_HandleTypeDef hmdma_mdma_channel40_dma1_stream0_tc_0;
MDMA_LinkNodeTypeDef node_mdma_channel40_dma1_stream0_tc_1;

/** 
  * Enable MDMA controller clock
  * Configure MDMA for global transfers
  *   hmdma_mdma_channel40_dma1_stream0_tc_0
  *   node_mdma_channel40_dma1_stream0_tc_1
  */
void MX_MDMA_Init(void) 
{

  /* MDMA controller clock enable */
  __HAL_RCC_MDMA_CLK_ENABLE();
  /* Local variables */
  MDMA_LinkNodeConfTypeDef nodeConfig;

  /* Configure MDMA channel MDMA_Channel0 */
  /* Configure MDMA request hmdma_mdma_channel40_dma1_stream0_tc_0 on MDMA_Channel0 */
  hmdma_mdma_channel40_dma1_stream0_tc_0.Instance = MDMA_Channel0;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.Request = MDMA_REQUEST_DMA1_Stream0_TC;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.TransferTriggerMode = MDMA_BUFFER_TRANSFER;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.Priority = MDMA_PRIORITY_VERY_HIGH;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.SourceInc = MDMA_SRC_INC_HALFWORD;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.DestinationInc = MDMA_DEST_INC_HALFWORD;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.SourceDataSize = MDMA_SRC_DATASIZE_HALFWORD;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.DestDataSize = MDMA_DEST_DATASIZE_HALFWORD;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.BufferTransferLength = 12;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.SourceBurst = MDMA_SOURCE_BURST_SINGLE;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.DestBurst = MDMA_DEST_BURST_SINGLE;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.SourceBlockAddressOffset = 0;
  hmdma_mdma_channel40_dma1_stream0_tc_0.Init.DestBlockAddressOffset = 0;
  if (HAL_MDMA_Init(&hmdma_mdma_channel40_dma1_stream0_tc_0) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_MDMA_ConfigPostRequestMask(&hmdma_mdma_channel40_dma1_stream0_tc_0,(uint32_t)&(DMA1->HIFCR),DMA_HIFCR_CTCIF5);
  /* Template to be copied and modified in the user code section below */
  /* Please give a value to the following parameters set by default to 0 */
  /*
  nodeConfig.SrcAddress = 0;
  nodeConfig.DstAddress = 0;
  nodeConfig.BlockDataLength = 0;
  nodeConfig.BlockCount = 0;
  if (HAL_MDMA_LinkedList_CreateNode(&node_mdma_channel40_dma1_stream0_tc_1, &nodeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  */


  HAL_MDMA_RegisterCallback(&hmdma_mdma_channel40_dma1_stream0_tc_0, HAL_MDMA_XFER_CPLT_CB_ID, MDMA_TransferCompleteCallback);
  HAL_MDMA_RegisterCallback(&hmdma_mdma_channel40_dma1_stream0_tc_0, HAL_MDMA_XFER_ERROR_CB_ID, MDMA_TransferErrorCallback);


  /* MDMA interrupt initialization */
  /* MDMA_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(MDMA_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(MDMA_IRQn);

  //HAL_MDMA_Start_IT(&hmdma_mdma_channel40_dma1_stream0_tc_0, (uint32_t)&ADC_valuesDMA,(uint32_t)&ADC_values,12,1);

}
/* USER CODE BEGIN 2 */
static void MDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma)
{
	  //HAL_MDMA_Start_IT(&hmdma_mdma_channel40_dma1_stream0_tc_0, (uint32_t)&ADC_valuesDMA,(uint32_t)&ADC_values,12,1);
  setLED_A(1);
}

/**
  * @brief  MDMA transfer error callback
  * @note   This function is executed when the transfer error interrupt
  *         is generated during MDMA transfer
  * @retval None
  */
static void MDMA_TransferErrorCallback(MDMA_HandleTypeDef *hmdma)
{
  TransferErrorDetected = 1;
  setLED_B(1);
}
/* USER CODE END 2 */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

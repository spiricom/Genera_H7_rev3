/*
 * audiostream.c
 *
 *  Created on: Aug 30, 2019
 *      Author: jeffsnyder
 */


/* Includes ------------------------------------------------------------------*/
#include "audiostream.h"
#include "main.h"
#include "leaf.h"
#include "codec.h"
#include "tim.h"
#include "ui.h"

#define NUM_BUTTONS 3


//the audio buffers are put in the D2 RAM area because that is a memory location that the DMA has access to.
int32_t audioOutBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;
int32_t audioInBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;


#define MEM_SIZE 500000
char memory[MEM_SIZE] __ATTR_RAM_D1;

void audioFrame(uint16_t buffer_offset);
float audioTickL(float audioIn, uint32_t sampleNum);
float audioTickR(float audioIn, uint32_t sampleNum);
void buttonCheck(void);

HAL_StatusTypeDef transmit_status;
HAL_StatusTypeDef receive_status;


uint8_t codecReady = 0;

uint8_t buttonValues[NUM_BUTTONS];
uint8_t buttonValuesPrev[NUM_BUTTONS];
uint32_t buttonCounters[NUM_BUTTONS];
uint32_t buttonPressed[NUM_BUTTONS];


float sample = 0.0f;

float syncADC[3][HALF_BUFFER_SIZE];
uint32_t sampleCounter = 0;
uint32_t adc_offset;

uint16_t frameCounter = 0;

//audio objects
tRamp adc[6];

tNoise noise;

tCycle mySine[6];




/**********************************************/

typedef enum BOOL {
	FALSE = 0,
	TRUE
} BOOL;


void audioInit(I2C_HandleTypeDef* hi2c, SAI_HandleTypeDef* hsaiOut, SAI_HandleTypeDef* hsaiIn)
{
	// Initialize LEAF.

	LEAF_init(SAMPLE_RATE, AUDIO_FRAME_SIZE, memory, MEM_SIZE, &randomNumber);


	for (int i = 0; i < 6; i++)
	{
		tRamp_init(&adc[i],7.0f, 1); //set all ramps for knobs to be 7ms ramp time and let the init function know they will be ticked every sample

	}
	tNoise_init(&noise, WhiteNoise);
	for (int i = 0; i < 6; i++)
	{
		tCycle_init(&mySine[i]);
		tCycle_setFreq(&mySine[i], 440.0f);
	}

	HAL_Delay(10);

	for (int i = 0; i < AUDIO_BUFFER_SIZE; i++)
	{
		audioOutBuffer[i] = 0;
	}



	HAL_Delay(1);

	// set up the I2S driver to send audio data to the codec (and retrieve input as well)
	transmit_status = HAL_SAI_Transmit_DMA(hsaiOut, (uint8_t *)&audioOutBuffer[0], AUDIO_BUFFER_SIZE);
	receive_status = HAL_SAI_Receive_DMA(hsaiIn, (uint8_t *)&audioInBuffer[0], AUDIO_BUFFER_SIZE);

	// with the CS4271 codec IC, the SAI Transmit and Receive must be happening before the chip will respond to
	// I2C setup messages (it seems to use the masterclock input as it's own internal clock for i2c data, etc)
	// so while we used to set up codec before starting SAI, now we need to set up codec afterwards, and set a flag to make sure it's ready

	//now to send all the necessary messages to the codec
	AudioCodec_init(hi2c);

}

void audioFrame(uint16_t buffer_offset)
{
	int i;
	int32_t current_sample = 0;
	sampleCounter = buffer_offset;

	frameCounter++;
	if (frameCounter > 1)
	{
		frameCounter = 0;
		buttonCheck();
	}

	//read the analog inputs and smooth them with ramps
	for (i = 0; i < 6; i++)
	{
		tRamp_setDest(&adc[i], (ADC_values[i] * INV_TWO_TO_16));
	}


	//if the codec isn't ready, keep the buffer as all zeros
	//otherwise, start computing audio!

	if (codecReady)
	{
		for (i = 0; i < (HALF_BUFFER_SIZE); i++)
		{
			if ((i & 1) == 0)
			{
				current_sample = ((int32_t)((audioTickR(((float) (audioInBuffer[buffer_offset + i] << 8)) * INV_TWO_TO_31, (adc_offset + (i / 2)))) * TWO_TO_23));
			}
			else
			{
				current_sample = ((int32_t)((audioTickL(((float) (audioInBuffer[buffer_offset + i] << 8)) * INV_TWO_TO_31, (adc_offset + (i / 2)))) * TWO_TO_23));
			}

			audioOutBuffer[buffer_offset + i] = current_sample;
		}
	}
}
float rightIn = 0.0f;


uint32_t readSample = 0;
float audioTickL(float audioIn, uint32_t sampleNum)
{

	sample = 0.0f;

	for (int i = 0; i < 6; i = i+2) // even numbered knobs (left side of board)
	{
		tCycle_setFreq(&mySine[i], (tRamp_tick(&adc[i]) * 500.0f) + 100.0f); // use knob to set frequency between 100 and 600 Hz
		sample += tCycle_tick(&mySine[i]); // tick the oscillator
	}
	sample *= 0.33f; // drop the gain because we've got three full volume sine waves summing here
	if (sampleNum < HALF_BUFFER_SIZE)
	{
		//sample = syncADC[0][sampleNum];
		//readSample = sampleNum;
	}
	return sample;
}


uint32_t myCounter = 0;

float audioTickR(float audioIn, uint32_t sampleNum)
{
	rightIn = audioIn;

	sample = 0.0f;


	for (int i = 0; i < 6; i = i+2) // odd numbered knobs (right side of board)
	{
		tCycle_setFreq(&mySine[i+1], (tRamp_tick(&adc[i+1]) * 500.0f) + 100.0f); // use knob to set frequency between 100 and 600 Hz
		sample += tCycle_tick(&mySine[i+1]); // tick the oscillator
	}
	sample *= 0.33f; // drop the gain because we've got three full volume sine waves summing here

	if (sampleNum < HALF_BUFFER_SIZE)
	{
		//sample = syncADC[1][sampleNum];
	}
	//sample = tNoise_tick(&noise); // or uncomment this to try white noise

	return sample;
}


uint8_t LED_States[3] = {0,0,0};
void buttonCheck(void)
{
	buttonValues[0] = !HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_6);
	buttonValues[1] = !HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_7);
	buttonValues[2] = !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11);
	for (int i = 0; i < NUM_BUTTONS; i++)
	{
	  if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] < 10))
	  {
		  buttonCounters[i]++;
	  }
	  if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] >= 10))
	  {
		  if (buttonValues[i] == 1)
		  {
			  buttonPressed[i] = 1;
		  }
		  buttonValuesPrev[i] = buttonValues[i];
		  buttonCounters[i] = 0;
	  }
	}

	if (buttonPressed[0] == 1)
	{

		if (LED_States[0] == 0)
		{
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
			LED_States[0] = 1;
		}
		else
		{
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
			LED_States[0] = 0;
		}
		buttonPressed[0] = 0;
	}
	if (buttonPressed[1] == 1)
	{
		if (LED_States[1] == 0)
		{
			//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
			LED_States[1] = 1;
		}
		else
		{
			//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
			LED_States[1] = 0;
		}
		buttonPressed[1] = 0;
	}

	if (buttonPressed[2] == 1)
	{
		if (LED_States[2] == 0)
		{
			//HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);
			LED_States[2] = 1;
		}
		else
		{
			//HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);
			LED_States[2] = 0;
		}
		buttonPressed[2] = 0;
	}
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
	;
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
	;
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  ;
}


void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(HALF_BUFFER_SIZE);
	sampleCounter = 0;
	adc_offset = AUDIO_FRAME_SIZE;
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(0);
	sampleCounter = AUDIO_FRAME_SIZE;
	adc_offset = 0;
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
}

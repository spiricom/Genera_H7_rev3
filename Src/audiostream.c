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

#define NUM_BUTTONS 2


//the audio buffers are put in the D2 RAM area because that is a memory location that the DMA has access to.
int32_t audioOutBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;
int32_t audioInBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;

void audioFrame(uint16_t buffer_offset);
float audioTickL(float audioIn);
float audioTickR(float audioIn);
void buttonCheck(void);

HAL_StatusTypeDef transmit_status;
HAL_StatusTypeDef receive_status;


uint8_t codecReady = 0;


uint8_t buttonValues[NUM_BUTTONS];
uint8_t buttonValuesPrev[NUM_BUTTONS];
uint32_t buttonCounters[NUM_BUTTONS];
uint32_t buttonPressed[NUM_BUTTONS];


float sample = 0.0f;

uint16_t frameCounter = 0;

//audio objects
tRamp adc[6];

tNoise noise;

tCycle mySine[2];

tWDF WDF_r;
tWDF WDF_r2;
tWDF WDF_r3;
tWDF WDF_c;
tWDF WDF_c2;
tWDF WDF_sa;
tWDF WDF_sa2;
tWDF WDF_pa;


/**********************************************/

typedef enum BOOL {
	FALSE = 0,
	TRUE
} BOOL;


void audioInit(I2C_HandleTypeDef* hi2c, SAI_HandleTypeDef* hsaiOut, SAI_HandleTypeDef* hsaiIn)
{
	// Initialize LEAF.

	LEAF_init(SAMPLE_RATE, AUDIO_FRAME_SIZE, &randomNumber);


	for (int i = 0; i < 6; i++)
	{
		tRamp_init(&adc[i],7.0f, 1); //set all ramps for knobs to be 7ms ramp time and let the init function know they will be ticked every sample

	}
	tNoise_init(&noise, WhiteNoise);
	tCycle_init(&mySine[0]);
	tCycle_setFreq(&mySine[0], 440.0f);
	tCycle_init(&mySine[1]);
	tCycle_setFreq(&mySine[1], 440.0f);

//	tWDF_init(tWDF* const r, WDFComponentType type, float value, tWDF* const rL, tWDF* const rR, float sample_rate);
	tWDF_init(&WDF_r, Resistor, 10000.0f, NULL, NULL, leaf.sampleRate);
	tWDF_init(&WDF_c, Capacitor, 0.000000159154943f, NULL, NULL, leaf.sampleRate);
	tWDF_init(&WDF_sa, SeriesAdaptor, 0.0f, &WDF_r, &WDF_c, leaf.sampleRate);
	tWDF_init(&WDF_c2, Capacitor, 0.000000159154943f, NULL, NULL, leaf.sampleRate);
	tWDF_init(&WDF_r2, Resistor, 10000.0f, NULL, NULL, leaf.sampleRate);
	tWDF_init(&WDF_pa, ParallelAdaptor, 0.0f, &WDF_c2, &WDF_r2, leaf.sampleRate);
	tWDF_init(&WDF_sa2, SeriesAdaptor, 0.0f, &WDF_sa, &WDF_pa, leaf.sampleRate);


	tWDF_init(&WDF_r3, Resistor, 10000.0f, NULL, NULL, leaf.sampleRate);




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

	frameCounter++;
	if (frameCounter >= 1)
	{
		frameCounter = 0;
		buttonCheck();
	}

	//if the codec isn't ready, keep the buffer as all zeros
	//otherwise, start computing audio!

	if (codecReady)
	{
		for (i = 0; i < (HALF_BUFFER_SIZE); i++)
		{
			if ((i & 1) == 0)
			{
				current_sample = (int32_t)(audioTickL((float) (audioInBuffer[buffer_offset + i] * INV_TWO_TO_23)) * TWO_TO_23);
			}
			else
			{
				current_sample = (int32_t)(audioTickR((float) (audioInBuffer[buffer_offset + i] * INV_TWO_TO_23)) * TWO_TO_23);
			}

			audioOutBuffer[buffer_offset + i] = current_sample;
		}
	}
}
float rightIn = 0.0f;


float audioTickL(float audioIn)
{
	//read the analog inputs and smooth them with ramps
	tRamp_setDest(&adc[0], (ADC_values[0] * INV_TWO_TO_16));
	tRamp_setDest(&adc[1], (ADC_values[1] * INV_TWO_TO_16));
	tRamp_setDest(&adc[2], (ADC_values[2] * INV_TWO_TO_16));
	tRamp_setDest(&adc[3], (ADC_values[3] * INV_TWO_TO_16));
	tRamp_setDest(&adc[4], (ADC_values[4] * INV_TWO_TO_16));
	tRamp_setDest(&adc[5], (ADC_values[5] * INV_TWO_TO_16));
	//sample = tNoise_tick(&noise);

	//tCycle_setFreq(&mySine[0], (tRamp_tick(&adc[0]) * 400.0f) + 100.0f);
	//sample = tCycle_tick(&mySine[0]);
	sample = audioIn;
	return sample;
}


uint32_t myCounter = 0;

float audioTickR(float audioIn)
{
	rightIn = audioIn;

	sample = 0.0f;

	//tCycle_setFreq(&mySine[0], (tRamp_tick(&adc[0]) * 400.0f) + 100.0f);
    //sample = tCycle_tick(&mySine[0]);

	tWDF_setValue(&WDF_r, (tRamp_tick(&adc[0]) * 10000.0f) + 1.0f);
	tWDF_setValue(&WDF_r2, (tRamp_tick(&adc[1]) * 10000.0f) + 1.0f);

	return tWDF_tick(&WDF_sa2, tNoise_tick(&noise), true);

//	tWDFresistor_setElectricalResistance(&WDF_r, (tRamp_tick(&adc[0]) * 10000.0f) + 100.0f);
//	tWDFseriesAdaptor_setPortResistances(&WDF_sa);
//	//step 2 : scan the waves up the tree
//	float incident_wave = tWDFseriesAdaptor_getReflectedWave(&WDF_sa);
//
//	//step 3 : do root scattering computation
//	float reflected_wave = (2.0f * sample) - incident_wave;
//
//	//step 4 : propigate waves down the tree
//	tWDFseriesAdaptor_setIncidentWave(&WDF_sa, reflected_wave);
//
//	//step 5 : grab whatever voltages or currents we want as outputs
//	sample = -1.0f * tWDFseriesAdaptor_getVoltage(&WDF_c);


	/*
	sample = tNoise_tick(&noise);
	tCycle_setFreq(&mySine[1], (tRamp_tick(&adc[1]) * 400.0f) + 100.0f);
	sample = tCycle_tick(&mySine[1]);
	if (myCounter > 22000)
	{
		sample = 0.0f;
	}
	if (myCounter > 44000)
	{
		myCounter = 0;
	}
	myCounter++;
	*/
	return sample;
}


void buttonCheck(void)
{
	buttonValues[0] = !HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_6);
	buttonValues[1] = !HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_7);

	for (int i = 0; i < 2; i++)
	{
	  if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] < 40))
	  {
		  buttonCounters[i]++;
	  }
	  if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] >= 40))
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
		buttonPressed[0] = 0;
	}
	if (buttonPressed[1] == 1)
	{
		buttonPressed[1] = 0;
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
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(0);
}

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
#include "ui.h"
#include "tunings.h"
#include "i2c.h"
#include "gpio.h"
#include "sfx.h"

//the audio buffers are put in the D2 RAM area because that is a memory location that the DMA has access to.
int32_t audioOutBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;
int32_t audioInBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;

#define SMALL_MEM_SIZE 512
#define MED_MEM_SIZE 500000
#define LARGE_MEM_SIZE 33554432 //32 MBytes - size of SDRAM IC

tMempool small_pool;
tMempool large_pool;

char small_memory[SMALL_MEM_SIZE];
char medium_memory[MED_MEM_SIZE]__ATTR_RAM_D1;
char large_memory[LARGE_MEM_SIZE] __ATTR_SDRAM;

void audioFrame(uint16_t buffer_offset);
float audioTickL(float audioIn);
float audioTickR(float audioIn);
void buttonCheck(void);

HAL_StatusTypeDef transmit_status;
HAL_StatusTypeDef receive_status;

uint8_t codecReady = 0;

uint16_t frameCounter = 0;

tMempool smallPool;
tMempool largePool;

tRamp adc[6];

tNoise myNoise;
tCycle mySine[2];
float smoothedADC[6];
float floatADC[6];
float lastFloatADC[6];
float hysteresisThreshold = 0.001f;

uint8_t writeParameterFlag = 0;


uint32_t clipCounter[4] = {0,0,0,0};
uint8_t clipped[4] = {0,0,0,0};


float rightIn = 0.0f;
float rightOut = 0.0f;
float sample = 0.0f;



// Vocoder
float glideTimeVoc = 5.0f;

// Formant
float formantShiftFactor = -1.0f;
float formantKnob = 0.0f;

// PitchShift
float pitchFactor = 2.0f;
float formantWarp = 1.0f;
float formantIntensity = 1.0f;

// Autotune1

// Autotune2
float glideTimeAuto = 5.0f;

// Sampler Button Press


// Sampler Auto Grab






BOOL frameCompleted = TRUE;

BOOL bufferCleared = TRUE;

int numBuffersToClearOnLoad = 2;
int numBuffersCleared = 0;

/**********************************************/


void (*allocFunctions[PresetNil])(void);
void (*frameFunctions[PresetNil])(void);
void  (*tickFunctions[PresetNil])(float);
void (*freeFunctions[PresetNil])(void);

void audioInit(I2C_HandleTypeDef* hi2c, SAI_HandleTypeDef* hsaiOut, SAI_HandleTypeDef* hsaiIn)
{
	// Initialize LEAF.

	LEAF_init(SAMPLE_RATE, AUDIO_FRAME_SIZE, medium_memory, MED_MEM_SIZE, &randomNumber);


	tMempool_init(&small_pool, small_memory, SMALL_MEM_SIZE);
	tMempool_init(&small_pool, large_memory, LARGE_MEM_SIZE);



	initFunctionPointers();
	tNoise_init(&myNoise, WhiteNoise);
	tCycle_init(&mySine[0]);
	tCycle_setFreq(&mySine[0],2200.0f);
	//ramps to smooth the knobs
	for (int i = 0; i < 6; i++)
	{
		tRamp_init(&adc[i],19.0f, 1); //set all ramps for knobs to be 9ms ramp time and let the init function know they will be ticked every sample
	}

	initGlobalSFXObjects();

	allocFunctions[currentPreset]();

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
	HAL_Delay(1);

	//now reconfigue so buttons C and E can be used (they were also connected to I2C for codec setup)
	HAL_I2C_MspDeInit(hi2c);

	GPIO_InitTypeDef GPIO_InitStruct = {0};

    //PB10, PB11     ------> buttons C and E
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void audioFrame(uint16_t buffer_offset)
{
	frameCompleted = FALSE;

	int i;
	int32_t current_sample;

	buttonCheck();


	//read the analog inputs and smooth them with ramps
	for (i = 0; i < 6; i++)
	{
		//floatADC[i] = (float) (ADC_values[i]>>8) * INV_TWO_TO_8;
		floatADC[i] = (float) (ADC_values[i]>>6) * INV_TWO_TO_10;


		if (fastabsf(floatADC[i] - lastFloatADC[i]) > hysteresisThreshold)
		{
			lastFloatADC[i] = floatADC[i];
			writeParameterFlag = i+1;
		}
		tRamp_setDest(&adc[i], floatADC[i]);
	}


	if (!loadingPreset)
	{

		frameFunctions[currentPreset]();
	}


	//if the codec isn't ready, keep the buffer as all zeros
	//otherwise, start computing audio!

	bufferCleared = TRUE;

	if (codecReady)
	{
		for (i = 0; i < (HALF_BUFFER_SIZE); i++)
		{
			if ((i & 1) == 0)
			{
				current_sample = (int32_t)(audioTickR((float) (audioInBuffer[buffer_offset + i] << 8) * INV_TWO_TO_31) * TWO_TO_23);
			}
			else
			{
				current_sample = (int32_t)(audioTickL((float) (audioInBuffer[buffer_offset + i] << 8) * INV_TWO_TO_31) * TWO_TO_23);
			}

			audioOutBuffer[buffer_offset + i] = current_sample;
		}

	}

	if (bufferCleared)
	{
		numBuffersCleared++;
		if (numBuffersCleared >= numBuffersToClearOnLoad)
		{
			numBuffersCleared = numBuffersToClearOnLoad;
			if (loadingPreset)
			{
				if (previousPreset != PresetNil)
				{
					freeFunctions[previousPreset]();
				}
				allocFunctions[currentPreset]();
				loadingPreset = 0;
				//OLED_draw();
			}
		}
	}
	else numBuffersCleared = 0;

	frameCompleted = TRUE;

}




float audioTickL(float audioIn)
{
	sample = 0.0f;
	//audioIn = tanhf(audioIn);


	for (int i = 0; i < 6; i++)
	{
		smoothedADC[i] = tRamp_tick(&adc[i]);
	}

	if (loadingPreset) return sample;

	bufferCleared = FALSE;


	tickFunctions[currentPreset](audioIn);



	if ((audioIn >= 0.999999f) || (audioIn <= -0.999999f))
	{
		setLED_leftin_clip(1);
		clipCounter[0] = 10000;
		clipped[0] = 1;
	}
	if ((clipCounter[0] > 0) && (clipped[0] == 1))
	{
		clipCounter[0]--;
	}
	else if ((clipCounter[0] == 0) && (clipped[0] == 1))
	{
		setLED_leftin_clip(0);
		clipped[0] = 0;
	}



	if ((sample >= 0.999999f) || (sample <= -0.999999f))
	{
		setLED_leftout_clip(1);
		clipCounter[2] = 10000;
		clipped[2] = 1;
	}
	if ((clipCounter[2] > 0) && (clipped[2] == 1))
	{
		clipCounter[2]--;
	}
	else if ((clipCounter[2] == 0) && (clipped[2] == 1))
	{
		setLED_leftout_clip(0);
		clipped[2] = 0;
	}

	return sample;
}



float audioTickR(float audioIn)
{
	rightIn = audioIn;


	if ((rightIn >= 0.999999f) || (rightIn <= -0.999999f))
	{
		setLED_rightin_clip(1);
		clipCounter[1] = 10000;
		clipped[1] = 1;
	}
	if ((clipCounter[1] > 0) && (clipped[1] == 1))
	{
		clipCounter[1]--;
	}
	else if ((clipCounter[1] == 0) && (clipped[1] == 1))
	{
		setLED_rightin_clip(0);
		clipped[1] = 0;
	}



	if ((rightOut >= 0.999999f) || (rightOut <= -0.999999f))
	{
		setLED_rightout_clip(1);
		clipCounter[3] = 10000;
		clipped[3] = 1;
	}
	if ((clipCounter[3] > 0) && (clipped[3] == 1))
	{
		clipCounter[3]--;
	}
	else if ((clipCounter[3] == 0) && (clipped[3] == 1))
	{
		setLED_rightout_clip(0);
		clipped[3] = 0;
	}
	return rightOut;
}

void freePreset(VocodecPreset preset)
{



}

void allocPreset(VocodecPreset preset)
{

}


static void initFunctionPointers(void)
{
	allocFunctions[VocoderInternalPoly] = SFXVocoderIPAlloc;
	frameFunctions[VocoderInternalPoly] = SFXVocoderIPFrame;
	tickFunctions[VocoderInternalPoly] = SFXVocoderIPTick;
	freeFunctions[VocoderInternalPoly] = SFXVocoderIPFree;

	allocFunctions[VocoderInternalMono] = SFXVocoderIMAlloc;
	frameFunctions[VocoderInternalMono] = SFXVocoderIMFrame;
	tickFunctions[VocoderInternalMono] = SFXVocoderIMTick;
	freeFunctions[VocoderInternalMono] = SFXVocoderIMFree;

	allocFunctions[VocoderExternal] = SFXVocoderEAlloc;
	frameFunctions[VocoderExternal] = SFXVocoderEFrame;
	tickFunctions[VocoderExternal] = SFXVocoderETick;
	freeFunctions[VocoderExternal] = SFXVocoderEFree;

	allocFunctions[Pitchshift] = SFXPitchShiftAlloc;
	frameFunctions[Pitchshift] = SFXPitchShiftFrame;
	tickFunctions[Pitchshift] = SFXPitchShiftTick;
	freeFunctions[Pitchshift] = SFXPitchShiftFree;

	allocFunctions[AutotuneMono] = SFXNeartuneAlloc;
	frameFunctions[AutotuneMono] = SFXNeartuneFrame;
	tickFunctions[AutotuneMono] = SFXNeartuneTick;
	freeFunctions[AutotuneMono] = SFXNeartuneFree;

	allocFunctions[AutotunePoly] = SFXAutotuneAlloc;
	frameFunctions[AutotunePoly] = SFXAutotuneFrame;
	tickFunctions[AutotunePoly] = SFXAutotuneTick;
	freeFunctions[AutotunePoly] = SFXAutotuneFree;

	allocFunctions[SamplerButtonPress] = SFXSamplerBPAlloc;
	frameFunctions[SamplerButtonPress] = SFXSamplerBPFrame;
	tickFunctions[SamplerButtonPress] = SFXSamplerBPTick;
	freeFunctions[SamplerButtonPress] = SFXSamplerBPFree;

	allocFunctions[SamplerAutoGrabInternal] = SFXSamplerAuto1Alloc;
	frameFunctions[SamplerAutoGrabInternal] = SFXSamplerAuto1Frame;
	tickFunctions[SamplerAutoGrabInternal] = SFXSamplerAuto1Tick;
	freeFunctions[SamplerAutoGrabInternal] = SFXSamplerAuto1Free;

	allocFunctions[SamplerAutoGrabExternal] = SFXSamplerAuto2Alloc;
	frameFunctions[SamplerAutoGrabExternal] = SFXSamplerAuto2Frame;
	tickFunctions[SamplerAutoGrabExternal] = SFXSamplerAuto2Tick;
	freeFunctions[SamplerAutoGrabExternal] = SFXSamplerAuto2Free;

	allocFunctions[DistortionTanH] = SFXDistortionTanhAlloc;
	frameFunctions[DistortionTanH] = SFXDistortionTanhFrame;
	tickFunctions[DistortionTanH] = SFXDistortionTanhTick;
	freeFunctions[DistortionTanH] = SFXDistortionTanhFree;

	allocFunctions[DistortionShaper] = SFXDistortionShaperAlloc;
	frameFunctions[DistortionShaper] = SFXDistortionShaperFrame;
	tickFunctions[DistortionShaper] = SFXDistortionShaperTick;
	freeFunctions[DistortionShaper] = SFXDistortionShaperFree;

	allocFunctions[Wavefolder] = SFXWaveFolderAlloc;
	frameFunctions[Wavefolder] = SFXWaveFolderFrame;
	tickFunctions[Wavefolder] = SFXWaveFolderTick;
	freeFunctions[Wavefolder] = SFXWaveFolderFree;

	allocFunctions[BitCrusher] = SFXBitcrusherAlloc;
	frameFunctions[BitCrusher] = SFXBitcrusherFrame;
	tickFunctions[BitCrusher] = SFXBitcrusherTick;
	freeFunctions[BitCrusher] = SFXBitcrusherFree;

	allocFunctions[Delay] = SFXDelayAlloc;
	frameFunctions[Delay] = SFXDelayFrame;
	tickFunctions[Delay] = SFXDelayTick;
	freeFunctions[Delay] = SFXDelayFree;

	allocFunctions[Reverb] = SFXReverbAlloc;
	frameFunctions[Reverb] = SFXReverbFrame;
	tickFunctions[Reverb] = SFXReverbTick;
	freeFunctions[Reverb] = SFXReverbFree;

	allocFunctions[Reverb2] = SFXReverb2Alloc;
	frameFunctions[Reverb2] = SFXReverb2Frame;
	tickFunctions[Reverb2] = SFXReverb2Tick;
	freeFunctions[Reverb2] = SFXReverb2Free;

	allocFunctions[LivingString] = SFXLivingStringAlloc;
	frameFunctions[LivingString] = SFXLivingStringFrame;
	tickFunctions[LivingString] = SFXLivingStringTick;
	freeFunctions[LivingString] = SFXLivingStringFree;
}



void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
	if (!frameCompleted)
	{
		setLED_C(1);
	}
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
	if (!frameCompleted)
		{
			setLED_C(1);
		}
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	if (!frameCompleted)
		{
			setLED_C(1);
		}
}


void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
	if (!frameCompleted)
	{
		setLED_C(1);
	}

	audioFrame(HALF_BUFFER_SIZE);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	if (!frameCompleted)
	{
		setLED_C(1);
	}

	audioFrame(0);
}

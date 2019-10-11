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

float sample = 0.0f;

uint16_t frameCounter = 0;

tRamp adc[6];
float smoothedADC[6];

#define NUM_VOC_VOICES 8
#define NUM_VOC_OSC 4
#define INV_NUM_VOC_VOICES 0.125
#define INV_NUM_VOC_OSC 0.25
#define NUM_PS 1

//audio objects
tFormantShifter fs;
tPeriod p;
tPitchShift pshift[NUM_PS];
tRamp ramp[NUM_VOC_VOICES];
tPoly poly;
tSawtooth osc[NUM_VOC_VOICES][NUM_VOC_OSC];
tTalkbox vocoder;
tRamp comp;
//tHighpass highpass1;
//tHighpass highpass2;
tSVF lowpassVoc;
//tSVF lowpassSyn;

float nearestPeriod(float period);
void calculateFreq(int voice);

float notePeriods[128];
int chordArray[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int lockArray[12];
float freq[NUM_VOC_VOICES];
float detuneAmounts[NUM_VOC_VOICES][NUM_VOC_OSC];
float detuneSeeds[NUM_VOC_VOICES][NUM_VOC_OSC];
float centsDeviation[12] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
int keyCenter = 5;


float inBuffer[2048] __ATTR_RAM_D2;
float outBuffer[NUM_PS][2048] __ATTR_RAM_D2;

// Vocoder
float glideTimeVoc = 5.0f;
float lpFreqVoc = 10000.0f;
float detuneMaxVoc = 0.0f;

// Formant
float formantShiftFactor = -1.0f;
float formantKnob = 0.0f;

// PitchShift
float pitchFactor = 2.0f;
float formantShiftFactorPS = 0.0f;

//// Autotune1
//
//// Autotune2
//float glideTimeAuto = 5.0f;

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

//	tHighpass_init(&highpass1, 20.0f);
//	tHighpass_init(&highpass2, 20.0f);

	for (int i = 0; i < 128; i++)
	{
		notePeriods[i] = 1.0f / LEAF_midiToFrequency(i) * leaf.sampleRate;
	}

	tFormantShifter_init(&fs);

	tPoly_init(&poly, NUM_VOC_VOICES);
	tPoly_setPitchGlideTime(&poly, 50.0f);
//	numActiveVoices[VocoderMode] = 1;
//	numActiveVoices[AutotuneAbsoluteMode] = 1;
//	numActiveVoices[SynthMode] = 1;
	tTalkbox_init(&vocoder);
	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		for (int j = 0; j < NUM_VOC_OSC; j++)
		{
			detuneSeeds[i][j] = randomNumber();
			tSawtooth_init(&osc[i][j]);
		}
	}

	tRamp_init(&comp, 10.0f, 1);

	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		tRamp_init(&ramp[i], 10.0f, 1);
	}

	tPeriod_init(&p, inBuffer, outBuffer[0], 2048, PS_FRAME_SIZE);
	tPeriod_setWindowSize(&p, ENV_WINDOW_SIZE);
	tPeriod_setHopSize(&p, ENV_HOP_SIZE);


	for (int i = 0; i < NUM_PS; ++i)
	{
		tPitchShift_init(&pshift[i], &p, outBuffer[i], 2048);
	}

	tSVF_init(&lowpassVoc, SVFTypeLowpass, 20000.0f, 1.0f);
//	tSVF_init(&lowpassSyn, SVFTypeLowpass, 20000.0f, 1.0f);


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

	//tPoly_setNumVoices(poly, numActiveVoices[VocoderMode]);
	if (currentPreset == VocoderInternal || currentPreset == VocoderExternal)
	{
		glideTimeVoc = 5.0f;
		//glideTimeVoc = (smoothedADC[0] * 999.0f) + 0.1f;
		lpFreqVoc = (smoothedADC[2] * 17600.0f) + 400.0f;
		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			detuneMaxVoc = smoothedADC[3] * freq[i] * 0.05f;
		}
		tPoly_setPitchGlideTime(&poly, glideTimeVoc);
		tSVF_setFreq(&lowpassVoc, lpFreqVoc);
		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			tRamp_setDest(&ramp[i], (tPoly_getVelocity(&poly, i) > 0));
			calculateFreq(i);
			for (int j = 0; j < NUM_VOC_OSC; j++)
			{
				detuneAmounts[i][j] = (detuneSeeds[i][j] * detuneMaxVoc) - (detuneMaxVoc * 0.5f);
				tSawtooth_setFreq(&osc[i][j], freq[i] + detuneAmounts[i][j]);
			}
		}

		if (poly.stack->size != 0) tRamp_setDest(&comp, 1.0f / poly.stack->size);
	}
	else if (currentPreset == Pitchshift)
	{

	}
	else if (currentPreset == AutotuneMono)
	{

	}
	else if (currentPreset == AutotunePoly)
	{

	}

	//if the codec isn't ready, keep the buffer as all zeros
	//otherwise, start computing audio!

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
}
float rightIn = 0.0f;


float audioTickL(float audioIn)
{
	for (int i = 0; i < 6; i++)
	{
		smoothedADC[i] = tRamp_tick(&adc[i]);
	}

	sample = 0.0f;

	if (currentPreset == VocoderInternal)
	{
		tPoly_tickPitch(&poly);

		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			for (int j = 0; j < NUM_VOC_OSC; j++)
			{
				sample += tSawtooth_tick(&osc[i][j]) * tRamp_tick(&ramp[i]);
			}
		}

		sample *= INV_NUM_VOC_OSC * 0.5f * tRamp_tick(&comp);
		sample = tTalkbox_tick(&vocoder, sample, audioIn);
		//sample = tSVF_tick(&lowpassVoc, sample);
		sample = tanhf(sample);
	}
	else if (currentPreset == VocoderExternal)
	{

	}
	else if (currentPreset == Pitchshift)
	{

	}
	else if (currentPreset == AutotuneMono)
	{

	}
	else if (currentPreset == AutotunePoly)
	{

	}

	return sample;
}


uint32_t myCounter = 0;

float audioTickR(float audioIn)
{
	rightIn = audioIn;

	sample = 0.0f;

	return sample;
}

void calculateFreq(int voice)
{

	float tempNote = tPoly_getPitch(&poly, voice);
	float tempPitchClass = ((((int)tempNote) - keyCenter) % 12 );
	float tunedNote = tempNote + centsDeviation[(int)tempPitchClass];
	freq[voice] = LEAF_midiToFrequency(tunedNote);

}

//float nearestPeriod(float period)
//{
//
//	float leastDifference = fabsf(period - notePeriods[0]);
//	float difference;
//	int index = -1;
//
//	int* chord = chordArray;
//	//if (autotuneLock > 0) chord = lockArray;
//
//	for(int i = 0; i < 128; i++)
//	{
//		if (chord[i%12] > 0)
//		{
//			difference = fabsf(period - notePeriods[i]);
//			if(difference < leastDifference)
//			{
//				leastDifference = difference;
//				index = i;
//			}
//		}
//	}
//
//	if (index == -1) return period;
//
//	return notePeriods[index];
//
//}

void noteOn(int key, int velocity)
{
	if (!velocity)
	{
		if (chordArray[key%12] > 0) chordArray[key%12]--;

		int voice = tPoly_noteOff(&poly, key);
		if (voice >= 0) tRamp_setDest(&ramp[voice], 0.0f);

		for (int i = 0; i < poly.numVoices; i++)
		{
			if (tPoly_isOn(&poly, i) == 1)
			{
				tRamp_setDest(&ramp[i], 1.0f);
				calculateFreq(i);
			}
		}
		setLED_USB(0);
	}
	else
	{
		chordArray[key%12]++;

		tPoly_noteOn(&poly, key, velocity);

		for (int i = 0; i < poly.numVoices; i++)
		{
			if (tPoly_isOn(&poly, i) == 1)
			{
				tRamp_setDest(&ramp[i], 1.0f);
				calculateFreq(i);
			}
		}
		setLED_USB(1);
	}
}

void noteOff(int key, int velocity)
{
	if (chordArray[key%12] > 0) chordArray[key%12]--;

	int voice = tPoly_noteOff(&poly, key);
	if (voice >= 0) tRamp_setDest(&ramp[voice], 0.0f);

	for (int i = 0; i < poly.numVoices; i++)
	{
		if (tPoly_isOn(&poly, i) == 1)
		{
			tRamp_setDest(&ramp[i], 1.0f);
			calculateFreq(i);
		}
	}
	setLED_USB(0);
}

void sustainOff()
{

}

void sustainOn()
{

}

void toggleBypass()
{

}

void toggleSustain()
{

}

void ctrlInput(int ctrl, int value)
{

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

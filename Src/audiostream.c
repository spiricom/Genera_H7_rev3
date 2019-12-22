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

#define MEM_SIZE 500000
char memory[MEM_SIZE] __ATTR_RAM_D1;

void audioFrame(uint16_t buffer_offset);
float audioTickL(float audioIn);
float audioTickR(float audioIn);
void buttonCheck(void);

HAL_StatusTypeDef transmit_status;
HAL_StatusTypeDef receive_status;

uint8_t codecReady = 0;

uint16_t frameCounter = 0;

tRamp adc[6];

//tCycle mySine[2];

float targetADC[6];
float smoothedADC[6];
float hysteresisThreshold = 0.00f;

#define NUM_VOC_VOICES 8
#define NUM_VOC_OSC 4
#define INV_NUM_VOC_VOICES 0.125
#define INV_NUM_VOC_OSC 0.25
#define NUM_AUTOTUNE 8
#define NUM_RETUNE 1

//audio objects
tFormantShifter fs;
tAutotune autotuneMono;
tAutotune autotunePoly;
tRetune retune;
tRamp nearRamp;
tPoly poly;
tRamp polyRamp[NUM_VOC_VOICES];
tSawtooth osc[NUM_VOC_VOICES][NUM_VOC_OSC];
tTalkbox vocoder;
tTalkbox vocoder2;
tRamp comp;
tSVF lowpassVoc;
tCrusher dist;
tCycle mySine;
tBuffer buff;
tSampler sampler;
tPitchShift pitchDown;
tDattorroReverb rev;

float nearestPeriod(float period);
void calculateFreq(int voice);

float notePeriods[128];
float noteFreqs[128];
int chordArray[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int lockArray[12];
float freq[NUM_VOC_VOICES];
float detuneAmounts[NUM_VOC_VOICES][NUM_VOC_OSC];
float detuneSeeds[NUM_VOC_VOICES][NUM_VOC_OSC];
float centsDeviation[12] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
int keyCenter = 5;

// Vocoder
float glideTimeVoc = 5.0f;
float lpFreqVoc = 10000.0f;
float detuneMaxVoc = 0.0f;

// Formant
float formantShiftFactor = -1.0f;
float formantKnob = 0.0f;

// PitchShift
float pitchFactor = 2.0f;
float formantWarp = 1.0f;
float formantIntensity = 1.0f;


// Octaver
float pitchFactorB = 2.0f;
float pitchShiftB = -12.0f;


// Autotune1

// Autotune2
float glideTimeAuto = 5.0f;

// Sampler
int samplePlayStart = 0.0f;
int samplePlayEnd = 0.0f;
int sampleLength = 0.0f;
float samplerRate = 1.0f;
float maxSampleSizeSeconds = 2.5f;

int sinecount = 0;

uint8_t tickCompleted = 1;

uint8_t bufferCleared = 1;

int numBuffersToClearOnLoad = 2;
int numBuffersCleared = 0;

/**********************************************/

typedef enum BOOL {
	FALSE = 0,
	TRUE = 1
} BOOL;


void audioInit(I2C_HandleTypeDef* hi2c, SAI_HandleTypeDef* hsaiOut, SAI_HandleTypeDef* hsaiIn)
{
	// Initialize LEAF.

	LEAF_init(SAMPLE_RATE, AUDIO_FRAME_SIZE, memory, MEM_SIZE, &randomNumber);


	for (int i = 0; i < 6; i++)
	{
		tRamp_init(&adc[i],7.0f, 1); //set all ramps for knobs to be 7ms ramp time and let the init function know they will be ticked every sample
	}

	for (int i = 0; i < 128; i++)
	{
		notePeriods[i] = 1.0f / LEAF_midiToFrequency(i) * leaf.sampleRate;
		//		noteFreqs[i] = LEAF_midiToFrequency(i);
	}

	tPoly_init(&poly, NUM_VOC_VOICES);
	tPoly_setPitchGlideTime(&poly, 50.0f);
	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		tRamp_init(&polyRamp[i], 10.0f, 1);
	}

	tRamp_init(&nearRamp, 10.0f, 1);
	tRamp_init(&comp, 10.0f, 1);

	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		for (int j = 0; j < NUM_VOC_OSC; j++)
		{
			detuneSeeds[i][j] = randomNumber();
			tSawtooth_init(&osc[i][j]);
		}
	}
	tSawtooth_setFreq(&osc[0][0], 200);

	tSVF_init(&lowpassVoc, SVFTypeLowpass, lpFreqVoc, 1.0f);

	allocPreset(currentPreset);
	//	freePreset(currentPreset);
	//	allocPreset(currentPreset);

	//	tTalkbox_init(&vocoder, 1024);
	//
	//	tFormantShifter_init(&fs, 2048, 7);
	//	tRetune_init(&retune, NUM_RETUNE, 2048, 1024);
	//
	//	tAutotune_init(&autotuneMono, 1, 2048, 1024);
	//
	//	tAutotune_init(&autotunePoly, NUM_AUTOTUNE, 2048, 1024);


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
	//	if (!tickCompleted)
	//	{
	//		setLED_USB(0);
	//	}

	int i;
	int32_t current_sample = 0;

	frameCounter++;
	if (frameCounter > 1)
	{
		frameCounter = 0;
		buttonCheck();
	}

	float floatADC[6];
	//read the analog inputs and smooth them with ramps
	for (i = 0; i < 6; i++)
	{
		floatADC[i] = (float) ADC_values[i] * INV_TWO_TO_16;
		if (fabsf(floatADC[i] - targetADC[i]) > hysteresisThreshold)
		{
			targetADC[i] = floatADC[i];
			tRamp_setDest(&adc[i], targetADC[i]);
		}
	}
	// Set up preset parameters*******************************************************
	if (!loadingPreset)
	{
		if (currentPreset == VocoderInternal)
		{
			tPoly_setNumVoices(&poly, NUM_VOC_VOICES);
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
				tRamp_setDest(&polyRamp[i], (tPoly_getVelocity(&poly, i) > 0));
				calculateFreq(i);
				for (int j = 0; j < NUM_VOC_OSC; j++)
				{
					detuneAmounts[i][j] = (detuneSeeds[i][j] * detuneMaxVoc) - (detuneMaxVoc * 0.5f);
					tSawtooth_setFreq(&osc[i][j], freq[i] + detuneAmounts[i][j]);
				}
			}

			if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0f / tPoly_getNumActiveVoices(&poly));
		}
		if (currentPreset == VocoderExternal)
		{

		}
		else if (currentPreset == Pitchshift)
		{

			pitchFactor = (smoothedADC[0]*3.75f)+0.25f;
			tRetune_setPitchFactor(&retune, pitchFactor, 0);
			formantWarp = (smoothedADC[1]*3.75f)+0.25f;
			float scaleUp = (smoothedADC[2]) * 20.0f;
			tFormantShifter_setShiftFactor(&fs, formantWarp);
			tFormantShifter_setIntensity(&fs, scaleUp);
			uiPitchFactor = pitchFactor;
			uiFormantWarp = formantWarp;
		}
		else if (currentPreset == Octaver)
		{
			float pitchShiftB = -13.0f;
			//float pitchDown = -12.0f;
		}
		else if (currentPreset == AutotuneMono)
		{
			tAutotune_setFreq(&autotuneMono, leaf.sampleRate / nearestPeriod(tAutotune_getInputPeriod(&autotuneMono)), 0);
			if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&nearRamp, 1.0f);
			else tRamp_setDest(&nearRamp, 0.0f);
		}
		else if (currentPreset == AutotunePoly)
		{
			tPoly_setNumVoices(&poly, NUM_AUTOTUNE);
			for (int i = 0; i < tPoly_getNumVoices(&poly); ++i)
			{
				calculateFreq(i);
			}
			if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0f / tPoly_getNumActiveVoices(&poly));
		}
		else if (currentPreset == Sampler)
		{
			if (buttonPressed[3])
			{
				tSampler_stop(&sampler);
				tBuffer_record(&buff);
			}
			else if (buttonReleased[3])
			{
				tBuffer_stop(&buff);
				sampleLength = tBuffer_getRecordPosition(&buff);
				tSampler_play(&sampler);
			}
			if (buttonPressed[4])
			{
				tBuffer_clear(&buff);
			}
			samplePlayStart = smoothedADC[0] * sampleLength;
			samplePlayEnd = smoothedADC[1] * sampleLength;
			samplerRate = (smoothedADC[2] - 0.5f) * 40.0f;
			if (smoothedADC[3] > 0.5f)
			{
				tSampler_setStart(&sampler, 44832.4297f);
				tSampler_setEnd(&sampler, 119996.906f);
				tSampler_setRate(&sampler, 1.24937773f);
			}
			else
			{
				tSampler_setStart(&sampler, samplePlayStart); //44832.4297
				tSampler_setEnd(&sampler, samplePlayEnd);// 119996.906
				tSampler_setRate(&sampler, samplerRate); //1.24937773
			}
			//			tSampler_setCrossfadeLength(&sampler, 500);
		}
	}
// End set up preset parameters******************************************************
	//if the codec isn't ready, keep the buffer as all zeros
	//otherwise, start computing audio!

	bufferCleared = 1;

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
				freePreset(previousPreset);
				allocPreset(currentPreset);
				loadingPreset = 0;
				OLED_draw();
			}
		}
	}
	else numBuffersCleared = 0;
}
float rightIn = 0.0f;


float audioTickL(float audioIn)
{
	float sample = 0.0f;

	audioIn = tanhf(audioIn);

	for (int i = 0; i < 6; i++)
	{
		smoothedADC[i] = tRamp_tick(&adc[i]);
	}

	if (loadingPreset) return sample;

	bufferCleared = 0;

	//Create Presets**************************************************

	if (currentPreset == VocoderInternal)
	{
		tPoly_tickPitch(&poly);

		for (int i = 0; i < NUM_VOC_VOICES; i++)
		{
			for (int j = 0; j < NUM_VOC_OSC; j++)
			{
				sample += tSawtooth_tick(&osc[i][j]) * tRamp_tick(&polyRamp[i]);
			}
		}

		sample *= INV_NUM_VOC_OSC * tRamp_tick(&comp);
		sample = tTalkbox_tick(&vocoder, sample, audioIn);
		//sample = tSVF_tick(&lowpassVoc, sample);
		sample = tanhf(sample);
	}
	else if (currentPreset == VocoderExternal)
	{
		tickCompleted = 0;
		//		sample = tTalkbox_tick(&vocoder2, tSawtooth_tick(&osc[0][0]), audioIn);
		//		if (sample == 0.0f)
		//		{
		//			setLED_USB(1);
		//		}
		sample = tTalkbox_tick(&vocoder2, rightIn, audioIn);
		//sample = tSVF_tick(&lowpassVoc, sample);
		sample = tanhf(sample);
		tickCompleted = 1;
	}
	else if (currentPreset == Pitchshift)
	{
		sample = tFormantShifter_remove(&fs, audioIn);

		float* samples = tRetune_tick(&retune, sample);
		sample = samples[0];

		sample = tFormantShifter_add(&fs, sample);
	}
	else if (currentPreset == AutotuneMono)
	{
		float* samples = tAutotune_tick(&autotuneMono, audioIn);
		sample = samples[0] * tRamp_tick(&nearRamp);
	}
	else if (currentPreset == AutotunePoly)
	{
		tPoly_tickPitch(&poly);

		for (int i = 0; i < tPoly_getNumVoices(&poly); ++i)
		{
			tAutotune_setFreq(&autotunePoly, freq[i], i);
		}

		float* samples = tAutotune_tick(&autotunePoly, audioIn);

		for (int i = 0; i < tPoly_getNumVoices(&poly); ++i)
		{
			sample += samples[i] * tRamp_tick(&polyRamp[i]);
		}
		sample *= tRamp_tick(&comp);
	}
	else if (currentPreset == Sampler)
	{
		tBuffer_tick(&buff, audioIn);
		sample = tSampler_tick(&sampler);
	}
	else if (currentPreset == RingMod)
	{

		//			int i = 0;
		//			int mod_freq = 650;
		//			//sample = tCycle_setFreq(&mySine, audioIn);
		//
		//			while(!feof(&buff)) {
		//			       tBuffer_tick(&buff, audioIn);
		//			        float sample = &buff[0] * sin(2 * M_PI * mod_freq * i / 4499);
		//			        i++; if (i == 44100) i = 0;
		//			       // fwrite(&sample, sizeof(float), 1, out);
		//			    }
	}

	else if (currentPreset == Distort)
	{
		sample = tCrusher_tick(&dist, audioIn);
	}
	else if (currentPreset == Octaver)
	{
		sample = tPitchShift_shiftToFreq(&pitchShiftB, audioIn);

		//sample = samples[0];
	}
//	else if (currentPreset == Fear)
//	{
//		sample = tDattorroReverb_tick(&rev, audioIn);
//
//	}

	return tanhf(sample);
}


uint32_t myCounter = 0;

float audioTickR(float audioIn)
{
	rightIn = audioIn;

	float sample = 0.0f;
	//test code
	//tCycle_setFreq(&mySine[1], 400.0f);
	//sample = tCycle_tick(&mySine[1]);
	return sample;
}
// Free Memory Used By Presets*********************************************

void freePreset(VocodecPreset preset)
{
	if (preset == VocoderInternal)
	{
		tTalkbox_free(&vocoder);
	}
	else if (preset == VocoderExternal)
	{
		tTalkbox_free(&vocoder2);
	}
	else if (preset == Pitchshift)
	{
		tFormantShifter_free(&fs);
		tRetune_free(&retune);
	}
	else if (preset == AutotuneMono)
	{
		tAutotune_free(&autotuneMono);
	}
	else if (preset == AutotunePoly)
	{
		tAutotune_free(&autotunePoly);
	}
	else if (preset == Sampler)
	{
		tBuffer_free(&buff);
		tSampler_free(&sampler);
	}
	else if (preset == RingMod)
	{
		tCycle_free(&mySine);
	}
	else if (preset == Distort)
	{
		tCrusher_free(&dist);
	}
	else if (preset == Octaver)
	{
		tPitchShift_free(&pitchShiftB);
	}

//	else if (preset == Fear)
//	{
//		tDattorroReverb_free(&rev);
//	}
}
//Allocate Memory To Preset**********************************************

void allocPreset(VocodecPreset preset)
{
	if (preset == VocoderInternal)
	{
		for (int i = 0; i < NUM_VOC_VOICES; i++)
		{
			for (int j = 0; j < NUM_VOC_OSC; j++)
			{
				detuneSeeds[i][j] = randomNumber();
			}
		}
		tTalkbox_init(&vocoder, 1024);
	}
	else if (preset == VocoderExternal)
	{
		tTalkbox_init(&vocoder2, 1024);
	}
	else if (preset == Pitchshift)
	{
		tFormantShifter_init(&fs, 1024, 8);
		tRetune_init(&retune, NUM_RETUNE, 2048, 1024);
	}
	else if (preset == AutotuneMono)
	{
		tAutotune_init(&autotuneMono, 1, 2048, 1024);
	}
	else if (preset == AutotunePoly)
	{
		tAutotune_init(&autotunePoly, NUM_AUTOTUNE, 2048, 1024);
	}
	else if (preset == Sampler)
	{
		tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
		tBuffer_setRecordMode(&buff, RecordOneShot);
		tSampler_init(&sampler, &buff);
		tSampler_setMode(&sampler, PlayLoop);
	}
	else if (preset == RingMod)
	{
		tCycle_init(&mySine);
	}
	else if (preset == Distort)
	{
		tCrusher_init(&dist);
	}

	else if (preset == Octaver)
	{
		//tFormantShifter_init(&fs, 1024, 8);
		//tRetune_init(&pitchShiftB, NUM_RETUNE, 2048, 1024);
		tPitchShift_init(&pitchShiftB, NUM_RETUNE, 2048, 1024);
	}

//	else if (preset == Fear)
//	{
//		tDattorroReverb_init(&rev);
//	}
}
void calculateFreq(int voice)
{

	float tempNote = tPoly_getPitch(&poly, voice);
	float tempPitchClass = ((((int)tempNote) - keyCenter) % 12 );
	float tunedNote = tempNote + centsDeviation[(int)tempPitchClass];
	freq[voice] = LEAF_midiToFrequency(tunedNote);

}

void calculatePeriodArray()
{
	for (int i = 0; i < 128; i++)
	{
		float tempNote = i;
		float tempPitchClass = ((((int)tempNote) - keyCenter) % 12 );
		float tunedNote = tempNote + centsDeviation[(int)tempPitchClass];
		notePeriods[i] = 1.0f / LEAF_midiToFrequency(tunedNote) * leaf.sampleRate;
	}
}



float nearestPeriod(float period)
{
	float leastDifference = fabsf(period - notePeriods[0]);
	float difference;
	int index = 0;

	int* chord = chordArray;
	//if (autotuneLock > 0) chord = lockArray;

	for(int i = 1; i < 128; i++)
	{
		if (chord[i%12] > 0)
		{
			difference = fabsf(period - notePeriods[i]);
			if(difference < leastDifference)
			{
				leastDifference = difference;
				index = i;
			}
		}
	}

	return notePeriods[index];

}

void noteOn(int key, int velocity)
{
	if (!velocity)
	{
		if (chordArray[key%12] > 0) chordArray[key%12]--;

		int voice = tPoly_noteOff(&poly, key);
		if (voice >= 0) tRamp_setDest(&polyRamp[voice], 0.0f);

		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			if (tPoly_isOn(&poly, i) == 1)
			{
				tRamp_setDest(&polyRamp[i], 1.0f);
				calculateFreq(i);
			}
		}
		setLED_USB(0);
	}
	else
	{
		chordArray[key%12]++;



		tPoly_noteOn(&poly, key, velocity);

		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			if (tPoly_isOn(&poly, i) == 1)
			{
				tRamp_setDest(&polyRamp[i], 1.0f);
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
	if (voice >= 0) tRamp_setDest(&polyRamp[voice], 0.0f);

	for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
	{
		if (tPoly_isOn(&poly, i) == 1)
		{
			tRamp_setDest(&polyRamp[i], 1.0f);
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

}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{

}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{

}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(HALF_BUFFER_SIZE);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(0);
}

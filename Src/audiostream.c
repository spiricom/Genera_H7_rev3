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

//the audio buffers are put in the D2 RAM area because that is a memory location that the DMA has access to.
int32_t audioOutBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;
int32_t audioInBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;

#define MEM_SIZE 524288
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

tCycle mySine[2];
float targetADC[6];
float smoothedADC[6];
float hysteresisThreshold = 0.00f;

uint32_t clipCounter[4] = {0,0,0,0};
uint8_t clipped[4] = {0,0,0,0};
#define NUM_VOC_VOICES 8
#define NUM_VOC_OSC 1
#define INV_NUM_VOC_VOICES 0.125
#define INV_NUM_VOC_OSC 1
#define NUM_AUTOTUNE 8
#define NUM_RETUNE 1
#define OVERSAMPLER_RATIO 2
#define OVERSAMPLER_HQ FALSE
//audio objects
tFormantShifter fs;
tAutotune autotuneMono;
tAutotune autotunePoly;
tRetune retune;
tRamp nearWetRamp;
tRamp nearDryRamp;
tPoly poly;
tRamp polyRamp[NUM_VOC_VOICES];
tSawtooth osc[NUM_VOC_VOICES];
tTalkbox vocoder;
tTalkbox vocoder2;
tRamp comp;

tBuffer buff;
tSampler sampler;

tBuffer autograb_buffs[2];
tSampler autograb_samplers[2];

tOversampler oversampler;


tLockhartWavefolder wavefolder;

float nearestPeriod(float period);
void calculateFreq(int voice);


float rightIn = 0.0f;
float sample = 0.0f;

float notePeriods[128];
float noteFreqs[128];
int chordArray[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int lockArray[12];
float freq[NUM_VOC_VOICES];
float oversamplerArray[OVERSAMPLER_RATIO];

/*
 * typedef enum _VocodecPreset
{
	VocoderInternalPoly = 0,
	VocoderInternalMono,
	VocoderExternal,
	Pitchshift,
	AutotuneMono,
	AutotunePoly,
	SamplerButtonPress,
	SamplerAutoGrabInternal,
	SamplerAutoGrabExternal,
	DistortionTanH,
	DistortionShaper,
	Wavefolder,
	BitCrusher,
	Delay,
	Reverb,
	PresetNil
} VocodecPreset;
 */
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
int samplePlayStart = 0.0f;
int samplePlayEnd = 0.0f;
int sampleLength = 0.0f;
float samplerRate = 1.0f;
float maxSampleSizeSeconds = 2.5f;

// Sampler Auto Grab



int sinecount = 0;
uint8_t tickCompleted = 1;

uint8_t bufferCleared = 1;

int numBuffersToClearOnLoad = 2;
int numBuffersCleared = 0;

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

	calculatePeriodArray();

	tPoly_init(&poly, NUM_VOC_VOICES);
	tPoly_setPitchGlideTime(&poly, 50.0f);
	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		tRamp_init(&polyRamp[i], 10.0f, 1);
	}

	tRamp_init(&nearWetRamp, 10.0f, 1);
	tRamp_init(&nearDryRamp, 10.0f, 1);
	tRamp_init(&comp, 10.0f, 1);

	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		tSawtooth_init(&osc[i]);
	}
	tSawtooth_setFreq(&osc[0], 200);

	allocPreset(currentPreset);

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

	if (!loadingPreset)
	{
		if (currentPreset == VocoderInternalPoly)
		{
			tPoly_setNumVoices(&poly, NUM_VOC_VOICES);
			glideTimeVoc = 5.0f;

			tPoly_setPitchGlideTime(&poly, glideTimeVoc);
			for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
			{
				tRamp_setDest(&polyRamp[i], (tPoly_getVelocity(&poly, i) > 0));
				calculateFreq(i);
				tSawtooth_setFreq(&osc[i], freq[i]);
			}

			if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0f / tPoly_getNumActiveVoices(&poly));
		}

		else if (currentPreset == VocoderInternalMono)
		{
			tPoly_setNumVoices(&poly, 1);
			glideTimeVoc = 5.0f;

			tPoly_setPitchGlideTime(&poly, glideTimeVoc);

			tRamp_setDest(&polyRamp[0], (tPoly_getVelocity(&poly, 0) > 0));
			calculateFreq(0);
			tSawtooth_setFreq(&osc[0], freq[0]);

			if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0);
		}

		else if (currentPreset == VocoderExternal)
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
		else if (currentPreset == AutotuneMono)
		{
			tAutotune_setFreq(&autotuneMono, leaf.sampleRate / nearestPeriod(tAutotune_getInputPeriod(&autotuneMono)), 0);
			if (tPoly_getNumActiveVoices(&poly) != 0)
			{
				tRamp_setDest(&nearWetRamp, 1.0f);
				tRamp_setDest(&nearDryRamp, 1.0f);
			}
			else
			{
				tRamp_setDest(&nearWetRamp, 0.0f);
				tRamp_setDest(&nearDryRamp, 0.0f);
			}
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
		else if (currentPreset == SamplerButtonPress)
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

			tSampler_setStart(&sampler, samplePlayStart); //44832.4297
			tSampler_setEnd(&sampler, samplePlayEnd);// 119996.906
			tSampler_setRate(&sampler, samplerRate); //1.24937773
//			tSampler_setCrossfadeLength(&sampler, 500);
		}

		else if (currentPreset == SamplerAutoGrabInternal)
		{

		}

		else if (currentPreset == SamplerAutoGrabExternal)
		{

		}

		else if (currentPreset == DistortionTanH)
		{

		}

		else if (currentPreset == DistortionShaper)
		{

		}

		else if (currentPreset == BitCrusher)
		{

		}

		else if (currentPreset == Delay)
		{

		}

		else if (currentPreset == Reverb)
		{

		}
	}


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




float audioTickL(float audioIn)
{
	sample = 0.0f;
	//audioIn = tanhf(audioIn);

	for (int i = 0; i < 6; i++)
	{
		smoothedADC[i] = tRamp_tick(&adc[i]);
	}

	if (loadingPreset) return sample;

	bufferCleared = 0;

	if (currentPreset == VocoderInternalPoly)
	{
		tPoly_tickPitch(&poly);

		for (int i = 0; i < NUM_VOC_VOICES; i++)
		{
			sample = tSawtooth_tick(&osc[i]) * tRamp_tick(&polyRamp[i]);
		}
		sample *= tRamp_tick(&comp);
		sample = tTalkbox_tick(&vocoder, sample, audioIn);
		sample = tanhf(sample);
	}
	else if (currentPreset == VocoderInternalMono)
	{
		tPoly_tickPitch(&poly);

		sample = tSawtooth_tick(&osc[0]) * tRamp_tick(&polyRamp[0]);

		sample *= tRamp_tick(&comp);
		sample = tTalkbox_tick(&vocoder, sample, audioIn);
		sample = tanhf(sample);
	}
	else if (currentPreset == VocoderExternal)
	{
		tickCompleted = 0;
		sample = tTalkbox_tick(&vocoder2, rightIn, audioIn);
		sample = tanhf(sample);
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
		sample = samples[0] * tRamp_tick(&nearWetRamp);
		sample += audioIn * tRamp_tick(&nearDryRamp); // crossfade to dry signal if no notes held down.
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
	else if (currentPreset == SamplerButtonPress)
	{
		tBuffer_tick(&buff, audioIn);
		sample = tSampler_tick(&sampler);
	}

	else if (currentPreset == SamplerAutoGrabInternal)
	{
		tBuffer_tick(&buff, audioIn);
		sample = tSampler_tick(&sampler);
	}

	else if (currentPreset == SamplerAutoGrabExternal)
	{
		tBuffer_tick(&buff, audioIn);
		sample = tSampler_tick(&sampler);
	}

	else if (currentPreset == DistortionTanH)
	{
		//knob 0 = gain



		sample = audioIn;
		sample = sample * ((smoothedADC[0] * 30.0f) + 1.0f);

		tOversampler_upsample(&oversampler, sample, oversamplerArray);
		for (int i = 0; i < OVERSAMPLER_RATIO; i++)
		{
			oversamplerArray[i] = tanhf(oversamplerArray[i]);
		}
		sample = tOversampler_downsample(&oversampler, oversamplerArray);
		sample *= .65f;

		//sample = tOversampler_tick(&oversampler, sample, &tanhf);

	}

	else if (currentPreset == DistortionShaper)
	{
		//knob 0 = gain
		//knob 1 = shaper drive
		sample = audioIn;
		sample = sample * ((smoothedADC[0] * 30.0f) + 1.0f);
		tOversampler_upsample(&oversampler, sample, oversamplerArray);
		for (int i = 0; i < OVERSAMPLER_RATIO; i++)
		{
			oversamplerArray[i] = LEAF_shaper(oversamplerArray[i], 1.0f);
		}
		sample = tOversampler_downsample(&oversampler, oversamplerArray);
		sample *= .75f;
	}
	else if (currentPreset == Wavefolder)
	{
		//knob 0 = gain
		sample = audioIn;
		sample = sample * ((smoothedADC[0] * 8.0f) + 1.0f);

		sample = tLockhartWavefolder_tick(&wavefolder, sample);
		sample = tLockhartWavefolder_tick(&wavefolder, sample);
		sample = tLockhartWavefolder_tick(&wavefolder, sample);
		sample *= 0.75f;

	}
	else if (currentPreset == BitCrusher)
	{

	}
	else if (currentPreset == Delay)
	{

	}
	else if (currentPreset == Reverb)
	{

	}

	if ((audioIn > 0.999f) || (audioIn < -0.999f))
	{
		setLED_leftin_clip(1);
		clipCounter[0] = 22000;
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



	if ((sample > 0.999f) || (sample < -0.999f))
	{
		setLED_leftout_clip(1);
		clipCounter[2] = 22000;
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

	//return tanhf(sample);
}



float audioTickR(float audioIn)
{
	rightIn = audioIn;

	//float sample = 0.0f;
	//test code
    //tCycle_setFreq(&mySine[1], 400.0f);
	//sample = tCycle_tick(&mySine[1]);
	return sample;
}

void freePreset(VocodecPreset preset)
{
	if (preset == VocoderInternalPoly)
	{
		tTalkbox_free(&vocoder);
	}
	if (preset == VocoderInternalMono)
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
	else if (preset == SamplerButtonPress)
	{
		tBuffer_free(&buff);
		tSampler_free(&sampler);
	}
	else if (preset == SamplerAutoGrabInternal)
	{
		tBuffer_free(&buff);
		tSampler_free(&sampler);
	}
	else if (preset == SamplerAutoGrabExternal)
	{
		tBuffer_free(&buff);
		tSampler_free(&sampler);
	}

	else if (preset == DistortionTanH)
	{
		tOversampler_free(&oversampler);
	}

	else if (preset == DistortionShaper)
	{
		tOversampler_free(&oversampler);
	}

	else if (preset == Wavefolder)
	{
		tLockhartWavefolder_free(&wavefolder);
	}

	else if (preset == BitCrusher)
	{

	}

	else if (preset == Delay)
	{

	}

	else if (preset == Reverb)
	{

	}
}

void allocPreset(VocodecPreset preset)
{
	if (preset == VocoderInternalPoly)
	{
		tTalkbox_init(&vocoder, 1024);
	}
	else if (preset == VocoderInternalMono)
	{
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
		calculatePeriodArray();
	}
	else if (preset == AutotunePoly)
	{
		tAutotune_init(&autotunePoly, NUM_AUTOTUNE, 2048, 1024);
	}
	else if (preset == SamplerButtonPress)
	{
		tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
		tBuffer_setRecordMode(&buff, RecordOneShot);
		tSampler_init(&sampler, &buff);
		tSampler_setMode(&sampler, PlayLoop);
	}

	else if (preset == SamplerAutoGrabInternal)
	{
		tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
		tBuffer_setRecordMode(&buff, RecordOneShot);
		tSampler_init(&sampler, &buff);
		tSampler_setMode(&sampler, PlayLoop);
	}

	else if (preset == SamplerAutoGrabExternal)
	{
		tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
		tBuffer_setRecordMode(&buff, RecordOneShot);
		tSampler_init(&sampler, &buff);
		tSampler_setMode(&sampler, PlayLoop);
	}

	else if (preset == DistortionTanH)
	{
		tOversampler_init(&oversampler, OVERSAMPLER_RATIO, OVERSAMPLER_HQ);
	}
	else if (preset == DistortionShaper)
	{
		tOversampler_init(&oversampler, OVERSAMPLER_RATIO, OVERSAMPLER_HQ);
	}

	else if (preset == Wavefolder)
	{
		tLockhartWavefolder_init(&wavefolder);
	}

	else if (preset == BitCrusher)
	{

	}

	else if (preset == Delay)
	{

	}

	else if (preset == Reverb)
	{

	}

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

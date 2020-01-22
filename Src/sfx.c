/*
 * sfx.c
 *
 *  Created on: Dec 23, 2019
 *      Author: josnyder
 */

#include "main.h"
#include "audiostream.h"
#include "sfx.h"
#include "ui.h"
#include "tunings.h"


//audio objects
tFormantShifter fs;
tAutotune autotuneMono;
tAutotune autotunePoly;
tRetune retune;
tRetune retune2;
tRamp pitchshiftRamp;
tRamp nearWetRamp;
tRamp nearDryRamp;
tPoly poly;
tRamp polyRamp[NUM_VOC_VOICES];
tSawtooth osc[NUM_VOC_VOICES];
tTalkbox vocoder;
tTalkbox vocoder2;
tTalkbox vocoder3;
tRamp comp;

tBuffer buff;
tBuffer buff2;
tSampler sampler;

tEnvelopeFollower envfollow;

tOversampler oversampler;


tLockhartWavefolder wavefolder1;
tLockhartWavefolder wavefolder2;
tLockhartWavefolder wavefolder3;
tLockhartWavefolder wavefolder4;

tCrusher crush;
tCrusher crush2;

tTapeDelay delay;
tSVF delayLP;
tSVF delayHP;
tTapeDelay delay2;
tSVF delayLP2;
tSVF delayHP2;
tHighpass delayShaperHp;
tFeedbackLeveler feedbackControl;

tDattorroReverb reverb;
tNReverb reverb2;
tSVF lowpass;
tSVF highpass;
tSVF bandpass;
tSVF lowpass2;
tSVF highpass2;
tSVF bandpass2;

tCycle testSine;

tExpSmooth smoother1;
tExpSmooth smoother2;
tExpSmooth smoother3;

tExpSmooth neartune_smoother;

#define NUM_STRINGS 4
tLivingString theString[NUM_STRINGS];

//control objects
float notePeriods[128];
float noteFreqs[128];
int chordArray[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int chromaticArray[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int autotuneChromatic = 0;
int lockArray[12];
float freq[NUM_VOC_VOICES];
float oversamplerArray[OVERSAMPLER_RATIO];
int delayShaper = 0;

//sampler objects
int samplePlayStart = 0;
int samplePlayEnd = 0;
int sampleLength = 0;
int crossfadeLength = 0;
float samplerRate = 1.0f;
float maxSampleSizeSeconds = 1.0f;


//autosamp objects
volatile float currentPower = 0.0f;
volatile float previousPower = 0.0f;
float samp_thresh = 0.0f;
volatile int samp_triggered = 0;
uint32_t sample_countdown = 0;
PlayMode samplerMode = PlayLoop;
uint32_t powerCounter = 0;



//reverb objects
uint32_t freeze = 0;

void initGlobalSFXObjects()
{
	calculatePeriodArray();

	tPoly_init(&poly, NUM_VOC_VOICES);
	tPoly_setPitchGlideActive(&poly, FALSE);
	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		tRamp_init(&polyRamp[i], 10.0f, 1);
	}


	for (int i = 0; i < NUM_VOC_VOICES; i++)
		{
			tSawtooth_init(&osc[i]);
		}
		tSawtooth_setFreq(&osc[0], 200);

	tRamp_init(&nearWetRamp, 10.0f, 1);
	tRamp_init(&nearDryRamp, 10.0f, 1);
	tRamp_init(&comp, 10.0f, 1);
}

///1 vocoder internal poly

void SFXVocoderIPAlloc()
{
	tTalkbox_init(&vocoder, 1024);
	tPoly_setNumVoices(&poly, NUM_VOC_VOICES);
}

void SFXVocoderIPFrame()
{
	//glideTimeVoc = 5.0f;
	//tPoly_setPitchGlideTime(&poly, glideTimeVoc);
	for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
	{
		tRamp_setDest(&polyRamp[i], (tPoly_getVelocity(&poly, i) > 0));
		calculateFreq(i);
		tSawtooth_setFreq(&osc[i], freq[i]);
	}

	if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0f / tPoly_getNumActiveVoices(&poly));
}

void SFXVocoderIPTick(float audioIn)
{
	tPoly_tickPitch(&poly);
	uiParams[0] = smoothedADC[0]; //vocoder volume

	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		sample += tSawtooth_tick(&osc[i]) * tRamp_tick(&polyRamp[i]);
	}
	sample *= tRamp_tick(&comp);
	sample *= uiParams[0];
	sample = tTalkbox_tick(&vocoder, sample, audioIn);
	sample = tanhf(sample);
	rightOut = sample;
}

void SFXVocoderIPFree(void)
{
	tTalkbox_free(&vocoder);
}



///2 vocoder internal mono

void SFXVocoderIMAlloc()
{
	tTalkbox_init(&vocoder3, 1024);
	tPoly_setNumVoices(&poly, 1);
}

void SFXVocoderIMFrame()
{

	//glideTimeVoc = 5.0f;

	//tPoly_setPitchGlideTime(&poly, glideTimeVoc);

	tRamp_setDest(&polyRamp[0], (tPoly_getVelocity(&poly, 0) > 0));
	calculateFreq(0);
	tSawtooth_setFreq(&osc[0], freq[0]);
}

void SFXVocoderIMTick(float audioIn)
{
	tPoly_tickPitch(&poly);
	uiParams[0] = smoothedADC[0]; //vocoder volume
	sample = tSawtooth_tick(&osc[0]) * tRamp_tick(&polyRamp[0]);
	sample *= uiParams[0];
	sample = tTalkbox_tick(&vocoder3, sample, audioIn);
	sample = tanhf(sample);
	rightOut = sample;
}

void SFXVocoderIMFree(void)
{
	tTalkbox_free(&vocoder3);

}




//3 vocodec external

void SFXVocoderEAlloc()
{
	tTalkbox_init(&vocoder2, 1024);
}

void SFXVocoderEFrame()
{
	;
}

void SFXVocoderETick(float audioIn)
{
	//should add knob for ch2 input gain?

	sample = tTalkbox_tick(&vocoder2, rightIn, audioIn);
	sample = tanhf(sample);
	rightOut = rightIn;
}

void SFXVocoderEFree(void)
{
	tTalkbox_free(&vocoder2);
}






//4 pitch shift

void SFXPitchShiftAlloc()
{
	//tFormantShifter_init(&fs, 1024, 7);
	//tRetune_init(&retune, NUM_RETUNE, 2048, 1024);

	tFormantShifter_init(&fs, 256, 20);
	tRetune_init(&retune, NUM_RETUNE, 512, 256);
	tRetune_init(&retune2, NUM_RETUNE, 512, 256);
	tRamp_init(&pitchshiftRamp, 100.0f, 1);


	tExpSmooth_init(&smoother1, 0.0f, 0.01f);
	tExpSmooth_init(&smoother2, 0.0f, 0.01f);
	tExpSmooth_init(&smoother3, 0.0f, 0.01f);
}

void SFXPitchShiftFrame()
{


}

void SFXPitchShiftTick(float audioIn)
{
	//pitchFactor = (smoothedADC[0]*3.75f)+0.25f;




	float myPitchFactorCoarse = (smoothedADC[0]*2.0f) - 1.0f;
	float myPitchFactorFine = ((smoothedADC[1]*2.0f) - 1.0f) * 0.1f;
	float myPitchFactorCombined = myPitchFactorFine + myPitchFactorCoarse;
	uiParams[0] = myPitchFactorCombined;
	uiParams[1] = myPitchFactorCombined;
	float myPitchFactor = fastexp2f(myPitchFactorCombined);
	tRetune_setPitchFactor(&retune, myPitchFactor, 0);
	tRetune_setPitchFactor(&retune2, myPitchFactor, 0);


	uiParams[2] = LEAF_clip( 0.0f,((smoothedADC[2]) * 1.1f) - 0.2f, 2.0f);

	uiParams[3] = fastexp2f((smoothedADC[3]*2.0f) - 1.0f);

	tExpSmooth_setDest(&smoother3, uiParams[2]);


	tFormantShifter_setIntensity(&fs, tExpSmooth_tick(&smoother3)+.1f);
	tFormantShifter_setShiftFactor(&fs, uiParams[3]);
	if (uiParams[2] > 0.01f)
	{
		tRamp_setDest(&pitchshiftRamp, -1.0f);
	}
	else
	{
		tRamp_setDest(&pitchshiftRamp, 1.0f);
	}

	float crossfadeVal = tRamp_tick(&pitchshiftRamp);
	float myGains[2];
	LEAF_crossfade(crossfadeVal, myGains);
	tExpSmooth_setDest(&smoother1, myGains[0]);
	tExpSmooth_setDest(&smoother2, myGains[1]);

	float formantsample = (tanhf(tFormantShifter_remove(&fs, audioIn)));


	float* samples = tRetune_tick(&retune2, formantsample);
	formantsample = samples[0];
	sample = audioIn;
	samples = tRetune_tick(&retune, sample);
	sample = samples[0];

	formantsample = (tanhf(tFormantShifter_add(&fs, formantsample) * 0.9f)) * tExpSmooth_tick(&smoother2) ;
	sample = (sample * (tExpSmooth_tick(&smoother1))) +  formantsample;
	rightOut = sample;

}

void SFXPitchShiftFree(void)
{
	tFormantShifter_free(&fs);
	tRetune_free(&retune);
	tRetune_free(&retune2);

	tRamp_free(&pitchshiftRamp);

	tExpSmooth_free(&smoother1);
	tExpSmooth_free(&smoother2);
	tExpSmooth_free(&smoother3);
}




//5 neartune
void SFXNeartuneAlloc()
{
	tAutotune_init(&autotuneMono, 1, 512, 256);
	calculatePeriodArray();
	tExpSmooth_init(&neartune_smoother, 100.0f, .007f);
}

void SFXNeartuneFrame()
{

	if ((tPoly_getNumActiveVoices(&poly) != 0) || (autotuneChromatic == 1))
	{
		tRamp_setDest(&nearWetRamp, 1.0f);
		tRamp_setDest(&nearDryRamp, 0.0f);
	}
	else
	{
		tRamp_setDest(&nearWetRamp, 0.0f);
		tRamp_setDest(&nearDryRamp, 1.0f);
	}

	if (buttonPressed[5])
	{
		autotuneChromatic = 1;
		buttonPressed[5] = 0;
		setLED_A(1);
		setLED_B(0);
	}

	if (buttonPressed[6])
	{
		autotuneChromatic = 0;
		buttonPressed[6] = 0;
		setLED_A(0);
		setLED_B(1);
	}
}

void SFXNeartuneTick(float audioIn)
{
	//JS: suggested improvements -
	// right now the period is used for "nearest pitch matching" but that is skewed - doing it on a midi note represetation would be more perceptually reasonable
	// but it's a little expensive to use midi to freq and then put it to freq for the shifter.

	float* samples = tAutotune_tick(&autotuneMono, audioIn);
	float detectedPeriod = tAutotune_getInputPeriod(&autotuneMono);
	float desiredSnap = nearestPeriod(detectedPeriod);

	uiParams[0] = smoothedADC[0]; // amount of forcing to new pitch
	uiParams[1] = smoothedADC[1]; //speed to get to desired pitch shift
	tExpSmooth_setFactor(&neartune_smoother, (uiParams[1] * .01f));
	float destinationPeriod = (desiredSnap * uiParams[0]) + (detectedPeriod * (1.0f - uiParams[0]));
	float destinationFreq = (leaf.sampleRate / destinationPeriod);
	tExpSmooth_setDest(&neartune_smoother, destinationFreq);
	tAutotune_setFreq(&autotuneMono, tExpSmooth_tick(&neartune_smoother), 0);
	//tAutotune_setFreq(&autotuneMono, leaf.sampleRate / nearestPeriod(tAutotune_getInputPeriod(&autotuneMono)), 0);
	sample = samples[0] * tRamp_tick(&nearWetRamp);
	sample += audioIn * tRamp_tick(&nearDryRamp); // crossfade to dry signal if no notes held down.
	rightOut = sample;
}



void SFXNeartuneFree(void)
{
	tAutotune_free(&autotuneMono);
	tExpSmooth_free(&neartune_smoother);
}





//6 autotune
void SFXAutotuneAlloc()
{
	tAutotune_init(&autotunePoly, NUM_AUTOTUNE, 1024, 512);
	tPoly_setNumVoices(&poly, NUM_AUTOTUNE);

	//tAutotune_init(&autotunePoly, NUM_AUTOTUNE, 2048, 1024); //old settings
}

void SFXAutotuneFrame()
{
	for (int i = 0; i < tPoly_getNumVoices(&poly); ++i)
	{
		calculateFreq(i);
	}
	if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0f / tPoly_getNumActiveVoices(&poly));
}

void SFXAutotuneTick(float audioIn)
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
	rightOut = sample;
}

void SFXAutotuneFree(void)
{
	tAutotune_free(&autotunePoly);
}



//7 sampler - button press
void SFXSamplerBPAlloc()
{
	tBuffer_init_locate(&buff, leaf.sampleRate * 172.0f, &large_pool);
	tBuffer_setRecordMode(&buff, RecordOneShot);
	tSampler_init(&sampler, &buff);
	tSampler_setMode(&sampler, PlayLoop);
}

void SFXSamplerBPFrame()
{
}

void SFXSamplerBPTick(float audioIn)
{
	uiParams[0] = smoothedADC[0] * sampleLength;
	uiParams[1] = smoothedADC[1] * sampleLength;
	uiParams[2] = (smoothedADC[2] - 0.5f) * 4.0f;

	uiParams[3] = smoothedADC[3] * 4000.0f;

	samplePlayStart = uiParams[0];
	samplePlayEnd = uiParams[1];
	samplerRate = uiParams[2];
	crossfadeLength = uiParams[3];
	tSampler_setStart(&sampler, samplePlayStart);
	tSampler_setEnd(&sampler, samplePlayEnd);
	tSampler_setRate(&sampler, samplerRate);
	tSampler_setCrossfadeLength(&sampler, crossfadeLength);


	if (buttonPressed[5])
	{
		if (sampler->active != 0)
		{
			tSampler_stop(&sampler);
		}
		tBuffer_record(&buff);
		buttonPressed[5] = 0;
	}
	else if (buttonReleased[5])
	{
		tBuffer_stop(&buff);
		sampleLength = tBuffer_getRecordPosition(&buff);
		tSampler_play(&sampler);
		tSampler_setStart(&sampler, samplePlayStart);
		tSampler_setEnd(&sampler, samplePlayEnd);
		tSampler_setRate(&sampler, samplerRate);
		tSampler_setCrossfadeLength(&sampler, crossfadeLength);
		buttonReleased[5] = 0;
	}
	if (buttonPressed[4])
	{
		tBuffer_clear(&buff);
		buttonPressed[4] = 0;
	}



	tBuffer_tick(&buff, audioIn);
	sample = tanhf(tSampler_tick(&sampler));
	rightOut = sample;
}

void SFXSamplerBPFree(void)
{
	tBuffer_free_locate(&buff, &large_pool);
	tSampler_free(&sampler);
}






//8 sampler - auto ch1
void SFXSamplerAuto1Alloc()
{
	tBuffer_init_locate(&buff2, leaf.sampleRate * 2.0f, &large_pool);
	tBuffer_setRecordMode(&buff2, RecordOneShot);
	tSampler_init(&sampler, &buff2);
	tSampler_setMode(&sampler, PlayLoop);
	tEnvelopeFollower_init(&envfollow, 0.05f, 0.9999f);
	tSampler_play(&sampler);
}

void SFXSamplerAuto1Frame()
{
}




void SFXSamplerAuto1Tick(float audioIn)
{
	currentPower = tEnvelopeFollower_tick(&envfollow, audioIn);
	samp_thresh = 1.0f - smoothedADC[0];
	uiParams[0] = samp_thresh;
	int window_size = smoothedADC[1] * 10000.0f;
	uiParams[3] = smoothedADC[3] * 1000.0f;
	crossfadeLength = uiParams[3];

	tSampler_setCrossfadeLength(&sampler, crossfadeLength);

	if ((currentPower > (samp_thresh)) && (currentPower > previousPower + 0.001f) && (samp_triggered == 0) && (sample_countdown == 0))
	{
		samp_triggered = 1;
		setLED_A(1);
		tBuffer_record(&buff2);
		sample_countdown = window_size + 24;//arbitrary extra time to avoid resampling while playing previous sample - better solution would be alternating buffers and crossfading
		powerCounter = 1000;
	}

	if (sample_countdown > 0)
	{
		sample_countdown--;
	}

	tSampler_setEnd(&sampler,window_size);
	tBuffer_tick(&buff2, audioIn);
	//on it's way down
	if (currentPower <= previousPower)
	{
		if (powerCounter > 0)
		{
			powerCounter--;
		}
		else if (samp_triggered == 1)
		{
			setLED_A(0);
			samp_triggered = 0;
		}
	}
	if (buttonPressed[5])
	{
		if (samplerMode == PlayLoop)
		{
			tSampler_setMode(&sampler, PlayBackAndForth);
			samplerMode = PlayBackAndForth;
			setLED_1(1);
			buttonPressed[5] = 0;
		}
		else if (samplerMode == PlayBackAndForth)
		{
			tSampler_setMode(&sampler, PlayLoop);
			samplerMode = PlayLoop;
			setLED_1(0);
			buttonPressed[5] = 0;
		}

	}
	sample = tSampler_tick(&sampler);
	rightOut = sample;
	previousPower = currentPower;
}

void SFXSamplerAuto1Free(void)
{
	tBuffer_free_locate(&buff2, &large_pool);
	tSampler_free(&sampler);
	tEnvelopeFollower_free(&envfollow);
}

//9 sampler - auto ch2
void SFXSamplerAuto2Alloc()
{
	tBuffer_init_locate(&buff2, leaf.sampleRate * 2.0f, &large_pool);
	tBuffer_setRecordMode(&buff2, RecordOneShot);
	tSampler_init(&sampler, &buff2);
	tSampler_setMode(&sampler, PlayLoop);
	tEnvelopeFollower_init(&envfollow, 0.05f, 0.9999f);
	tSampler_play(&sampler);
}

void SFXSamplerAuto2Frame()
{
}

void SFXSamplerAuto2Tick(float audioIn)
{
	currentPower = tEnvelopeFollower_tick(&envfollow, rightIn);
	samp_thresh = 1.0f - smoothedADC[0];
	uiParams[0] = samp_thresh;
	int window_size = smoothedADC[1] * 10000.0f;
	uiParams[3] = smoothedADC[3] * 1000.0f;
	crossfadeLength = uiParams[3];

	tSampler_setCrossfadeLength(&sampler, crossfadeLength);

	if ((currentPower > (samp_thresh)) && (currentPower > previousPower + 0.001f) && (samp_triggered == 0) && (sample_countdown == 0))
	{
		samp_triggered = 1;
		setLED_A(1);
		tBuffer_record(&buff2);
		sample_countdown = window_size + 24;//arbitrary extra time to avoid resampling while playing previous sample - better solution would be alternating buffers and crossfading
		powerCounter = 1000;
	}

	if (sample_countdown > 0)
	{
		sample_countdown--;
	}

	tSampler_setEnd(&sampler,window_size);
	tBuffer_tick(&buff2, audioIn);
	//on it's way down
	if (currentPower <= previousPower)
	{
		if (powerCounter > 0)
		{
			powerCounter--;
		}
		else if (samp_triggered == 1)
		{
			setLED_A(0);
			samp_triggered = 0;
		}
	}
	if (buttonPressed[5])
	{
		if (samplerMode == PlayLoop)
		{
			tSampler_setMode(&sampler, PlayBackAndForth);
			samplerMode = PlayBackAndForth;
			setLED_1(1);
			buttonPressed[5] = 0;
		}
		else if (samplerMode == PlayBackAndForth)
		{
			tSampler_setMode(&sampler, PlayLoop);
			samplerMode = PlayLoop;
			setLED_1(0);
			buttonPressed[5] = 0;
		}

	}
	sample = tSampler_tick(&sampler);
	rightOut = sample;
	previousPower = currentPower;
}

void SFXSamplerAuto2Free(void)
{
	tBuffer_free_locate(&buff2, &large_pool);
	tSampler_free(&sampler);
	tEnvelopeFollower_free(&envfollow);
}


//10 distortion tanh
void SFXDistortionTanhAlloc()
{
	tOversampler_init(&oversampler, OVERSAMPLER_RATIO, OVERSAMPLER_HQ);
}

void SFXDistortionTanhFrame()
{
}

void SFXDistortionTanhTick(float audioIn)
{
	//knob 0 = gain
		sample = audioIn;
		uiParams[0] = ((smoothedADC[0] * 40.0f) + 1.0f);
		sample = sample * uiParams[0];

		tOversampler_upsample(&oversampler, sample, oversamplerArray);
		for (int i = 0; i < OVERSAMPLER_RATIO; i++)
		{
			oversamplerArray[i] = tanhf(oversamplerArray[i]);
		}
		sample = tOversampler_downsample(&oversampler, oversamplerArray);
		sample *= .65f;
		rightOut = sample;

		//sample = tOversampler_tick(&oversampler, sample, &tanhf);
}

void SFXDistortionTanhFree(void)
{
	tOversampler_free(&oversampler);
}



//11 distortion shaper function
void SFXDistortionShaperAlloc()
{
	tOversampler_init(&oversampler, OVERSAMPLER_RATIO, OVERSAMPLER_HQ);
}

void SFXDistortionShaperFrame()
{
}

void SFXDistortionShaperTick(float audioIn)
{
	//knob 0 = gain
		//knob 1 = shaper drive
		sample = audioIn;
		uiParams[0] = ((smoothedADC[0] * 15.0f) + 1.0f);
		sample = sample * uiParams[0];
		tOversampler_upsample(&oversampler, sample, oversamplerArray);
		for (int i = 0; i < OVERSAMPLER_RATIO; i++)
		{
			oversamplerArray[i] = LEAF_shaper(oversamplerArray[i], 1.0f);
		}
		sample = tOversampler_downsample(&oversampler, oversamplerArray);
		sample *= .75f;
		rightOut = sample;
}

void SFXDistortionShaperFree(void)
{
	tOversampler_free(&oversampler);
}


//12 distortion wave folder
void SFXWaveFolderAlloc()
{
	tLockhartWavefolder_init(&wavefolder1);
	tLockhartWavefolder_init(&wavefolder2);
	tLockhartWavefolder_init(&wavefolder3);
	tLockhartWavefolder_init(&wavefolder4);
	tOversampler_init(&oversampler, 2, FALSE);
}

void SFXWaveFolderFrame()
{
}

void SFXWaveFolderTick(float audioIn)
{
	//knob 0 = gain
	sample = audioIn;
	uiParams[0] = (smoothedADC[0] * 20.0f) + 1.0f;

	uiParams[1] = (smoothedADC[1] * 2.0f) - 1.0f;

	uiParams[2] = (smoothedADC[2] * 2.0f) - 1.0f;

	uiParams[3] = (smoothedADC[3] * 2.0f) - 1.0f;

	float gain = uiParams[0];


	sample = sample * gain * 0.33f;
	sample = sample + uiParams[1];

	tOversampler_upsample(&oversampler, sample, oversamplerArray);
	for (int i = 0; i < 2; i++)
	{
		oversamplerArray[i] = tLockhartWavefolder_tick(&wavefolder1, oversamplerArray[i]);
		//sample = sample * gain;
		oversamplerArray[i] = sample + uiParams[2];
		oversamplerArray[i] = tLockhartWavefolder_tick(&wavefolder2, oversamplerArray[i]);
		//oversamplerArray[i] = sample + uiParams[3];
		//sample *= .6f;
		//oversamplerArray[i] = tLockhartWavefolder_tick(&wavefolder3, oversamplerArray[i]);
		//sample = tLockhartWavefolder_tick(&wavefolder4, sample);
		oversamplerArray[i] *= .8f;
		oversamplerArray[i] = tanhf(oversamplerArray[i]);
	}
	sample = tOversampler_downsample(&oversampler, oversamplerArray);
	rightOut = sample;

	/*
	sample = tLockhartWavefolder_tick(&wavefolder1, sample);
	//sample = sample * gain;
	sample = sample + uiParams[2];
	sample = tLockhartWavefolder_tick(&wavefolder2, sample);
	sample = sample + uiParams[3];
	//sample *= .6f;
	sample = tLockhartWavefolder_tick(&wavefolder3, sample);
	//sample = tLockhartWavefolder_tick(&wavefolder4, sample);
	sample *= .8f;
	sample = tanhf(sample);
	rightOut = sample;

	*/
}

void SFXWaveFolderFree(void)
{
	tLockhartWavefolder_free(&wavefolder1);
	tLockhartWavefolder_free(&wavefolder2);
	tLockhartWavefolder_free(&wavefolder3);
	tLockhartWavefolder_free(&wavefolder4);
	tOversampler_free(&oversampler);
}


//13 bitcrusher
void SFXBitcrusherAlloc()
{
	tCrusher_init(&crush);
	tCrusher_init(&crush2);
}

void SFXBitcrusherFrame()
{
}

void SFXBitcrusherTick(float audioIn)
{
	uiParams[0] = smoothedADC[0];
	tCrusher_setQuality (&crush, smoothedADC[0]);
	tCrusher_setQuality (&crush2, smoothedADC[0]);
	uiParams[1] = smoothedADC[1];
	tCrusher_setSamplingRatio (&crush, smoothedADC[1]);
	tCrusher_setSamplingRatio (&crush2, smoothedADC[1]);
	uiParams[2] = smoothedADC[2];
	tCrusher_setRound (&crush, smoothedADC[2]);
	tCrusher_setRound (&crush2, smoothedADC[2]);
	uiParams[3] = smoothedADC[3];
	tCrusher_setOperation (&crush, smoothedADC[3]);
	tCrusher_setOperation (&crush2, smoothedADC[3]);
	uiParams[4] = smoothedADC[4] + 1.0f;
	sample = tCrusher_tick(&crush, tanhf(audioIn * (smoothedADC[4] + 1.0f))) * .9f;
	rightOut = tCrusher_tick(&crush2, tanhf(rightIn * (smoothedADC[4] + 1.0f))) * .9f;

}

void SFXBitcrusherFree(void)
{
	tCrusher_free(&crush);
	tCrusher_free(&crush2);
}


//14 delay
void SFXDelayAlloc()
{
	tTapeDelay_init(&delay, 2000, 30000);
	tTapeDelay_init(&delay2, 2000, 30000);
	tSVF_init(&delayLP, SVFTypeLowpass, 16000.f, .7f);
	tSVF_init(&delayHP, SVFTypeHighpass, 20.f, .7f);

	tSVF_init(&delayLP2, SVFTypeLowpass, 16000.f, .7f);
	tSVF_init(&delayHP2, SVFTypeHighpass, 20.f, .7f);

	tHighpass_init(&delayShaperHp, 20.0f);
	tFeedbackLeveler_init(&feedbackControl, .99f, 0.01, 0.125f, 0);

}

void SFXDelayFrame()
{

	if (buttonPressed[5])
	{
		delayShaper = 1;
		buttonPressed[5] = 0;
		setLED_A(1);
		setLED_B(0);
	}

	if (buttonPressed[6])
	{
		delayShaper = 0;
		buttonPressed[6] = 0;
		setLED_A(0);
		setLED_B(1);
	}
}

void SFXDelayTick(float audioIn)
{
	uiParams[0] = smoothedADC[0] * 30000.0f;
	uiParams[1] = smoothedADC[1] * 30000.0f;
	uiParams[2] = smoothedADC[2] * 1.1f;
	uiParams[3] = faster_mtof((smoothedADC[3] * 128) + 10.0f);
	uiParams[4] = faster_mtof((smoothedADC[4] * 128) + 10.0f);

	tSVF_setFreq(&delayLP, uiParams[3]);
	tSVF_setFreq(&delayLP2, uiParams[3]);
	tSVF_setFreq(&delayHP, uiParams[4]);
	tSVF_setFreq(&delayHP2, uiParams[4]);

	//swap tanh for shaper and add cheap fixed highpass after both shapers

	float prevSamp1 = tTapeDelay_getLastOut(&delay);
	float prevSamp2 = tTapeDelay_getLastOut(&delay2);

	prevSamp1 = tSVF_tick(&delayLP, prevSamp1);
	prevSamp2 = tSVF_tick(&delayLP2, prevSamp2);

	prevSamp1 = tSVF_tick(&delayHP, prevSamp1);
	prevSamp2 = tSVF_tick(&delayHP2, prevSamp2);
	float input1, input2;

	if (delayShaper == 1)
	{
		input1 = tFeedbackLeveler_tick(&feedbackControl, tanhf(audioIn + (prevSamp1 * uiParams[2])));
		input2 = tFeedbackLeveler_tick(&feedbackControl, tanhf(audioIn + (prevSamp2 * uiParams[2])));
	}
	else
	{
		input1 = tFeedbackLeveler_tick(&feedbackControl, tHighpass_tick(&delayShaperHp, LEAF_shaper(audioIn + (prevSamp1 * uiParams[2] * 0.5f), 0.5f)));
		input2 = tFeedbackLeveler_tick(&feedbackControl, tHighpass_tick(&delayShaperHp, LEAF_shaper(audioIn + (prevSamp2 * uiParams[2] * 0.5f), 0.5f)));
	}
	tTapeDelay_setDelay(&delay, uiParams[0]);

	sample = tTapeDelay_tick(&delay, input1);

	tTapeDelay_setDelay(&delay2, uiParams[1]);

	rightOut = tTapeDelay_tick(&delay2, input2);

}

void SFXDelayFree(void)
{
	tTapeDelay_free(&delay);
	tTapeDelay_free(&delay2);
	tSVF_free(&delayLP);
	tSVF_free(&delayHP);
	tSVF_free(&delayLP2);
	tSVF_free(&delayHP2);

	tHighpass_free(&delayShaperHp);
	tFeedbackLeveler_free(&feedbackControl);
}



//15 reverb
void SFXReverbAlloc()
{
	tDattorroReverb_init(&reverb);
	tDattorroReverb_setMix(&reverb, 1.0f);
}

void SFXReverbFrame()
{
	uiParams[1] = faster_mtof(smoothedADC[1]*135.0f);
	tDattorroReverb_setInputFilter(&reverb, uiParams[1]);
	uiParams[2] =  faster_mtof(smoothedADC[2]*128.0f);
	tDattorroReverb_setHP(&reverb, uiParams[2]);
	uiParams[3] = faster_mtof(smoothedADC[3]*135.0f);
	tDattorroReverb_setFeedbackFilter(&reverb, uiParams[3]);

}

void SFXReverbTick(float audioIn)
{
	float stereo[2];

	if (buttonPressed[5])
	{
		if (freeze == 0)
		{
			freeze = 1;
			tDattorroReverb_setFreeze(&reverb, 1);
			setLED_1(1);
			buttonPressed[5] = 0;
		}
		else
		{
			freeze = 0;
			tDattorroReverb_setFreeze(&reverb, 0);
			setLED_1(0);
			buttonPressed[5] = 0;
		}

	}

	//tDattorroReverb_setInputDelay(&reverb, smoothedADC[1] * 200.f);
	audioIn *= 4.0f;
	uiParams[0] = smoothedADC[0];
	tDattorroReverb_setSize(&reverb, uiParams[0]);
	uiParams[4] = smoothedADC[4];
	tDattorroReverb_setFeedbackGain(&reverb, uiParams[4]);
	tDattorroReverb_tickStereo(&reverb, audioIn, stereo);
	sample = tanhf(stereo[0]) * 0.99f;
	rightOut = tanhf(stereo[1]) * 0.99f;
}

void SFXReverbFree(void)
{
	tDattorroReverb_free(&reverb);
}


//16 reverb2
void SFXReverb2Alloc()
{
	tNReverb_init(&reverb2, 1.0f);
	tNReverb_setMix(&reverb2, 1.0f);
	tSVF_init(&lowpass, SVFTypeLowpass, 18000.0f, 0.75f);
	tSVF_init(&highpass, SVFTypeHighpass, 40.0f, 0.75f);
	tSVF_init(&bandpass, SVFTypeBandpass, 2000.0f, 1.0f);
	tSVF_init(&lowpass2, SVFTypeLowpass, 18000.0f, 0.75f);
	tSVF_init(&highpass2, SVFTypeHighpass, 40.0f, 0.75f);
	tSVF_init(&bandpass2, SVFTypeBandpass, 2000.0f, 1.0f);
}

void SFXReverb2Frame()
{


}


void SFXReverb2Tick(float audioIn)
{
	float stereoOuts[2];
	uiParams[0] = smoothedADC[0] * 4.0f;
	if (!freeze)
	{
		tNReverb_setT60(&reverb2, uiParams[0]);

	}
	else
	{
		tNReverb_setT60(&reverb2, 1000.0f);
		audioIn = 0.0f;
	}

	uiParams[1] =  faster_mtof(smoothedADC[1]*135.0f);
	tSVF_setFreq(&lowpass, uiParams[1]);
	tSVF_setFreq(&lowpass2, uiParams[1]);
	uiParams[2] = faster_mtof(smoothedADC[2]*128.0f);
	tSVF_setFreq(&highpass, uiParams[2]);
	tSVF_setFreq(&highpass2, uiParams[2]);
	uiParams[3] = faster_mtof(smoothedADC[3]*128.0f);
	tSVF_setFreq(&bandpass, uiParams[3]);
	tSVF_setFreq(&bandpass2, uiParams[3]);

	uiParams[4] = (smoothedADC[4] * 4.0f) - 2.0f;

	if (buttonPressed[5])
	{
		if (freeze == 0)
		{
			freeze = 1;
			setLED_1(1);
			buttonPressed[5] = 0;
		}
		else
		{
			freeze = 0;
			setLED_1(0);
			buttonPressed[5] = 0;
		}
	}


	tNReverb_tickStereo(&reverb2, audioIn, stereoOuts);
	float leftOut = tSVF_tick(&lowpass, stereoOuts[0]);
	leftOut = tSVF_tick(&highpass, leftOut);
	leftOut += tSVF_tick(&bandpass, leftOut) * uiParams[4];

	float rightOutTemp = tSVF_tick(&lowpass2, stereoOuts[1]);
	rightOutTemp = tSVF_tick(&highpass2, rightOutTemp);
	rightOutTemp += tSVF_tick(&bandpass, rightOutTemp) * uiParams[4];
	sample = tanhf(leftOut);
	rightOut = tanhf(rightOutTemp);

}

void SFXReverb2Free(void)
{
	tNReverb_free(&reverb2);
	tSVF_free(&lowpass);
	tSVF_free(&highpass);
	tSVF_free(&bandpass);
	tSVF_free(&lowpass2);
	tSVF_free(&highpass2);
	tSVF_free(&bandpass2);
}

//17 Living String
float myFreq;
float myDetune[NUM_STRINGS];

void SFXLivingStringAlloc()
{
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		myFreq = (randomNumber() * 300.0f) + 60.0f;
		myDetune[i] = (randomNumber() * 0.3f) - 0.15f;
		tLivingString_init(&theString[i],  myFreq, 0.4f, 0.0f, 16000.0f, .99f, .25f, .01f, 0.1f, 0);
	}
}

void SFXLivingStringFrame()
{


}


void SFXLivingStringTick(float audioIn)
{
	uiParams[0] = mtof(smoothedADC[0] * 128.0f);
	uiParams[1] = smoothedADC[1];
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		tLivingString_setFreq(&theString[i], (i + (1.0f+(myDetune[i] * uiParams[1]))) * uiParams[0]);
		sample += tLivingString_tick(&theString[i], audioIn);
	}
	//tLivingString_setFreq(&theString, uiParams[0]);
	sample *= 1.0f/NUM_STRINGS;
	rightOut = sample * 0.5f;

}

void SFXLivingStringFree(void)
{
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		tLivingString_free(&theString[i]);
	}
}


// midi functions


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
	int* chord;

	if (autotuneChromatic > 0)
	{
		chord = chromaticArray;
	}
	else
	{
		chord = chordArray;
	}
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
		setLED_2(1);
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
	setLED_2(0);
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


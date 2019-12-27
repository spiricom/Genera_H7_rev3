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
tSampler sampler;

tBuffer autograb_buffs[2];
tSampler autograb_samplers[2];

tOversampler oversampler;


tLockhartWavefolder wavefolder1;
tLockhartWavefolder wavefolder2;
tLockhartWavefolder wavefolder3;
tLockhartWavefolder wavefolder4;

tCrusher crush;

tDelay delay;
tSVF delayLP;
tSVF delayHP;


tDattorroReverb reverb;


tCycle testSine;



//control objects
float notePeriods[128];
float noteFreqs[128];
int chordArray[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int lockArray[12];
float freq[NUM_VOC_VOICES];
float oversamplerArray[OVERSAMPLER_RATIO];


//sampler objects
int samplePlayStart = 0.0f;
int samplePlayEnd = 0.0f;
int sampleLength = 0.0f;
float samplerRate = 1.0f;
float maxSampleSizeSeconds = 1.0f;


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

	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		sample += tSawtooth_tick(&osc[i]) * tRamp_tick(&polyRamp[i]);
	}
	sample *= tRamp_tick(&comp);
	sample = tTalkbox_tick(&vocoder, sample, audioIn);
	sample = tanhf(sample);
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

	if (tPoly_getNumActiveVoices(&poly) != 0) tRamp_setDest(&comp, 1.0);
}

void SFXVocoderIMTick(float audioIn)
{
	tPoly_tickPitch(&poly);

	sample = tSawtooth_tick(&osc[0]) * tRamp_tick(&polyRamp[0]);

	sample *= tRamp_tick(&comp);
	sample = tTalkbox_tick(&vocoder3, sample, audioIn);
	sample = tanhf(sample);
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

	tFormantShifter_init(&fs, 2048, 7);
	tRetune_init(&retune, NUM_RETUNE, 512, 256);
}

void SFXPitchShiftFrame()
{
	//pitchFactor = (smoothedADC[0]*3.75f)+0.25f;
	uiParams[0] = fastexp2f((smoothedADC[0]*2.0f) - 1.0f);

	tRetune_setPitchFactor(&retune, uiParams[0], 0);
	//formantWarp = (smoothedADC[1]*3.75f)+0.25f;
	uiParams[1] = fastexp2f((smoothedADC[1]*2.0f) - 1.0f);

	uiParams[2] = (smoothedADC[2]) * 10.0f;
	tFormantShifter_setShiftFactor(&fs, uiParams[1]);
	tFormantShifter_setIntensity(&fs, uiParams[2]);
}

void SFXPitchShiftTick(float audioIn)
{
	sample = tanhf(tFormantShifter_remove(&fs, audioIn));

	float* samples = tRetune_tick(&retune, sample);
	sample = samples[0];

	sample = tanhf(tFormantShifter_add(&fs, sample) * 0.9f);
}

void SFXPitchShiftFree(void)
{
	tFormantShifter_free(&fs);
	tRetune_free(&retune);
}




//5 neartune
void SFXNeartuneAlloc()
{
	tAutotune_init(&autotuneMono, 1, 2048, 1024);
	calculatePeriodArray();
}

void SFXNeartuneFrame()
{
	tAutotune_setFreq(&autotuneMono, leaf.sampleRate / nearestPeriod(tAutotune_getInputPeriod(&autotuneMono)), 0);
	if (tPoly_getNumActiveVoices(&poly) != 0)
	{
		tRamp_setDest(&nearWetRamp, 1.0f);
		tRamp_setDest(&nearDryRamp, 0.0f);
	}
	else
	{
		tRamp_setDest(&nearWetRamp, 0.0f);
		tRamp_setDest(&nearDryRamp, 1.0f);
	}
}

void SFXNeartuneTick(float audioIn)
{
	float* samples = tAutotune_tick(&autotuneMono, audioIn);
	sample = samples[0] * tRamp_tick(&nearWetRamp);
	sample += audioIn * tRamp_tick(&nearDryRamp); // crossfade to dry signal if no notes held down.
}



void SFXNeartuneFree(void)
{
	tAutotune_free(&autotuneMono);
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
}

void SFXAutotuneFree(void)
{
	tAutotune_free(&autotunePoly);
}



//7 sampler - button press
void SFXSamplerBPAlloc()
{
	tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
	tBuffer_setRecordMode(&buff, RecordOneShot);
	tSampler_init(&sampler, &buff);
	tSampler_setMode(&sampler, PlayLoop);
}

void SFXSamplerBPFrame()
{
}

void SFXSamplerBPTick(float audioIn)
{
	if (buttonPressed[5])
	{
		tSampler_stop(&sampler);
		tBuffer_record(&buff);
	}
	else if (buttonReleased[5])
	{
		tBuffer_stop(&buff);
		sampleLength = tBuffer_getRecordPosition(&buff);
		tSampler_play(&sampler);
	}
	if (buttonPressed[4])
	{
		tBuffer_clear(&buff);
	}
	uiParams[0] = smoothedADC[0] * sampleLength;
	uiParams[1] = smoothedADC[1] * sampleLength;
	uiParams[2] = (smoothedADC[2] - 0.5f) * 4.0f;
	samplePlayStart = uiParams[0];
	samplePlayEnd = uiParams[1];
	samplerRate = uiParams[2];

	tSampler_setStart(&sampler, samplePlayStart);
	tSampler_setEnd(&sampler, samplePlayEnd);
	tSampler_setRate(&sampler, samplerRate);
//	    tSampler_setCrossfadeLength(&sampler, 500);



	tBuffer_tick(&buff, audioIn);
	sample = tanhf(tSampler_tick(&sampler));
}

void SFXSamplerBPFree(void)
{
	tBuffer_free(&buff);
	tSampler_free(&sampler);
}


//8 sampler - auto ch1
void SFXSamplerAuto1Alloc()
{
	tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
	tBuffer_setRecordMode(&buff, RecordOneShot);
	tSampler_init(&sampler, &buff);
	tSampler_setMode(&sampler, PlayLoop);
}

void SFXSamplerAuto1Frame()
{
}

void SFXSamplerAuto1Tick(float audioIn)
{
	tBuffer_tick(&buff, audioIn);
	sample = tSampler_tick(&sampler);
}

void SFXSamplerAuto1Free(void)
{
	tBuffer_free(&buff);
	tSampler_free(&sampler);
}

//9 sampler - auto ch2
void SFXSamplerAuto2Alloc()
{
	tBuffer_init(&buff, leaf.sampleRate * maxSampleSizeSeconds);
	tBuffer_setRecordMode(&buff, RecordOneShot);
	tSampler_init(&sampler, &buff);
	tSampler_setMode(&sampler, PlayLoop);
}

void SFXSamplerAuto2Frame()
{
}

void SFXSamplerAuto2Tick(float audioIn)
{
	tBuffer_tick(&buff, audioIn);
	sample = tSampler_tick(&sampler);
}

void SFXSamplerAuto2Free(void)
{
	tBuffer_free(&buff);
	tSampler_free(&sampler);
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
	sample = tLockhartWavefolder_tick(&wavefolder1, sample);
	//sample = sample * gain;
	sample = sample + uiParams[2];
	sample = tLockhartWavefolder_tick(&wavefolder2, sample);
	sample = sample + uiParams[3];
	//sample *= .6f;
	sample = tLockhartWavefolder_tick(&wavefolder3, sample);
	//sample = tLockhartWavefolder_tick(&wavefolder4, sample);
	sample *= .6f;
	sample = tanhf(sample);
}

void SFXWaveFolderFree(void)
{
	tLockhartWavefolder_free(&wavefolder1);
	tLockhartWavefolder_free(&wavefolder2);
	tLockhartWavefolder_free(&wavefolder3);
	tLockhartWavefolder_free(&wavefolder4);
}


//13 bitcrusher
void SFXBitcrusherAlloc()
{
	tCrusher_init(&crush);
}

void SFXBitcrusherFrame()
{
}

void SFXBitcrusherTick(float audioIn)
{
	uiParams[0] = smoothedADC[0];
	tCrusher_setQuality (&crush, smoothedADC[0]);
	uiParams[1] = smoothedADC[1];
	tCrusher_setSamplingRatio (&crush, smoothedADC[1]);
	uiParams[2] = smoothedADC[2];
	tCrusher_setRound (&crush, smoothedADC[2]);
	uiParams[3] = smoothedADC[3];
	tCrusher_setOperation (&crush, smoothedADC[3]);
	uiParams[4] = smoothedADC[4] + 1.0f;
	sample = tCrusher_tick(&crush, tanhf(audioIn * (smoothedADC[4] + 1.0f)));
	sample *= .9f;
}

void SFXBitcrusherFree(void)
{
	tCrusher_free(&crush);
}


//14 delay
void SFXDelayAlloc()
{
	tDelay_init(&delay, 24000, 48000);
	tSVF_init(&delayLP, SVFTypeLowpass, 16000.f, .7f);
	tSVF_init(&delayHP, SVFTypeLowpass, 20.f, .7f);
}

void SFXDelayFrame()
{
}

void SFXDelayTick(float audioIn)
{

}

void SFXDelayFree(void)
{
	tDelay_free(&delay);
	tSVF_free(&delayLP);
	tSVF_free(&delayHP);
}



//15 reverb
void SFXReverbAlloc()
{
	tDattorroReverb_init(&reverb);
	tDattorroReverb_setMix(&reverb, 1.0f);
}

void SFXReverbFrame()
{

	uiParams[1] =  faster_mtof(smoothedADC[1]*128.0f);
	tDattorroReverb_setHP(&reverb, uiParams[1]);
	uiParams[2] = faster_mtof(smoothedADC[2]*135.0f);
	tDattorroReverb_setInputFilter(&reverb, uiParams[2]);
	uiParams[3] = faster_mtof(smoothedADC[3]*135.0f);
	tDattorroReverb_setFeedbackFilter(&reverb, uiParams[3]);
}

void SFXReverbTick(float audioIn)
{
	float stereo[2];
	//tDattorroReverb_setInputDelay(&reverb, smoothedADC[1] * 200.f);
	uiParams[0] = smoothedADC[0];
	tDattorroReverb_setSize(&reverb, uiParams[0]);
	uiParams[4] = smoothedADC[4];
	tDattorroReverb_setFeedbackGain(&reverb, uiParams[4]);

	tDattorroReverb_tickStereo(&reverb, audioIn, stereo);
	sample = tanhf(stereo[0]);
	rightOut = tanhf(stereo[1]);
}

void SFXReverbFree(void)
{
	tDattorroReverb_free(&reverb);
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


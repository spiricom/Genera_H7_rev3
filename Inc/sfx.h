/*
 * sfx.h
 *
 *  Created on: Dec 23, 2019
 *      Author: josnyder
 */

#ifndef SFX_H_
#define SFX_H_

#define NUM_VOC_VOICES 8
#define NUM_VOC_OSC 1
#define INV_NUM_VOC_VOICES 0.125
#define INV_NUM_VOC_OSC 1
#define NUM_AUTOTUNE 5
#define NUM_RETUNE 1
#define OVERSAMPLER_RATIO 8
#define OVERSAMPLER_HQ FALSE


extern tPoly poly;
extern tRamp polyRamp[NUM_VOC_VOICES];
extern tSawtooth osc[NUM_VOC_VOICES];
extern int autotuneChromatic;

//PresetNil is used as a counter for the size of the enum
typedef enum _VocodecPreset
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
	Reverb2,
	PresetNil
} VocodecPreset;

void initGlobalSFXObjects();


//1 vocoder internal poly
void SFXVocoderIPAlloc();
void SFXVocoderIPFrame();
void SFXVocoderIPTick(float audioIn);
void SFXVocoderIPFree(void);

//2 vocoder internal mono
void SFXVocoderIMAlloc();
void SFXVocoderIMFrame();
void SFXVocoderIMTick(float audioIn);
void SFXVocoderIMFree(void);

//3 vocoder external
void SFXVocoderEAlloc();
void SFXVocoderEFrame();
void SFXVocoderETick(float audioIn);
void SFXVocoderEFree(void);

//4 pitch shift
void SFXPitchShiftAlloc();
void SFXPitchShiftFrame();
void SFXPitchShiftTick(float audioIn);
void SFXPitchShiftFree(void);

//5 neartune
void SFXNeartuneAlloc();
void SFXNeartuneFrame();
void SFXNeartuneTick(float audioIn);
void SFXNeartuneFree(void);

//6 autotune
void SFXAutotuneAlloc();
void SFXAutotuneFrame();
void SFXAutotuneTick(float audioIn);
void SFXAutotuneFree(void);



//7 sampler - button press
void SFXSamplerBPAlloc();
void SFXSamplerBPFrame();
void SFXSamplerBPTick(float audioIn);
void SFXSamplerBPFree(void);


//8 sampler - auto ch1
void SFXSamplerAuto1Alloc();
void SFXSamplerAuto1Frame();
void SFXSamplerAuto1Tick(float audioIn);
void SFXSamplerAuto1Free(void);

//9 sampler - auto ch2
void SFXSamplerAuto2Alloc();
void SFXSamplerAuto2Frame();
void SFXSamplerAuto2Tick(float audioIn);
void SFXSamplerAuto2Free(void);


//10 distortion tanh
void SFXDistortionTanhAlloc();
void SFXDistortionTanhFrame();
void SFXDistortionTanhTick(float audioIn);
void SFXDistortionTanhFree(void);

//11 distortion shaper function
void SFXDistortionShaperAlloc();
void SFXDistortionShaperFrame();
void SFXDistortionShaperTick(float audioIn);
void SFXDistortionShaperFree(void);

//12 distortion wave folder
void SFXWaveFolderAlloc();
void SFXWaveFolderFrame();
void SFXWaveFolderTick(float audioIn);
void SFXWaveFolderFree(void);


//13 bitcrusher
void SFXBitcrusherAlloc();
void SFXBitcrusherFrame();
void SFXBitcrusherTick(float audioIn);
void SFXBitcrusherFree(void);


//14 delay
void SFXDelayAlloc();
void SFXDelayFrame();
void SFXDelayTick(float audioIn);
void SFXDelayFree(void);


//15 reverb
void SFXReverbAlloc();
void SFXReverbFrame();
void SFXReverbTick(float audioIn);
void SFXReverbFree(void);

//15 reverb2
void SFXReverb2Alloc();
void SFXReverb2Frame();
void SFXReverb2Tick(float audioIn);
void SFXReverb2Free(void);



// MIDI FUNCTIONS
void noteOn(int key, int velocity);
void noteOff(int key, int velocity);
void sustainOn(void);
void sustainOff(void);
void toggleBypass(void);
void toggleSustain(void);

void calculateFreq(int voice);
void calculatePeriodArray(void);
float nearestPeriod(float period);

void clearNotes(void);

void ctrlInput(int ctrl, int value);


#endif /* SFX_H_ */

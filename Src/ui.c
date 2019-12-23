/*
 * ui.c
 *
 *  Created on: Dec 18, 2018
 *      Author: jeffsnyder
 */
#include "main.h"
#include "ui.h"
#include "ssd1306.h"
#include "gfx.h"
#include "custom_fonts.h"
#include "audiostream.h"
#include "tunings.h"

uint16_t ADC_values[NUM_ADC_CHANNELS] __ATTR_RAM_D2;

#define NUM_CHARACTERS_PER_PRESET_NAME 16
char* modeNames[PresetNil];
char* modeNamesDetails[PresetNil];
char* shortModeNames[PresetNil];
char* paramNames[PresetNil][NUM_ADC_CHANNELS];

uint8_t buttonValues[NUM_BUTTONS];
uint8_t buttonValuesPrev[NUM_BUTTONS];
uint32_t buttonCounters[NUM_BUTTONS];
uint32_t buttonPressed[NUM_BUTTONS];
uint32_t buttonReleased[NUM_BUTTONS];
uint32_t currentTuning = 0;
GFX theGFX;

char oled_buffer[32];
VocodecPreset currentPreset = Reverb;
VocodecPreset previousPreset;
uint8_t loadingPreset = 0;

float uiPitchFactor, uiFormantWarp;

uint8_t samplerRecording;

void OLED_init(I2C_HandleTypeDef* hi2c)
{
	  //start up that OLED display
	  ssd1306_begin(hi2c, SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);

	  HAL_Delay(5);

	  //clear the OLED display buffer
	  for (int i = 0; i < 512; i++)
	  {
		  buffer[i] = 0;
	  }
	  initModeNames();
	  //display the blank buffer on the OLED
	  //ssd1306_display_full_buffer();

	  //initialize the graphics library that lets us write things in that display buffer
	  GFXinit(&theGFX, 128, 32);

	  //set up the monospaced font

	  //GFXsetFont(&theGFX, &C649pt7b); //funny c64 text monospaced but very large
	  //GFXsetFont(&theGFX, &DINAlternateBold9pt7b); //very serious and looks good - definitely not monospaced can fit 9 Ms
	  //GFXsetFont(&theGFX, &DINCondensedBold9pt7b); // very condensed and looks good - definitely not monospaced can fit 9 Ms
	  GFXsetFont(&theGFX, &EuphemiaCAS9pt7b); //this one is elegant but definitely not monospaced can fit 9 Ms
	  //GFXsetFont(&theGFX, &GillSans9pt7b); //not monospaced can fit 9 Ms
	  //GFXsetFont(&theGFX, &Futura9pt7b); //not monospaced can fit only 7 Ms
	  //GFXsetFont(&theGFX, &FUTRFW8pt7b); // monospaced, pretty, (my old score font) fits 8 Ms
	  //GFXsetFont(&theGFX, &nk57_monospace_cd_rg9pt7b); //fits 12 characters, a little crammed
	  //GFXsetFont(&theGFX, &nk57_monospace_no_rg9pt7b); // fits 10 characters
	  //GFXsetFont(&theGFX, &nk57_monospace_no_rg7pt7b); // fits 12 characters
	  //GFXsetFont(&theGFX, &nk57_monospace_no_bd7pt7b); //fits 12 characters
	  //GFXsetFont(&theGFX, &nk57_monospace_cd_rg7pt7b); //fits 18 characters

	  GFXsetTextColor(&theGFX, 1, 0);
	  GFXsetTextSize(&theGFX, 1);

	  //ssd1306_display_full_buffer();

	  OLEDclear();
	  OLED_writePreset();
	  OLED_draw();
	//sdd1306_invertDisplay(1);
}

void setLED_Edit(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
	}
}


void setLED_USB(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
	}
}


void setLED_1(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	}
}

void setLED_2(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
	}
}


void setLED_A(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
	}
}

void setLED_B(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);
	}
}

void setLED_C(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_RESET);
	}
}

void setLED_leftout_clip(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
	}
}

void setLED_rightout_clip(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	}
}

void setLED_leftin_clip(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
	}
}

void setLED_rightin_clip(uint8_t onOff)
{
	if (onOff)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
	}
}

uint8_t buttonState[10];

void buttonCheck(void)
{
	if (codecReady)
	{
		buttonValues[0] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13); //edit
		buttonValues[1] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12); //left
		buttonValues[2] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14); //right
		buttonValues[3] = !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_11); //down
		buttonValues[4] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_15); //up
		buttonValues[5] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);  // A
		buttonValues[6] = !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_7);  // B
		buttonValues[7] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11); // C
		buttonValues[8] = !HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_11); // D
		buttonValues[9] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10); // E

		for (int i = 0; i < NUM_BUTTONS; i++)
		{
			// Presses and releases cannot last over consecutive checks
			buttonPressed[i] = 0;
			buttonReleased[i] = 0;
			if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] < 1))
			{
				buttonCounters[i]++;
			}
			if ((buttonValues[i] != buttonValuesPrev[i]) && (buttonCounters[i] >= 1))
			{
				if (buttonValues[i] == 1)
				{
					buttonPressed[i] = 1;
				}
				else if (buttonValues[i] == 0)
				{
					buttonReleased[i] = 1;
				}
				buttonValuesPrev[i] = buttonValues[i];
				buttonCounters[i] = 0;
			}
		}

		// make some if statements if you want to find the "attack" of the buttons (getting the "press" action)
		// we'll need if statements for each button  - maybe should go to functions that are dedicated to each button?

		// TODO: buttons C and E are connected to pins that are used to set up the codec over I2C - we need to reconfigure those pins in some kind of button init after the codec is set up. not done yet.

		if (buttonPressed[0] == 1)
		{
		}

		// left press
		if (buttonPressed[1] == 1)
		{
			previousPreset = currentPreset;
			if (currentPreset <= 0) currentPreset = PresetNil - 1;
			else currentPreset--;

			loadingPreset = 1;
			OLED_writePreset();
		}

		// right press
		if (buttonPressed[2] == 1)
		{
			previousPreset = currentPreset;
			if (currentPreset >= PresetNil - 1) currentPreset = 0;
			else currentPreset++;

			loadingPreset = 1;
			OLED_writePreset();
		}

		if (buttonPressed[7] == 1)
		{
			//GFXsetFont(&theGFX, &DINCondensedBold9pt7b);
			keyCenter = (keyCenter + 1) % 12;
			OLEDclearLine(SecondLine);
			OLEDwriteString("KEY: ", 5, 0, SecondLine);
			OLEDwritePitchClass(keyCenter+60, 64, SecondLine);
		}

		if (buttonPressed[8] == 1)
		{
			if (currentTuning == 0)
			{
				currentTuning = NUM_TUNINGS - 1;
			}
			else
			{
				currentTuning = (currentTuning - 1);
			}
			changeTuning();

		}

		if (buttonPressed[9] == 1)
		{

			currentTuning = (currentTuning + 1) % NUM_TUNINGS;
			changeTuning();
		}
	}
}

void changeTuning()
{
	for (int i = 0; i < 12; i++)
	{
		centsDeviation[i] = tuningPresets[currentTuning][i];

	}
	if (currentTuning == 0)
	{
		//setLED_C(0);
	}
	else
	{
		///setLED_C(1);
	}
	if (currentPreset == AutotuneMono)
	{
		calculatePeriodArray();
	}
	GFXsetFont(&theGFX, &EuphemiaCAS7pt7b);
	OLEDclearLine(SecondLine);
	OLEDwriteString("T ", 2, 0, SecondLine);
	OLEDwriteInt(currentTuning, 2, 12, SecondLine);
	OLEDwriteString(tuningNames[currentTuning], 6, 40, SecondLine);

}

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


static void initModeNames(void)
{
	modeNames[VocoderInternalPoly] = "VOCODER IP";
	shortModeNames[VocoderInternalPoly] = "V1";
	modeNamesDetails[VocoderInternalPoly] = "INTERNAL POLY";
	paramNames[VocoderInternalPoly][0] = " ";
	paramNames[VocoderInternalPoly][1] = " ";
	paramNames[VocoderInternalPoly][2] = " ";
	paramNames[VocoderInternalPoly][3] = " ";
	paramNames[VocoderInternalPoly][4] = " ";
	paramNames[VocoderInternalPoly][5] = " ";

	modeNames[VocoderInternalMono] = "VOCODER IM";
	shortModeNames[VocoderInternalMono] = "V2";
	modeNamesDetails[VocoderInternalMono] = "INTERNAL MONO";
	paramNames[VocoderInternalMono][0] = " ";
	paramNames[VocoderInternalMono][1] = " ";
	paramNames[VocoderInternalMono][2] = " ";
	paramNames[VocoderInternalMono][3] = " ";
	paramNames[VocoderInternalMono][4] = " ";
	paramNames[VocoderInternalMono][5] = " ";

	modeNames[VocoderExternal] = "VOCODEC E";
	shortModeNames[VocoderExternal] = "VE";
	modeNamesDetails[VocoderExternal] = "EXTERNAL";
	paramNames[VocoderExternal][0] = " ";
	paramNames[VocoderExternal][1] = " ";
	paramNames[VocoderExternal][2] = " ";
	paramNames[VocoderExternal][3] = " ";
	paramNames[VocoderExternal][4] = " ";
	paramNames[VocoderExternal][5] = " ";

	modeNames[Pitchshift] = "PITCHSHIFT";
	shortModeNames[Pitchshift] = "PS";
	modeNamesDetails[Pitchshift] = "";
	paramNames[Pitchshift][0] = "PITCH";
	paramNames[Pitchshift][1] = "FORMANT";
	paramNames[Pitchshift][2] = "F AMT";
	paramNames[Pitchshift][3] = " ";
	paramNames[Pitchshift][4] = " ";
	paramNames[Pitchshift][5] = " ";

	modeNames[AutotuneMono] = "NEARTUNE";
	shortModeNames[AutotuneMono] = "NT";
	modeNamesDetails[AutotuneMono] = "";
	paramNames[AutotuneMono][0] = " ";
	paramNames[AutotuneMono][1] = " ";
	paramNames[AutotuneMono][2] = " ";
	paramNames[AutotuneMono][3] = " ";
	paramNames[AutotuneMono][4] = " ";
	paramNames[AutotuneMono][5] = " ";

	modeNames[AutotunePoly] = "AUTOTUNE";
	shortModeNames[AutotunePoly] = "AT";
	modeNamesDetails[AutotunePoly] = "";
	paramNames[AutotunePoly][0] = " ";
	paramNames[AutotunePoly][1] = " ";
	paramNames[AutotunePoly][2] = " ";
	paramNames[AutotunePoly][3] = " ";
	paramNames[AutotunePoly][4] = " ";
	paramNames[AutotunePoly][5] = " ";

	modeNames[SamplerButtonPress] = "SAMPLER BP";
	shortModeNames[SamplerButtonPress] = "SB";
	modeNamesDetails[SamplerButtonPress] = "PRESS BUTTON A";
	paramNames[SamplerButtonPress][0] = "START TIME";
	paramNames[SamplerButtonPress][1] = "END TIME";
	paramNames[SamplerButtonPress][2] = "SPEED";
	paramNames[SamplerButtonPress][3] = " ";
	paramNames[SamplerButtonPress][4] = " ";
	paramNames[SamplerButtonPress][5] = " ";

	modeNames[SamplerAutoGrabInternal] = "AUTOSAMP1";
	shortModeNames[SamplerAutoGrabInternal] = "A1";
	modeNamesDetails[SamplerAutoGrabInternal] = "CH1 TRIG";
	paramNames[SamplerAutoGrabInternal][0] = " ";
	paramNames[SamplerAutoGrabInternal][1] = " ";
	paramNames[SamplerAutoGrabInternal][2] = " ";
	paramNames[SamplerAutoGrabInternal][3] = " ";
	paramNames[SamplerAutoGrabInternal][4] = " ";
	paramNames[SamplerAutoGrabInternal][5] = " ";

	modeNames[SamplerAutoGrabExternal] = "AUTOSAMP2";
	shortModeNames[SamplerAutoGrabExternal] = "A2";
	modeNamesDetails[SamplerAutoGrabExternal] = "CH2 TRIG";
	paramNames[SamplerAutoGrabExternal][0] = " ";
	paramNames[SamplerAutoGrabExternal][1] = " ";
	paramNames[SamplerAutoGrabExternal][2] = " ";
	paramNames[SamplerAutoGrabExternal][3] = " ";
	paramNames[SamplerAutoGrabExternal][4] = " ";
	paramNames[SamplerAutoGrabExternal][5] = " ";

	modeNames[DistortionTanH] = "DISTORT1";
	shortModeNames[DistortionTanH] = "D1";
	modeNamesDetails[DistortionTanH] = "TANH FUNCTION";
	paramNames[DistortionTanH][0] = "GAIN";
	paramNames[DistortionTanH][1] = " ";
	paramNames[DistortionTanH][2] = " ";
	paramNames[DistortionTanH][3] = " ";
	paramNames[DistortionTanH][4] = " ";
	paramNames[DistortionTanH][5] = " ";

	modeNames[DistortionShaper] = "DISTORT2";
	shortModeNames[DistortionShaper] = "D2";
	modeNamesDetails[DistortionShaper] = "WAVESHAPER";
	paramNames[DistortionShaper][0] = "GAIN";
	paramNames[DistortionShaper][1] = " ";
	paramNames[DistortionShaper][2] = " ";
	paramNames[DistortionShaper][3] = " ";
	paramNames[DistortionShaper][4] = " ";
	paramNames[DistortionShaper][5] = " ";

	modeNames[Wavefolder] = "WAVEFOLD";
	shortModeNames[Wavefolder] = "WF";
	modeNamesDetails[Wavefolder] = "SERGE STYLE";
	paramNames[Wavefolder][0] = "GAIN";
	paramNames[Wavefolder][1] = " ";
	paramNames[Wavefolder][2] = " ";
	paramNames[Wavefolder][3] = " ";
	paramNames[Wavefolder][4] = " ";
	paramNames[Wavefolder][5] = " ";

	modeNames[BitCrusher] = "BITCRUSH";
	shortModeNames[BitCrusher] = "BC";
	modeNamesDetails[BitCrusher] = "AHH HALP ME";
	paramNames[BitCrusher][0] = "GAIN";
	paramNames[BitCrusher][1] = " ";
	paramNames[BitCrusher][2] = " ";
	paramNames[BitCrusher][3] = " ";
	paramNames[BitCrusher][4] = " ";
	paramNames[BitCrusher][5] = " ";

	modeNames[Delay] = "DELAY";
	shortModeNames[Delay] = "DL";
	modeNamesDetails[Delay] = "";
	paramNames[Delay][0] = " ";
	paramNames[Delay][1] = " ";
	paramNames[Delay][2] = " ";
	paramNames[Delay][3] = " ";
	paramNames[Delay][4] = " ";
	paramNames[Delay][5] = " ";

	modeNames[Reverb] = "REVERB";
	shortModeNames[Reverb] = "RV";
	modeNamesDetails[Reverb] = "DATTORRO ALG";
	paramNames[Reverb][0] = " ";
	paramNames[Reverb][1] = " ";
	paramNames[Reverb][2] = " ";
	paramNames[Reverb][3] = " ";
	paramNames[Reverb][4] = " ";
	paramNames[Reverb][5] = " ";
}

void OLED_writePreset()
{
	GFXsetFont(&theGFX, &EuphemiaCAS9pt7b);
	OLEDclear();
	char tempString[24];
	itoa((currentPreset+1), tempString, 10);
	strcat(tempString, ":");
	strcat(tempString, modeNames[currentPreset]);
	int myLength = strlen(tempString);
	//OLEDwriteInt(currentPreset+1, 2, 0, FirstLine);
	//OLEDwriteString(":", 1, 20, FirstLine);
	//OLEDwriteString(modeNames[currentPreset], 12, 24, FirstLine);
	OLEDwriteString(tempString, myLength, 0, FirstLine);
	GFXsetFont(&theGFX, &EuphemiaCAS7pt7b);
	OLEDwriteString(modeNamesDetails[currentPreset], strlen(modeNamesDetails[currentPreset]), 0, SecondLine);

}

void OLED_draw()
{
	ssd1306_display_full_buffer();
}

/// OLED Stuff

void OLEDdrawPoint(int16_t x, int16_t y, uint16_t color)
{
	GFXwritePixel(&theGFX, x, y, color);
	//ssd1306_display_full_buffer();
}

void OLEDdrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
	GFXwriteLine(&theGFX, x0, y0, x1, y1, color);
	//ssd1306_display_full_buffer();
}

void OLEDdrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
	GFXfillCircle(&theGFX, x0, y0, r, color);
	//ssd1306_display_full_buffer();
}


void OLEDclear()
{
	GFXfillRect(&theGFX, 0, 0, 128, 32, 0);
	//ssd1306_display_full_buffer();
}

void OLEDclearLine(OLEDLine line)
{
	GFXfillRect(&theGFX, 0, (line%2)*16, 128, 16*((line/2)+1), 0);
	//ssd1306_display_full_buffer();
}

void OLEDwriteString(char* myCharArray, uint8_t arrayLength, uint8_t startCursor, OLEDLine line)
{
	uint8_t cursorX = startCursor;
	uint8_t cursorY = 15 + (16 * (line%2));
	GFXsetCursor(&theGFX, cursorX, cursorY);

	GFXfillRect(&theGFX, startCursor, line*16, arrayLength*12, (line*16)+16, 0);
	for (int i = 0; i < arrayLength; ++i)
	{
		GFXwrite(&theGFX, myCharArray[i]);
	}
	//ssd1306_display_full_buffer();
}

void OLEDwriteLine(char* myCharArray, uint8_t arrayLength, OLEDLine line)
{
	if (line == FirstLine)
	{
		GFXfillRect(&theGFX, 0, 0, 128, 16, 0);
		GFXsetCursor(&theGFX, 4, 15);
	}
	else if (line == SecondLine)
	{
		GFXfillRect(&theGFX, 0, 16, 128, 16, 0);
		GFXsetCursor(&theGFX, 4, 31);
	}
	else if (line == BothLines)
	{
		GFXfillRect(&theGFX, 0, 0, 128, 32, 0);
		GFXsetCursor(&theGFX, 4, 15);
	}
	for (int i = 0; i < arrayLength; ++i)
	{
		GFXwrite(&theGFX, myCharArray[i]);
	}
	//ssd1306_display_full_buffer();
}

void OLEDwriteInt(uint32_t myNumber, uint8_t numDigits, uint8_t startCursor, OLEDLine line)
{
	int len = OLEDparseInt(oled_buffer, myNumber, numDigits);

	OLEDwriteString(oled_buffer, len, startCursor, line);
}

void OLEDwriteIntLine(uint32_t myNumber, uint8_t numDigits, OLEDLine line)
{
	int len = OLEDparseInt(oled_buffer, myNumber, numDigits);

	OLEDwriteLine(oled_buffer, len, line);
}

void OLEDwritePitch(float midi, uint8_t startCursor, OLEDLine line)
{
	int len = OLEDparsePitch(oled_buffer, midi);

	OLEDwriteString(oled_buffer, len, startCursor, line);
}

void OLEDwritePitchClass(float midi, uint8_t startCursor, OLEDLine line)
{
	int len = OLEDparsePitchClass(oled_buffer, midi);

	OLEDwriteString(oled_buffer, len, startCursor, line);
}

void OLEDwritePitchLine(float midi, OLEDLine line)
{
	int len = OLEDparsePitch(oled_buffer, midi);

	OLEDwriteLine(oled_buffer, len, line);
}

void OLEDwriteFixedFloat(float input, uint8_t numDigits, uint8_t numDecimal, uint8_t startCursor, OLEDLine line)
{
	int len = OLEDparseFixedFloat(oled_buffer, input, numDigits, numDecimal);

	OLEDwriteString(oled_buffer, len, startCursor, line);
}

void OLEDwriteFixedFloatLine(float input, uint8_t numDigits, uint8_t numDecimal, OLEDLine line)
{
	int len = OLEDparseFixedFloat(oled_buffer, input, numDigits, numDecimal);

	OLEDwriteLine(oled_buffer, len, line);
}

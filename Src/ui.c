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

uint8_t buttonValues[NUM_BUTTONS];
uint8_t buttonValuesPrev[NUM_BUTTONS];
uint32_t buttonCounters[NUM_BUTTONS];
uint32_t buttonPressed[NUM_BUTTONS];
uint32_t buttonReleased[NUM_BUTTONS];
uint32_t currentTuning = 0;
GFX theGFX;
char oled_buffer[32];
VocodecPreset currentPreset;
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

	  //display the blank buffer on the OLED
	  //ssd1306_display_full_buffer();

	  //initialize the graphics library that lets us write things in that display buffer
	  GFXinit(&theGFX, 128, 32);

	  //set up the monospaced font
	  GFXsetFont(&theGFX, &Lato_Hairline_16);

	  GFXsetTextColor(&theGFX, 1, 0);
	  GFXsetTextSize(&theGFX, 1);

	  //ssd1306_display_full_buffer();

	  // should eventually move this elsewhere
	  currentPreset = DistortionTanH;
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
		keyCenter = (keyCenter + 1) % 12;
		OLEDclearLine(SecondLine);
		OLEDwriteString("Key: ", 5, 0, SecondLine);
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
void OLED_writePreset()
{
	if (currentPreset == VocoderInternalPoly)
	{
		OLEDwriteString("1:VOCODE I P", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == VocoderInternalMono)
	{
		OLEDwriteString("2:VOCODE I M", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == VocoderExternal)
	{
		OLEDwriteString("3:VOCODE E  ", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == Pitchshift)
	{
		OLEDwriteString("4:PSHIFT    ", 12, 0, FirstLine);
		//OLEDwriteFixedFloat(uiPitchFactor, 3, 2, 0, SecondLine);
		//OLEDwriteFixedFloat(uiFormantWarp, 4, 2, 64, SecondLine);
	}
	else if (currentPreset == AutotuneMono)
	{
		OLEDwriteString("5:NEARTUNE  ", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == AutotunePoly)
	{
		OLEDwriteString("6:AUTOTUNE  ", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == SamplerButtonPress)
	{
		OLEDwriteString("7:SAMPLER BP", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == SamplerAutoGrabInternal)
	{
		OLEDwriteString("8:AUTOSAMP I", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == SamplerAutoGrabExternal)
	{
		OLEDwriteString("9:AUTOSAMP E", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == DistortionTanH)
	{
		OLEDwriteString("10:DISTORT 1", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == DistortionShaper)
	{
		OLEDwriteString("11:DISTORT 2", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == Wavefolder)
	{
		OLEDwriteString("12:WAVEFOLDR", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == BitCrusher)
	{
		OLEDwriteString("13:BITCRUSHR", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == Delay)
	{
		OLEDwriteString("14:DELAY", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == Reverb)
	{
		OLEDwriteString("15:REVERB", 12, 0, FirstLine);
		//OLEDwriteString("            ", 12, 0, SecondLine);
	}
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

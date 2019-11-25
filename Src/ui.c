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

#define NUM_BUTTONS 10

uint16_t ADC_values[NUM_ADC_CHANNELS] __ATTR_RAM_D2;

uint8_t buttonValues[NUM_BUTTONS];
uint8_t buttonValuesPrev[NUM_BUTTONS];
uint32_t buttonCounters[NUM_BUTTONS];
uint32_t buttonPressed[NUM_BUTTONS];

GFX theGFX;
uint8_t oled_buffer[32];
VocodecPreset currentPreset;
VocodecPreset previousPreset;
uint8_t loadingPreset = 0;

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
	  ssd1306_display_full_buffer();

	  //initialize the graphics library that lets us write things in that display buffer
	  GFXinit(&theGFX, 128, 32);

	  //set up the monospaced font
	  GFXsetFont(&theGFX, &Monospaced_plain_18);

	  GFXsetTextColor(&theGFX, 1, 0);
	  GFXsetTextSize(&theGFX, 1);

	  ssd1306_display_full_buffer();

	  // should eventually move this elsewhere
	  currentPreset = VocoderInternal;
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
	buttonValues[8] = !HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_12); // D
	buttonValues[9] = !HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10); // E

	for (int i = 0; i < NUM_BUTTONS; i++)
	{
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
		  buttonValuesPrev[i] = buttonValues[i];
		  buttonCounters[i] = 0;
	  }
	}

	// make some if statements if you want to find the "attack" of the buttons (getting the "press" action)
	// we'll need if statements for each button  - maybe should go to functions that are dedicated to each button?

	// TODO: buttons C and E are connected to pins that are used to set up the codec over I2C - we need to reconfigure those pins in some kind of button init after the codec is set up. not done yet.

	if (buttonPressed[0] == 1)
	{
		buttonPressed[0] = 0;
	}

	// left press
	if (buttonPressed[1] == 1)
	{
		previousPreset = currentPreset;
		if (currentPreset <= 0) currentPreset = PresetNil - 1;
		else currentPreset--;

		loadingPreset = 1;

		buttonPressed[1] = 0;
	}

	// right press
	if (buttonPressed[2] == 1)
	{
		previousPreset = currentPreset;
		if (currentPreset >= PresetNil - 1) currentPreset = 0;
		else currentPreset++;

		loadingPreset = 1;

		buttonPressed[2] = 0;
	}
}

void OLED_draw()
{
	if (currentPreset == VocoderInternal)
	{
		OLEDwriteString("1:VOCODE I  ", 12, 0, FirstLine);
		OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == VocoderExternal)
	{
		OLEDwriteString("2:VOCODE E  ", 12, 0, FirstLine);
		OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == Pitchshift)
	{
		OLEDwriteString("3:PSHIFT    ", 12, 0, FirstLine);
		OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == AutotuneMono)
	{
		OLEDwriteString("4:NEARTUNE  ", 12, 0, FirstLine);
		OLEDwriteString("            ", 12, 0, SecondLine);
	}
	else if (currentPreset == AutotunePoly)
	{
		OLEDwriteString("5:AUTOTUNE  ", 12, 0, FirstLine);
		OLEDwriteString("            ", 12, 0, SecondLine);
	}
}

/// OLED Stuff

void OLEDdrawPoint(int16_t x, int16_t y, uint16_t color)
{
	GFXwritePixel(&theGFX, x, y, color);
	ssd1306_display_full_buffer();
}

void OLEDdrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
	GFXwriteLine(&theGFX, x0, y0, x1, y1, color);
	ssd1306_display_full_buffer();
}

void OLEDdrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
	GFXfillCircle(&theGFX, x0, y0, r, color);
	ssd1306_display_full_buffer();
}


void OLEDclear()
{
	GFXfillRect(&theGFX, 0, 0, 128, 32, 0);
	ssd1306_display_full_buffer();
}

void OLEDclearLine(OLEDLine line)
{
	GFXfillRect(&theGFX, 0, (line%2)*16, 128, 16*((line/2)+1), 0);
	ssd1306_display_full_buffer();
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
	ssd1306_display_full_buffer();
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
	ssd1306_display_full_buffer();
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

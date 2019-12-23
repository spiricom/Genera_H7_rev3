/*
 * ui.h
 *
 *  Created on: Aug 30, 2019
 *      Author: jeffsnyder
 */

#ifndef UI_H_
#define UI_H_
#define NUM_ADC_CHANNELS 6
#define NUM_BUTTONS 10

extern uint16_t ADC_values[NUM_ADC_CHANNELS];
extern char oled_buffer[32];

extern uint8_t buttonValues[NUM_BUTTONS];
extern uint32_t buttonPressed[NUM_BUTTONS];
extern uint32_t buttonReleased[NUM_BUTTONS];

extern uint8_t currentPreset;
extern uint8_t previousPreset;
extern uint8_t loadingPreset;
// Display values
extern float uiParams[NUM_ADC_CHANNELS];


typedef enum _OLEDLine
{
	FirstLine = 0,
	SecondLine,
	BothLines,
	NilLine
} OLEDLine;

void OLED_init(I2C_HandleTypeDef* hi2c);

static void initModeNames(void);

void setLED_Edit(uint8_t onOff);

void setLED_USB(uint8_t onOff);

void setLED_1(uint8_t onOff);

void setLED_2(uint8_t onOff);

void setLED_A(uint8_t onOff);

void setLED_B(uint8_t onOff);

void setLED_C(uint8_t onOff);

void setLED_leftout_clip(uint8_t onOff);

void setLED_rightout_clip(uint8_t onOff);

void setLED_leftin_clip(uint8_t onOff);

void setLED_rightin_clip(uint8_t onOff);


void buttonCheck(void);

void changeTuning(void);

void OLED_writePreset(void);

void OLED_writeParameter(uint8_t whichParam);

void OLED_draw(void);

void OLEDclear(void);

void OLEDclearLine(OLEDLine line);

void OLEDwriteString(char* myCharArray, uint8_t arrayLength, uint8_t startCursor, OLEDLine line);

void OLEDwriteLine(char* myCharArray, uint8_t arrayLength, OLEDLine line);

void OLEDwriteInt(uint32_t myNumber, uint8_t numDigits, uint8_t startCursor, OLEDLine line);

void OLEDwriteIntLine(uint32_t myNumber, uint8_t numDigits, OLEDLine line);

void OLEDwritePitch(float midi, uint8_t startCursor, OLEDLine line);

void OLEDwritePitchClass(float midi, uint8_t startCursor, OLEDLine line);

void OLEDwritePitchLine(float midi, OLEDLine line);

void OLEDwriteFixedFloat(float input, uint8_t numDigits, uint8_t numDecimal, uint8_t startCursor, OLEDLine line);

void OLEDwriteFixedFloatLine(float input, uint8_t numDigits, uint8_t numDecimal, OLEDLine line);

#endif /* UI_H_ */


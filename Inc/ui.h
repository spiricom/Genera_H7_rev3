/*
 * ui.h
 *
 *  Created on: Aug 30, 2019
 *      Author: jeffsnyder
 */

#ifndef UI_H_
#define UI_H_
#define NUM_ADC_CHANNELS 6
extern uint16_t ADC_values[NUM_ADC_CHANNELS];
extern uint8_t oled_buffer[32];

extern uint8_t currentPreset;
extern float maxIn;
extern float minIn;

typedef enum _OLEDLine
{
	FirstLine = 0,
	SecondLine,
	BothLines,
	NilLine
} OLEDLine;

typedef enum _VocodecPreset
{
	VocoderInternal = 0,
	VocoderExternal,
	Pitchshift,
	AutotuneMono,
	AutotunePoly,
	PresetNil
} VocodecPreset;

void OLED_init(I2C_HandleTypeDef* hi2c);

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

void OLED_draw();

void OLEDdrawPoint(int16_t x, int16_t y, uint16_t color);
void OLEDdrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void OLEDdrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void OLEDclear();
void OLEDclearLine(OLEDLine line);
void OLEDwriteString(char* myCharArray, uint8_t arrayLength, uint8_t startCursor, OLEDLine line);
void OLEDwriteLine(char* myCharArray, uint8_t arrayLength, OLEDLine line);
void OLEDwriteInt(uint32_t myNumber, uint8_t numDigits, uint8_t startCursor, OLEDLine line);
void OLEDwriteIntLine(uint32_t myNumber, uint8_t numDigits, OLEDLine line);
void OLEDwritePitch(float midi, uint8_t startCursor, OLEDLine line);
void OLEDwritePitchLine(float midi, OLEDLine line);
void OLEDwriteFixedFloat(float input, uint8_t numDigits, uint8_t numDecimal, uint8_t startCursor, OLEDLine line);
void OLEDwriteFixedFloatLine(float input, uint8_t numDigits, uint8_t numDecimal, OLEDLine line);
#endif /* UI_H_ */


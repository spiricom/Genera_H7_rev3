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
extern uint8_t previousPreset;
extern uint8_t loadingPreset;

typedef enum _OLEDLine
{
	FirstLine = 0,
	SecondLine,
	BothLines,
	NilLine
} OLEDLine;

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

#endif /* UI_H_ */


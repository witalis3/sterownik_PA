/*
 * sterownik_FT81x.h
 *
 *  Created on: 18 lut 2021
 *      Author: witek
 */
#include "Arduino.h"
#ifndef STEROWNIK_FT81X_H_
#define STEROWNIK_FT81X_H_

//#define DEBUG
#define CZAS_PETLI

/*
 * wejścia/wyjścia cyfrowe:
 * D0, D1 zarezerowowane dla serial debug?? a może inny serial? -> inny niż jest przez USB
 *
 */

#define PDPin 				54	// A0
#define CS					55	// A1
#define WY_ALARMU_PIN		4	// PIN wyłączający zasilanie PA - alarm - stan aktywny wysoki
#define RESET_ALARMU_PIN	5	// PIN resetujący wyłączenie zasilania PA; aktywny jest stan niski
#define ALARM_OD_IDD_PIN	6	// informacja o zadziałaniu zabezpieczenia od prądu w PA (zadziałanie BTSa)
#define WE_PTT_PIN			7	// wejście informacji o PTT (TRX zwiera do masy, gdy przechodzi na nadawanie); stan aktywny niski
#define WY_PTT_PIN			8	// wyjście PTT (przekaźniki w PA)
#define BIAS_PIN			9	// wyjście na BIAS
#define FAN_ON_PIN			13	// włączenie wentylatora
#define FAN1_PIN			12	// pierwszy bieg wentylatora
#define FAN2_PIN			11	// drugi bieg wentylatora
#define FAN3_PIN			10	// trzeci bieg wentylatora
#define CZAS_PETLI_PIN		19	// RxD1 PD2

#define doPin_blokada       44	// aktywny stan wysoki - blokada głównie od temperatury
#define doPin_errLED      	47	// dioda wystąpienia jakiegoś błędu - aktywny stan wysoki - jak jest błąd - stan wysoki i mruga

/*
 * wejścia analogowe:
 */
#define TEMP1_PIN			A12		// tranzystor 1
#define TEMP2_PIN			A13		// tranzystor 2
#define TEMP3_PIN			A14		// wejście dla temperatury radiatora
#define IDD_PIN				A15		// pomiar prądu PA

#define thresholdTemperaturAirOn1   50
#define thresholdTemperaturTransistorMax	90		// temperatura tranzystora (z termistora nr 1), przy której PA jest blokowane - STBY

#define inputFactorVoltage (5.0/1023.0)
#ifdef ACS758
#define pa1AmperFactor (inputFactorVoltage * (125/2.5))    // 20mV/A ACS758LCB-100B
#define pa1AmperOffset (1023/5 * 2.505)                     // 2.5V z czujnika Hallla -> zmierzyć i wstawić
#else
#define pa1AmperFactor (inputFactorVoltage * (22.0/5.0))    // 3k Ris w BTS50085
#define pa1AmperOffset (0.0)                     // 0.0V	- pomiar z BTS50085 - od zera
#endif


// VGA color palette
#define VGA_BLACK		0x0000
#define VGA_WHITE		0xFFFF
//#define VGA_RED			0xF800
#define VGA_RED			0xFF0000

#define VGA_GREEN		0x0400
#define VGA_BLUE		0x001F
#define VGA_SILVER		0xC618
#define VGA_GRAY		0x8410
#define VGA_MAROON		0x8000
//#define VGA_YELLOW		0xFFE0
#define VGA_YELLOW		0xFFFF00

#define VGA_OLIVE		0x8400
#define VGA_LIME		0x07E0
#define VGA_AQUA		0x07FF
#define VGA_TEAL		0x0410
#define VGA_NAVY		0x0010
#define VGA_FUCHSIA		0xF81F
#define VGA_PURPLE		0x8010
#define VGA_TRANSPARENT	0xFFFFFFFF

int getFontXsize(byte font);
int getFontYsize(byte font);

void fillRect(int x1, int y1, int x2, int y2, long color);
void drawLine(int x1, int y1, int x2, int y2);
void printNumI(float value, int x, int y, byte font);
void printNumF(float value, byte dec, int x, int y, byte font);

void read_inputs();
float getTemperatura(uint8_t pin);
int getTempInt(uint8_t pin);
void FanController(byte co);

#endif /* STEROWNIK_FT81X_H_ */

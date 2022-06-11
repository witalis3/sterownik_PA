/*
 * sterownik_FT81x.h
 *
 *  Created on: 18 lut 2021
 *      Author: witek
 */
//#include "Arduino.h"
#ifndef STEROWNIK_FT81X_H_
#define STEROWNIK_FT81X_H_

#define DEBUG		// na RxD1 (złącze J7 RS232 PIN 3)
//#define CZAS_PETLI		// wyklucza użycie gniazda RS232
//#define ACS712
#define ACS713

/*
 * wejścia/wyjścia cyfrowe:
 * D0, D1, D2, D3 wolne...
 *
 */

#define PDPin 				54	// A0 FT810
#define CS					55	// A1 FT810
#define WY_ALARMU_PIN		4	// PIN wyłączający zasilanie PA - alarm - stan aktywny wysoki (sterownik J5-2) ToDo
#define RESET_ALARMU_PIN	5	// PIN resetujący wyłączenie zasilania PA; aktywny jest stan niski (sterownik J5-3) jest obsługa
#define ALARM_OD_IDD_PIN	6	// informacja o zadziałaniu zabezpieczenia od prądu w PA (zadziałanie BTSa) jest obsługa, stan aktywny wysoki
#define WE_PTT_PIN			7	// wejście informacji o PTT; stan aktywny niski
/*
 * BLOKADA_PTT na schemacie sterownika -> w sekwencerze blokuje PTT (J8-2)
 * blokada PTT dla QRP/QRO (standby)
 * blokada PTT od alarmów
 */
#define BLOKADA_PTT_PIN       8	// aktywny stan wysoki jest obsługa
#define BIAS_PIN			9	// wyjście na BIAS; niewykorzystane
#define FAN_ON_PIN			13	// włączenie wentylatora
#define FAN1_PIN			12	// pierwszy bieg wentylatora
#define FAN2_PIN			11	// drugi bieg wentylatora
#define FAN3_PIN			10	// trzeci bieg wentylatora
#define CZAS_PETLI_PIN		19	// RxD1 PD2

// wejścia do dekodowania pasm (sygnały z TRXa)
#define BAND0_PIN			29	// bit0 <-> A
#define BAND1_PIN			31	// bit1 <-> B
#define BAND2_PIN			30	// bit2 <-> C
#define BAND3_PIN			32	// bit3 <-> D

#define doPin_errLED      	47	// dioda wystąpienia jakiegoś błędu - aktywny stan wysoki (jest tranzystor na wyjściu)? -
								// jak jest błąd - stan wysoki i mruga; u mnie brak diody

/*
 * wejścia analogowe:
 */
#define FWD_PIN				A6		// forward na antenie 	J2->3
#define REF_PIN				A7		// odbita na antenie 	J2->2
#define TEMP1_PIN			A12		// tranzystor 1 -> termistor 1,8k
#define TEMP2_PIN			A13		// tranzystor 2 -> termistor 1,8k
#define TEMP3_PIN			A14		// temperatura radiatora -> termistor 1,8k
#define IDD_PIN				A15		// pomiar prądu PA	J26->2

#define thresholdTemperaturAirOn1   50
#define thresholdTemperaturTransistorMax	70		// temperatura tranzystora (z termistora nr 1), przy której PA jest blokowane - STBY
#ifdef ACS713
#define pa1AmperFactor (inputFactorVoltage * (30/4.0))    // 133mV/A -> 7.5A/V; ACS713 30A, Vout od 0.5V do 4.5V
#define pa1AmperOffset (1023/5 * 0.512)                     // 0.5V z czujnika Hallla dla Idd = 0A -> zmierzyć i wstawić
#else
#ifdef ACS712
#define pa1AmperFactor (inputFactorVoltage * (30/2.0))    // 66mV/A -> 15A/V; ACS712 30A
#define pa1AmperOffset (1023/5 * 2.487)                     // 2.5V z czujnika Hallla -> zmierzyć i wstawić
#else
#define pa1AmperFactor (inputFactorVoltage * (22.0/5.0))    // 3k Ris w BTS50085
#define pa1AmperOffset (0.0)                     // 0.0V	- pomiar z BTS50085 - od zera
#endif
#endif

// VGA color palette: https://www.rapidtables.com/web/color/RGB_Color.html
#define VGA_BLACK		0x000000
#define VGA_WHITE		0xFFFFFF
#define VGA_RED			0xFF0000

#define VGA_GREEN		0x008000
#define VGA_BLUE		0x0000FF
#define VGA_SILVER		0xC0C0C0
#define VGA_GRAY		0x808080
#define VGA_MAROON		0x800000
#define VGA_YELLOW		0xFFFF00

#define VGA_OLIVE		0x808000
#define VGA_LIME		0x00FF00
#define VGA_AQUA		0x00FFFF
#define VGA_TEAL		0x008080
#define VGA_NAVY		0x000080
#define VGA_FUCHSIA		0xFF00FF
#define VGA_PURPLE		0x800080
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
float calc_SWR(int forward, int ref);
int getTemperatura(uint8_t pin, int Rf);

// pomiar mocy wg ATU (na wzór sterownik_PA_500W)
int get_forward();
int get_reverse();
void get_pwr();
int correction(int input);
void switch_bands();
byte readDataPort();

#endif /* STEROWNIK_FT81X_H_ */

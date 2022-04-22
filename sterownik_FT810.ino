#include "sterownik_FT810.h"

#include <EEPROM.h>
#include <SPI.h>
#include <GD3.h>
#include <math.h>
//#include "FranklinGothic_assets.h"
/* schemat sterownika: PA_500W->PA_500W_3->sterownik->sterownik_FT810
 *
 * ToDo
 *  - brak wartości szczytowych
 * 		- blokada przełączania pasm podczas nadawania
 * 	- czekanie na puszczenie dotyku ??
 * 	- obsługa WY_ALARMU_PIN
 * 		- obsługa wyjścia WY_ALARMU_PIN -> powoduje odcięcie zasilania (np. przy alarmie)
 * 			- kiedy, przy którym alarmie wykonać i czy w ogóle?
 * 	- komunikat o przekroczeniu temp radiatora nie znika po dotknięciu
 * 		- komunikat error
 * 			- za krótki (za mała czcionka?)
 * 			- nie wyśrodkowany w pionie
 * ver 1.0.4
 * 	- możliwość ręcznego uruchomienia wentylatora
 * 	- przełączanie pasm na podstawie kodu DCBA z portu Band Data
 * 	- pomiar temperatury na termistorach
 * ver 1.0.3
 * 	- przełączanie pasm
 *	- pamięć mode i pasma: EEPROM
 * 	- obsługa ddotyku na przycisku OPR/TX/STBY
 *  - pomiar mocy i SWR
 * ver 1.0.2
 * ver 1.0.1
 * 	- obsługa wyjścia BLOKADA_PTT_PIN
 * ver. 1.0.0
 * 	- pomiar czasu pętli: 18,5ms
 * schemat
 * - bieżące
 * 	- alarmy związane z IDD
 *
 *
 */



// Define some colors
// Help under http://www.barth-dev.de/online/rgb565-color-picker/
// https://chrishewett.com/blog/true-rgb565-colour-picker/ !!!
//#define vgaBackgroundColor	0x10A2
long vgaBackgroundColor = 0x101410;	// 2,5,2
//#define vgaTitleUnitColor	0x632C
long vgaTitleUnitColor = 0x636563;	// 12,25,12
//#define vgaValueColor		0x94B2 565
long vgaValueColor = 0x949694;		// 18,37,18
//#define vgaBarColor			0xCE59
long vgaBarColor = 0xcecace;			// 25, 50, 25

String infoString = "";
String warningString = "";		// nieużywany
String errorString = "";
bool genOutputEnable = true;
bool isFanOn = false;

enum
{
	MANUAL = 0,
	AUTO
};
byte mode = MANUAL;
const char * modeTxt = "MANUALLY";

enum
{
	BAND_160 = 0,
	BAND_80,
	BAND_60,
	BAND_40,
	BAND_30,
	BAND_20,
	BAND_17,
	BAND_15,
	BAND_12,
	BAND_10,
	BAND_6,
	BAND_NUM
};
enum
{
	LPF1_PIN = 22,
	LPF2_PIN,
	LPF3_PIN,
	LPF4_PIN,
	LPF5_PIN,
	LPF6_PIN,
	LPF7_PIN
};
const char * BAND[BAND_NUM] = {"160m", "80m", "60m", "40m", "30m", "20m","17m", "15m","12m", "10m", "6m"};

byte current_band = BAND_80;
byte prev_band = BAND_NUM;
byte Band_PIN[BAND_NUM] = {LPF7_PIN, LPF6_PIN, LPF5_PIN, LPF5_PIN, LPF4_PIN, LPF4_PIN, LPF3_PIN, LPF3_PIN, LPF2_PIN, LPF2_PIN, LPF1_PIN};

byte AutoBandIdx = 15;
#define COLDSTART_REF	0x12   		// when started, the firmware examines this "Serial Number"
#define CZAS_REAKCJI	1000		// the time [ms] after which the writing into EEPROM takes place
boolean byla_zmiana = false;
unsigned long czas_zmiany;
boolean airBox1Manual = false;
unsigned long timeAtCycleStart, timeAtCycleEnd, timeStartMorseDownTime,
		actualCycleTime, timeToogle500ms = 0;
#define cycleTime        20		// kompromis pomiędzy szybką odpowiedzią linijki a skakaniem odczytów ToDo rozdzielić?
// ToDo nowy czas do czytania dotyku
bool toogle500ms;


/*
 * fonty:
 * SmallFont[];					// 8x12 -> 18 (8x16)
 * Grotesk16x32[];				// ? 22 (17x20)
 * GroteskBold16x32[];			// ? 23 (18x22) bold
 * GroteskBold32x64[];			// ? 31 (37x49)
 * nadianne[];					// 16x16 -> 21 (13x17) lub 22 (17x20)
 * franklingothic_normal[];		// 16x16 -> 21 (13x17) lub 22 (17x20)
 */
byte SmallFont = 18;
byte Grotesk16x32 = 22;		// font nr 22
//byte franklingothic_normal = FRANKLINGOTHIC_HANDLE;
byte GroteskBold16x32 = 24;
byte GroteskBold32x64 = 31;

char work_str[7];

float pa1AmperValue;
float swrValue;
float pwrForwardValue;
float pwrReturnValue;
int temperaturValueI1;
int temperaturValueI2;
int temperaturValueI3;
int forwardValue;		// odczyt napięcia padającego z direct couplera
int forwardValueAvg;	// średnia z napięcia padającego z direct couplera
int returnValue;		// odczyt napięcia odbitego z direct couplera
int returnValueAvg;		// średnia napięcia odbitego z direct couplera
int fwd_calc = 0;		// sumaryczny odczyt
int rev_calc = 0;
int p_curr = 0;			// licznik odczytów
float fwd_pwr;
float rev_pwr;
#define SWR_SAMPLES_CNT             1

int drawWidgetIndex = 1;

bool pttValue = false;

// zmienne powodujące przejście PA w tryb standby -> blokada nadawania (blokada PTT)
bool stbyValue = false;
bool ImaxValue;
bool PmaxValue;
bool SWRmaxValue;
bool SWR3Value;
bool SWRLPFmaxValue;
bool SWR_ster_max;
bool TemperaturaTranzystoraMaxValue;
bool TermostatValue;

const float inputFactorVoltage (5.0/1023.0);
#define pwrForwardFactor (inputFactorVoltage * (222.0/5.0))
#define pwrReturnFactor (inputFactorVoltage * (222.0/5.0))
/* pomiar temperatury na bazie termistora
* 		- termistory TEWA TTS-1.8KC7-BG 1,8kom (25C) beta = 3500
* 			- obliczenia: R = R25*exp[beta(1/T - 1/298,15)] T - temperatura
* 				- R = Rf*U/(Uref - U) gdzie U - napięcie na dzielnku z Rf i termistora zasilanego przez Uref (5V)
* 			- obliczenie temp T = 1/((ln(R/R25)/beta + 1/T25)) [K] ; T25 = 298,15
*/
float Vref = 4.994;			// napięcie odniesienia dla ADC ??
float Uref = 4.994;			// napięcie zasilające dzielnik pomiarowy temperatury
int beta = 3500;			// współczynnik beta termistora
int R25 = 1800;				// rezystancja termistora w temperaturze 25C
int Rf1 = 1794;				// rezystancja rezystora szeregowego z termistorem -> zmierzyć przed wlutowaniem
int Rf2 = 1798;				// rezystancja rezystora szeregowego z termistorem -> zmierzyć
int Rf3 = 1800;				// rezystancja rezystora szeregowego z termistorem -> zmierzyć


/* zmienne do pomiaru mocy i swr
 *
 */
int PWR, SWR;
byte K_Mult = 24;	// ilość zwojów w transformatorze direct couplera

union swaper
{
	byte bajt;
	struct
	{
		unsigned char b0 :1;
		unsigned char b1 :1;
		unsigned char b2 :1;
		unsigned char b3 :1;
		unsigned char b4 :1;
		unsigned char b5 :1;
		unsigned char b6 :1;
		unsigned char b7 :1;
	} bit;
};


/*
 * button czuły na dotyk bez tekstu i grafiki
 */
class PushButton
{
	int _x, _y;
	unsigned int _h, _w;
	byte _tag;
public:
	PushButton(int x, int y, unsigned int w, unsigned int h, byte tag)
	{
		_tag = tag;
		_x = x;
		_y = y;
		_h = h;
		_w = w;
	}
	void init()
	{
		GD.Tag(_tag);
		GD.Begin(RECTS);
		GD.ColorRGB(vgaBackgroundColor);
		GD.Vertex2ii(_x, _y);
		GD.Vertex2ii(_x + _w, _y + _h);
	}
	bool isTouchInside()
	{
		// Check if touch is inside this widget
		GD.get_inputs();
		return GD.inputs.tag == _tag;
	}
	bool isTouchInside(byte tag)
	{
		// Check if touch is inside this widget
		return tag == _tag;
	}
private:
};


class InfoBox
{
	// This class draws a info box with value or text, title and unit.
	// Class Variables
	char * _unit;
	char * _tytul;
	char * _text;

	float _value, _minValue, _maxValue;

	int _xPos, _yPos, _height, _width;
	int _xPadding, _yPadding;
	long _colorValue, _colorBack;
	byte _font;

	bool _raisedError = false;
	bool _drawLater = false;

	byte _tag;
public:
	InfoBox(const char * tytul, const char * unit, int xPos, int yPos, int height,
			int width, float minValue, float maxValue, long colorValue,
			long colorBack, byte font, byte tag)
	{
		// Store parameter
		_unit = unit;
		_tytul = tytul;
		_value = 0;
		_text = "";

		_xPos = xPos;
		_yPos = yPos;
		_height = height;
		_width = width;
		_minValue = minValue;
		_maxValue = maxValue;
		_xPadding = 2;
		_yPadding = 1;

		_colorValue = colorValue;
		_colorBack = colorBack;
		_font = font;
		_tag = tag;
	}

	void init()
	{
		// Called by main setup
		//myGLCD.setBackColor(_colorBack);
		//myGLCD.setColor(_colorBack);
		//myGLCD.fillRect(_xPos, _yPos, _xPos + _width, _yPos + _height);

		// Background
		GD.Tag(_tag);
		GD.Begin(RECTS);
		GD.ColorRGB(_colorBack);
		int translateX;
		if (_xPos > 511)
		{
			translateX = 300;
		}
		else
		{
			translateX = 0;
		}
		GD.VertexTranslateX(translateX*16);
		GD.Vertex2ii(_xPos - translateX, _yPos);
		if ((_xPos + _width) > 511)
		{
			translateX = 300;
		}
		else
		{
			translateX = 0;
		}
		GD.VertexTranslateX(translateX*16);
		GD.Vertex2ii(_xPos - translateX + _width, _yPos + _height);
		GD.ColorRGB(vgaTitleUnitColor);
		// Title
		int titleFontXsize = getFontXsize(SmallFont);	// SmallFont
		int titleLength = String(_tytul).length();
		GD.cmd_text(_xPos - translateX + (_width - titleLength * titleFontXsize) - _xPadding, _yPos + _yPadding + 1, SmallFont, 0, _tytul);
		// Unit
//		int unitFontXsize = getFontXsize(franklingothic_normal);
	//	int unitFontYsize = getFontYsize(franklingothic_normal);
		int unitFontXsize = getFontXsize(SmallFont);
		int unitFontYsize = getFontYsize(SmallFont);
		int unitLength = String(_unit).length();
		GD.VertexTranslateX(0*16);
		GD.cmd_text(_xPos + (_width - unitLength * unitFontXsize) - _xPadding, _yPos + _yPadding + unitFontYsize + 1, SmallFont, 0, _unit);

		//GD.cmd_text(_xPos - translateX + (_width - unitLength * unitFontXsize) - _xPadding, _yPos + _yPadding + unitFontYsize + 1, franklingothic_normal, 0, _unit);
		GD.VertexTranslateX(0*16);
	}

	void setColorValue(long color)
	{
		_colorValue = color;
	}

	void setColorBack(long color)
	{
		_colorBack = color;
	}

	bool isValueOk()
	{
		return not _raisedError;
	}

	void setFloat(float value, int dec, int length, bool show)
	{
		// dec ile po przecinku, lenght długość całkowita
		//if ((value != _value) or _drawLater) -> brak wyświetlania ...
		if (true)
		{
		    char work_str[7];
			_value = value;
			GD.ColorRGB(_colorBack);

			if (value < _minValue or value > _maxValue)
			{
				GD.ColorRGB(VGA_RED);
				if (_raisedError == false and errorString == "")
				{
					_raisedError = true;
					errorString = "Error: " + String(_tytul) + " " + _value + String(_unit)
							+ " outside range of " + int(_minValue) + _unit
							+ "-" + int(_maxValue) + _unit;
				}
			}
			else
			{
				_raisedError = false;
				GD.ColorRGB(_colorValue);
			}

			if (show)
			{
			    //indu = _value;
			    //indu_sub = (int) _value % 10;
			    //sprintf(work_str,"%2u.%01u", indu, indu_sub);
			    //sprintf(work_str, "%f", _value);
			    dtostrf(_value, 5, 1, work_str);
			    //String dane = String(_value, 1);
			    GD.cmd_text(_xPos + _xPadding, _yPos + _yPadding, _font, 0, work_str);
			    //GD.cmd_text(_xPos, _yPos, _font, OPT_CENTER, work_str);
			    //GD.cmd_number(_xPos + _xPadding, _yPos + _yPadding, _font, 0, _value);

				_drawLater = false;
			}
			else
			{
				_drawLater = true;
			}
		}
	}

	void setInt(int value, int length, bool show)
	{
		//if ((value != _value) or _drawLater)
		if (true)
		{
			_value = value;

			//myGLCD.setBackColor(_colorBack);
			GD.ColorRGB(_colorBack);

			if (value < _minValue or value > _maxValue)
			{
				GD.ColorRGB(VGA_RED);
				if (_raisedError == false and errorString == "")
				{
					_raisedError = true;
					errorString = "Error: " + String(_tytul) + " " + _value + String(_unit)
							+ " outside range of " + int(_minValue) + _unit
							+ "-" + int(_maxValue) + _unit;
				}
			}
			else
			{
				_raisedError = false;
				GD.ColorRGB(_colorValue);
			}

			if (show)
			{
				//myGLCD.setFont(_font);
				//myGLCD.printNumI(_value, _xPos + _xPadding, _yPos + _yPadding,
					//	length);
				int work_int = _value;
				char work_str[7];
				if (work_int >= 1000)
				{
					sprintf(work_str,"C=%4up", work_int);
				}
				else if (work_int >= 100)
				{
					sprintf(work_str,"C= %3up", work_int);
				}
				else if (work_int >= 10)
				{
					sprintf(work_str,"C=  %2up", work_int);
				}
				else
				{
					sprintf(work_str,"C=   %1up", work_int);
				}
				//GD.cmd_text(_xPos + _xPadding, _yPos + _yPadding, _font, 0, work_str);
				GD.cmd_number(_xPos + _xPadding, _yPos + _yPadding, _font, 0, _value);
				_drawLater = false;
			}
			else
			{
				_drawLater = true;
			}
		}
	}
	void setText(const char *text)
	{
		init();
		GD.ColorRGB(_colorValue);
		byte fontemp;
		if (String(text).length() < 46)
		{
			fontemp = _font;
		}
		else
		{
			fontemp = SmallFont;
		}
		GD.cmd_text(_xPos + _xPadding, _yPos + _yPadding, fontemp, 0, text);
	}

	float getValue()
	{
		return _value;
	}

	char getText()
	{
		return _text;
	}

	bool isTouchInside()
	{
		// Check if touch is inside this widget
		return GD.inputs.tag == _tag;
	}
	bool isTouchInside(byte tag)
	{
		return tag == _tag;
	}
};
class DisplayBar
{
	// This class draws a bar with scale, title, actual and maximum value.
	// Class Variables
	char * _title;
	char * _unit;

	float _value, _valueMin, _valueOld, _valueMax;
	float _minValue, _maxValue, _rangeValue;
	float _warnValue1, _warnValue2;
	float _level, _levelOld, _delta;

	int _xPos, _yPos;
	int _xPosBar, _yPosBar, _heightBar, _widthBar;
	int _height, _width;
	int _xPadding, _yPadding;
	int _colorBar, _colorBack, _font;
	int _noOffHelplines;

	// filter
	int _holdMaxCycles;
	int _filterForValueRefresh;
	float _deltaMaxNeg;
	bool _showMax;

	InfoBox *ptrActBox;
	InfoBox *ptrMaxBox;
	byte _tag;

public:
	DisplayBar(const char * title, const char * unit, int xPos, int yPos, int height,
			int width, float minValue, float maxValue, float warnValue1,
			float warnValue2, int colorBar, int colorBack, int noOffHelplines, byte tag)

	{
		// Store parameter
		_title = title;
		_unit = unit;

		_valueMin = minValue;
		_value = _valueMin;
		_valueOld = _value;
		_valueMax = 0;

		_minValue = minValue;
		_maxValue = maxValue;
		_rangeValue = _maxValue - _minValue;
		_warnValue1 = warnValue1;
		_warnValue2 = warnValue2;

		_xPos = xPos;
		_yPos = yPos;
		_height = height;
		_width = width;
		_xPadding = 4; // x padding inside the box
		_yPadding = 1; // y padding inside the box

		_colorBar = colorBar;
		_colorBack = colorBack;
		_font = 1;

		_noOffHelplines = noOffHelplines;

		// Filter
		_holdMaxCycles = 4;
		_deltaMaxNeg = maxValue / 100 * 4; // max decrement 4% of the max value
		_filterForValueRefresh = 0;
		_showMax = false;

		_tag = tag;

	}

	void init(bool nowe = false)
	{
		// Background
		GD.Tag(_tag);
		GD.Begin(RECTS);
		GD.ColorRGB(_colorBack);
		GD.Vertex2f(_xPos*16, _yPos*16);
		GD.Vertex2f((_xPos + _width)*16, (_yPos + _height)*16);

		// Title
		GD.ColorRGB(vgaTitleUnitColor);
		GD.cmd_text(_xPos + _xPadding, _yPos + _yPadding, SmallFont, 0, _title);

		// Info boxes
		int xPosInfoBox = _xPos + _width - _xPadding - 125;
		int yPosInfoBox = _yPos + _height - _yPadding - 64;
		_xPosBar = _xPos + _xPadding;
		_yPosBar = _yPos + (_height / 2);
		_heightBar = (_yPos + _height) - _yPosBar - 4;
		_widthBar = xPosInfoBox - _xPos - 2 * _xPadding;

		if (nowe)
		{
			//                               title    unit     xPos           yPos              height  width, minValue, maxValue,  colorValue       colorBack             font
			ptrActBox = new InfoBox("", _unit, xPosInfoBox, yPosInfoBox, 32, 125, 0, _maxValue, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 16);
			ptrMaxBox = new InfoBox("PEP", _unit, xPosInfoBox, yPosInfoBox + 32, 32, 125, 0, _maxValue, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 17);
		}

		ptrActBox->init();
		ptrMaxBox->init();

		//        xPos,     yPos,           height,  width
		drawScale(_xPosBar, _yPosBar - 18, 15, _widthBar);
		/* ToDo ??
		myGLCD.drawRect(_xPosBar, _yPosBar, _xPosBar + _widthBar,
				_yPosBar + _heightBar);
		_xPosBar = _xPosBar + 1;
		_yPosBar = _yPosBar + 1;
		_widthBar = _widthBar - 2;
		_heightBar = _heightBar - 2;

		myGLCD.setColor( VGA_BLACK);
		myGLCD.drawRect(_xPosBar, _yPosBar, _xPosBar + _widthBar,
				_yPosBar + _heightBar);
		_xPosBar = _xPosBar + 1;
		_yPosBar = _yPosBar + 1;
		_widthBar = _widthBar - 2;
		_heightBar = _heightBar - 2;
		*/
	}

	void drawScale(int xPos, int yPos, int height, int width)
	{
		// Draw the scale with value and warning levels
		//myGLCD.setColor(vgaValueColor);

		// Horizontal base line
		//myGLCD.fillRect(xPos, yPos + height - 2, xPos + width, yPos + height);
		fillRect(xPos, yPos + height - 2, xPos + width, yPos + height, vgaValueColor);
		// Draw warning level
		int warningLevel1 = (_warnValue1 - _minValue) / _rangeValue * _widthBar;
		int warningLevel2 = (_warnValue2 - _minValue) / _rangeValue * _widthBar;
		//myGLCD.setColor(VGA_YELLOW);
		fillRect(xPos + warningLevel1, yPos + height - 1,
				xPos + warningLevel2, yPos + height, VGA_YELLOW);
		//fillRect(xPos + warningLevel1, yPos + height - 1, )
		//myGLCD.setColor(VGA_RED);
		fillRect(xPos + warningLevel2, yPos + height - 1, xPos + width,
				yPos + height, VGA_RED);

		// Draw helplines and values
		//myGLCD.setColor(vgaValueColor);
		GD.ColorRGB(vgaValueColor);
		//myGLCD.setFont(SmallFont);

		float xPosHelpline;
		float helpValue;
		float helpValueIncrement = _rangeValue / _noOffHelplines;

		for (float helpline = 0; helpline <= _noOffHelplines; helpline++)
		{
			helpValue = _minValue + (helpline * helpValueIncrement);
			xPosHelpline = xPos + (helpline / _noOffHelplines * _widthBar);
			drawLine(xPosHelpline, yPos, xPosHelpline,
					yPos + height - 2);
			if (helpline != _noOffHelplines)
			{
				//if (helpValue <= 10 & helpValue > 0)
				if (helpValue <= 10 && helpValue > 0)
				{
					// Small values as float with 1 dec
					//myGLCD.printNumF(helpValue, 1, xPosHelpline + 3, yPos);
					//printNumI(helpValue, xPosHelpline + 3, yPos, SmallFont);
					printNumF(helpValue, 1, xPosHelpline + 3, yPos, SmallFont);
				}
				else
				{
					// Larg values as int
					printNumI(helpValue, xPosHelpline + 3, yPos, SmallFont);
				}
			}
		}
	}

	void setValue(float value, bool show)
	{
		// Set value and draw bar and info box
		// Refresh the info box only all 4 updates
		/*
		 * spowolnione zmniejszanie tylko dla wartości z zakresu poniżej _valueMax; "wyskoki" są brane bez zmian
		 * -> dodatkowy warunek value < _maxValue
		 */

		if (value < _valueOld and value < _maxValue)
		{
			_delta = _valueOld - value;
			if (_delta > _deltaMaxNeg)
			{
				value = _valueOld - _deltaMaxNeg;
			}
		}

		// Set the actual value info box
		if (value < 100)
		{
			ptrActBox->setFloat(value, 1, 4, show);
		}
		else
		{
			ptrActBox->setInt(value, 4, show);
		}

		// Update the bar
		_value = value;
		if (_showMax)
		{
			setValueMax(_value);
		}

		if (show)
		{
			_showMax = true;
		}
		else
		{
			_showMax = false;
		}

		_level = (value - _minValue) / _rangeValue * _widthBar;

		if (_level > _widthBar)
		{
			_level = _widthBar;
		}
		/*
		if (_level > _levelOld)
		{
			//myGLCD.setColor(_colorBar);
			//myGLCD.fillRect(_xPosBar + _levelOld, _yPosBar, _xPosBar + _level, _yPosBar + _heightBar);
			fillRect(_xPosBar + _levelOld, _yPosBar, _xPosBar + _level, _yPosBar + _heightBar, _colorBar);
		}
		else
		{
			//myGLCD.setColor(_colorBack);
			//myGLCD.fillRect(_xPosBar + _level, _yPosBar, _xPosBar + _levelOld,_yPosBar + _heightBar);
			fillRect(_xPosBar + _level, _yPosBar, _xPosBar + _levelOld,_yPosBar + _heightBar, _colorBack);
		}
		*/
		// za każdym razem malujemy całość
		fillRect(_xPosBar, _yPosBar, _xPosBar + _level, _yPosBar + _heightBar, _colorBar);
		fillRect(_xPosBar + _level, _yPosBar, _xPosBar + _widthBar, _yPosBar + _heightBar, _colorBack);

		_levelOld = _level;
		_valueOld = value;
	}

	void setValueMax(float value)
	{
		// Set the maximum value
		if (value > _valueMax)
		{
			_valueMax = value;

			// Set the maximum value info box
			if (value < 100)
			{
				ptrMaxBox->setFloat(value, 1, 4, true);
			}
			else
			{
				ptrMaxBox->setInt(value, 4, true);
			}
		}
	}

	void resetValueMax()
	{
		_valueMax = -1;
	}

	float getValue()
	{
		// Return the actual value
		return _value;
	}

	bool isTouchInside()
	{
		// Check if touch is inside this widget
		//return ((x > _xPos and x < _xPos + _width)
			//	and (y > _yPos and y < _yPos + _height));
		return GD.inputs.tag == _tag;
	}
};



PushButton Down(20, 20, 165, 72, 1);
PushButton Up(185, 20, 165, 72, 2);
//InfoBox bandBox("LPF", "m", 20, 120, 72, 330, 0, 0, vgaValueColor, vgaBackgroundColor, GroteskBold32x64, 3);
InfoBox modeBox("MODE", "", 395, 60, 32, 200, 4, 5, vgaValueColor, vgaBackgroundColor, Grotesk16x32, 4);

//InfoBox aux1VoltageBox("AUX R", "V", 170, 340, 32, 125, 11, 15, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 6);
// InfoBox aux2VoltageBox("AUX B", "V", 320, 340, 32, 125, 11, 14, vgaValueColor, vgaBackgroundColor, GroteskBold16x32);
//InfoBox airBox2("AIR2", "", 470, 380, 32, 125, 0, 0, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 8);

InfoBox pa1AmperBox("IDD", "A", 20, 340, 32, 125, 0, 20.0, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 9);
InfoBox temperaturBox1("Tranzyst1", "`C", 170, 340, 32, 125, 10, 65, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 10);
InfoBox temperaturBox3("Radiator", "`C", 320, 340, 32, 125, 10, 60, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 12);
InfoBox airBox1("AIR1", "", 470, 340, 32, 125, 2, 3, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 7);

//InfoBox pa2AmperBox("PA 2", "A", 170, 380, 32, 125, 0, 24.9, vgaValueColor, vgaBackgroundColor, GroteskBold16x32);

InfoBox temperaturBox2("Tranzyst2", "`C", 170, 380, 32, 125, 10, 60, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 11);
//InfoBox drainVoltageBox("DRAIN", "V", 20, 340, 32, 125, 48, 54, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 5);


InfoBox emptyBox("", "", 470, 380, 32, 125, 6.0, 7.0, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 13);

//InfoBox msgBox("", "", 20, 420, 32, 760, 0, 0, vgaValueColor, vgaBackgroundColor, Grotesk16x32, 14);
InfoBox msgBox("", "", 20, 420, 32, 760, 1, 2, vgaValueColor, vgaBackgroundColor, GroteskBold16x32, 14);
InfoBox txRxBox("", "", 645, 340, 72, 135, 8, 9, vgaValueColor, vgaBackgroundColor, GroteskBold32x64, 15);
//DisplayBar pwrBar("PWR", "W", 20, 126, 80, 760, 0, 1250, 375, 875, vgaBarColor, vgaBackgroundColor, 10, 18);	// zakres do 1,25 kW
//DisplayBar pwrBar("PWR", "W", 20, 126, 80, 760, 0, 750, 225, 525, vgaBarColor, vgaBackgroundColor, 10, 18);      // Wybor zakresu do 0,75kW
//DisplayBar pwrBar("PWR", "W", 20, 126, 80, 760, 0, 650, 195, 455, vgaBarColor, vgaBackgroundColor, 10, 18);      // Wybor zakresu do 0,65kW
DisplayBar pwrBar("PWR", "W", 20, 126, 80, 760, 0, 600, 240, 420, vgaBarColor, vgaBackgroundColor, 10, 18);      // Wybor zakresu do 0,5kW

DisplayBar swrBar("SWR", "", 20, 226, 80, 760, 1, 5, 3, 4, vgaBarColor, vgaBackgroundColor, 16, 19);

void setup()
{
	//analogReference(INTERNAL2V56);
#ifdef DEBUG
	Serial1.begin(115200);
	Serial1.println("setup");
#endif
	// pierwsze 24 komórki EEPROM są używane i zajęte przez wyświetlacz (dane kalibracji)
	if (EEPROM.read(30) != COLDSTART_REF)
	{
		EEPROM.write(31, current_band);
		EEPROM.write(32, mode);
		EEPROM.write(30, COLDSTART_REF); // COLDSTART_REF in first byte indicates all initialized
#ifdef DEBUG
		Serial1.println("writing initial values into memory");
#endif
	}
	else                       // EEPROM contains stored data, retrieve the data
	{
		// read the current band
		current_band = EEPROM.read(31);
		// read mode
		mode = EEPROM.read(32);
#ifdef DEBUG
		Serial1.println("reading current_band from memory: ");
		Serial1.println(current_band);
#endif
	}
	switch_bands();

	pinMode(18, OUTPUT);
	digitalWrite(18, LOW);

	pinMode(WY_ALARMU_PIN, OUTPUT);
	digitalWrite(WY_ALARMU_PIN, LOW);
	pinMode(ALARM_OD_IDD_PIN, INPUT);
	pinMode(WE_PTT_PIN, INPUT);
	pinMode(RESET_ALARMU_PIN, OUTPUT);
	digitalWrite(RESET_ALARMU_PIN, HIGH);
	pinMode(WE_PTT_PIN, INPUT);
	pinMode(BLOKADA_PTT_PIN, OUTPUT);
	digitalWrite(BLOKADA_PTT_PIN, LOW);
	pinMode(FAN_ON_PIN, OUTPUT);
	digitalWrite(FAN_ON_PIN, LOW);
	pinMode(FAN1_PIN, OUTPUT);
	digitalWrite(FAN1_PIN, LOW);
	pinMode(FAN2_PIN, OUTPUT);
	digitalWrite(FAN2_PIN, LOW);
	pinMode(FAN3_PIN, OUTPUT);
	digitalWrite(FAN3_PIN, LOW);

	pinMode(CZAS_PETLI_PIN, OUTPUT);
	pinMode(doPin_errLED, OUTPUT);
	digitalWrite(doPin_errLED, LOW);

	pinMode(LPF1_PIN, OUTPUT);
	pinMode(LPF2_PIN, OUTPUT);
	pinMode(LPF3_PIN, OUTPUT);
	pinMode(LPF4_PIN, OUTPUT);
	pinMode(LPF5_PIN, OUTPUT);
	pinMode(LPF6_PIN, OUTPUT);
	pinMode(LPF7_PIN, OUTPUT);

	pinMode(BAND0_PIN, INPUT);
	pinMode(BAND1_PIN, INPUT);
	pinMode(BAND2_PIN, INPUT);
	pinMode(BAND3_PIN, INPUT);

	pinMode(PDPin, OUTPUT);
	digitalWrite(PDPin, HIGH);
	delay(20);
	digitalWrite(PDPin, LOW);
	delay(20);
	digitalWrite(PDPin, HIGH);
	delay(20);

	GD.begin(1);
	GD.cmd_setrotate(0);
	//LOAD_ASSETS();
	GD.ClearColorRGB(0x103000);
	GD.Clear();
	GD.cmd_text(GD.w / 2, GD.h / 2, 31, OPT_CENTER, "Sterownik PA ver. 1.0.4");
	GD.swap();
	delay(500);

	GD.Clear();

	//temperaturBox3.init();

	//temperaturBox3.setFloat(temperaturValue3, 1, 5, false);
	//airBox1.init();
	//pa1AmperBox.init();
	pwrBar.init(true);
	swrBar.init(true);
}

void loop()
{
	byte inputsTag;
	timeAtCycleStart = millis();
	if ((timeAtCycleStart - timeToogle500ms) > 500)
	{
		toogle500ms = not toogle500ms;
		timeToogle500ms = timeAtCycleStart;
	}

	GD.Clear();
	Down.init();
	Up.init();
	// pasma:
	const char *tyt = "LPF";
	GD.ColorRGB(vgaTitleUnitColor);
	GD.cmd_text(20 + (330 - String(tyt).length() * getFontXsize(18)) - 4,
			20 + 1 + 1, 18, 0, tyt);
	GD.ColorRGB(vgaValueColor);
	GD.cmd_text(185, 56, GroteskBold32x64, OPT_CENTER, BAND[current_band]);

	if (mode == MANUAL)
	{
		modeTxt = "MANUALLY";
	}
	else
	{
		modeTxt = "AUTO";
	}
	modeBox.setText(modeTxt);

	//drainVoltageBox.init();
	//aux1VoltageBox.init();
	pa1AmperBox.init();
	temperaturBox1.init();
	temperaturBox2.init();
	temperaturBox3.init();

	msgBox.init();

	txRxBox.init();

	pwrBar.init();
	swrBar.init();

	//airBox1.init();
	//airBox1.setText("OFF");

	read_inputs();
#ifdef DEBUGi
	if (true)
	{
		Serial1.print("temp3: ");
		Serial1.println(temperaturValueI3);
		Serial1.print("power: ");
		Serial1.println(pwrForwardValue);
	}
#endif

	pwrBar.setValue(PWR, true);

	//swrValue = calc_SWR(forwardValue, returnValue);
	swrBar.setValue(SWR/100.0, true);

	temperaturBox1.setInt(temperaturValueI1, 3, true);
	temperaturBox2.setInt(temperaturValueI2, 3, true);
	temperaturBox3.setInt(temperaturValueI3, 3, true);
	pa1AmperBox.setFloat(pa1AmperValue, 1, 4, true);
	// temperatura1 i temperatura2 i wentylator1
	//if (temperaturValueI1 >= thresholdTemperaturAirOn1 or temperaturValueI2 >= thresholdTemperaturAirOn1)
	// temperaturValueI3 - temperatura radiatora
	if (temperaturValueI3 >= thresholdTemperaturAirOn1)
	{
		airBox1.setText("ON");
		//isFanOn = true;
		if (not isFanOn)
		{
			FanController(1);
		}
	}
	//else if ((temperaturValueI1 <= thresholdTemperaturAirOn1 - 3) and (temperaturValueI2 <= thresholdTemperaturAirOn1 - 3))
	else if (temperaturValueI3 <= thresholdTemperaturAirOn1 - 3)
	{
		if (airBox1Manual)
		{
			airBox1.setText("ON");
			if (not isFanOn)
			{
				FanController(1);
			}
		}
		else
		{
			airBox1.setText("OFF");
			if (isFanOn)
			{
				FanController(0);
			}
		}
	}
	else	// ToDo co to jest ????
	{
		if (isFanOn)
			airBox1.setText("ON");
		else
			airBox1.setText("OFF");
	}

	// ToDo dubel kontroli temperatury: jest już kontrola w InfoBox; ale co z kontrolą spadku temperatury?
	if (temperaturValueI1 > thresholdTemperaturTransistorMax or temperaturValueI2 > thresholdTemperaturTransistorMax)	// przekroczenie temperatury granicznej obudowy jednego z tranzystorów
	{
		TemperaturaTranzystoraMaxValue = true;
	}
	else if ((temperaturValueI1 < thresholdTemperaturTransistorMax - 5) and (temperaturValueI2 < thresholdTemperaturTransistorMax - 5))
	{
		TemperaturaTranzystoraMaxValue = false;
	}


	if (TemperaturaTranzystoraMaxValue == true)
	{
		errorString = "Error: Protector transistor temp max detected";
	}

	/*
	// Draw index defines the infoBox that can draw new values on the utft.
	// If all infoBoxes would draw together, the cycletime is to long and not constant for the morse output.
	if (drawWidgetIndex == 8)
	{
		drawWidgetIndex = 1;
	}
	else
	{
		drawWidgetIndex++;
	}
	*/
	// Monitor additional inputs and set errorString
	if (ImaxValue == true)
	{
		errorString = "Error: Protector Imax detected";
	}
	// analiza dotyku
	GD.get_inputs();
	inputsTag = GD.inputs.tag;
	if (inputsTag > 0 and inputsTag < 255)
	{
#ifdef DEBUG
		Serial1.println(inputsTag);
#endif
		if (mode == MANUAL)
		{
			// pasmo w dół
			if (Down.isTouchInside(inputsTag))
			{
				if (current_band == BAND_160)
				{
					current_band = BAND_NUM - 1;
				}
				else
				{
					current_band--;
				}
				byla_zmiana = true;
				czas_zmiany = millis();
				switch_bands();
#ifdef DEBUG
			Serial1.println("Down");
#endif
			}
			// pasmo w górę
			if (Up.isTouchInside(inputsTag))
			{
				if (current_band == BAND_6)
				{
					current_band = BAND_160;
				}
				else
				{
					current_band++;
				}
				byla_zmiana = true;
				czas_zmiany = millis();
				switch_bands();
#ifdef DEBUG
			Serial1.println("Up");
#endif
			}
		}
		// zmiana trybu zmiany pasma
		if (modeBox.isTouchInside(inputsTag))
		{
			if (mode == MANUAL)
			{
				mode = AUTO;
			}
			else
			{
				mode = MANUAL;
			}
			byla_zmiana = true;
			czas_zmiany = millis();
		}
		// przejście w tryb standby i powrót
		if (txRxBox.isTouchInside(inputsTag))
		{
			if (stbyValue)
			{
				stbyValue = false;
			}
			else
			{
				stbyValue = true;
			}
		}
		if (airBox1.isTouchInside(inputsTag))
		{
			if (airBox1Manual)
			{
				airBox1.setText("OFF");
				airBox1Manual = false;
			}
			else
			{
				airBox1.setText("ON");
				airBox1Manual = true;
			}
		}
		if (msgBox.isTouchInside(inputsTag))
		{
			if (msgBox.getText() == "")
			{
				infoString = "no more messages";
			}
			else
			{
				msgBox.setText("");
				errorString = "";
				infoString = "";
				// reset alarmu od IDD
				if (ImaxValue)
				{
					digitalWrite(RESET_ALARMU_PIN, LOW);
					delay(10);
					digitalWrite(RESET_ALARMU_PIN, HIGH);
					ImaxValue = false;
				}
			}
		}
	}
	//-----------------------------------------------------------------------------
	// Reset genOutputEnable on any errorString
	if (errorString != "")
	{
		genOutputEnable = false;
	}
	else
	{
		genOutputEnable = true;
	}
	//-----------------------------------------------------------------------------
	// Signal evaluation
	if (stbyValue or TemperaturaTranzystoraMaxValue or PmaxValue or SWRmaxValue or SWRLPFmaxValue or SWR_ster_max or ImaxValue or TermostatValue or not genOutputEnable)
	{
		txRxBox.setColorValue(vgaBackgroundColor);
		txRxBox.setColorBack(VGA_YELLOW);
		txRxBox.setText("STBY");
		digitalWrite(BLOKADA_PTT_PIN, HIGH);
		/* co to jest? o co tu chodzi?
		if (not stbyValue)
		{
			digitalWrite(BLOKADA_PTT_PIN, HIGH);
		}
		if (not (TemperaturaTranzystoraMaxValue or PmaxValue or SWRmaxValue or SWRLPFmaxValue or SWR_ster_max or ImaxValue or TermostatValue or not genOutputEnable))
		{
			digitalWrite(BLOKADA_PTT_PIN, LOW);
		}
		*/
	}
	else
	{
		if (pttValue and genOutputEnable)
		{
			txRxBox.setColorValue(vgaBackgroundColor);
			txRxBox.setColorBack(VGA_RED);
			txRxBox.setText(" TX");
		}
		else
		{
			txRxBox.setColorValue(vgaBackgroundColor);
			txRxBox.setColorBack(VGA_GREEN);
			txRxBox.setText("OPR");
		}
		digitalWrite(BLOKADA_PTT_PIN, LOW);
	}

	//-----------------------------------------------------------------------------
	// Write to outputs
	if (mode == AUTO)
	{
		// na razie przy każdym obiegi pętli; docelowo co np. 200ms
		current_band = readDataPort();
		switch_bands();
	}
	if ((ImaxValue or TermostatValue or PmaxValue or SWRmaxValue or SWRLPFmaxValue or SWR_ster_max or TemperaturaTranzystoraMaxValue or not genOutputEnable)  and toogle500ms)
	{
		digitalWrite(doPin_errLED, LOW);
	}
	else
	{
		digitalWrite(doPin_errLED, HIGH);
	}
	//-----------------------------------------------------------------------------
	// Display error messages
	if (errorString != "")
	{
		msgBox.setColorValue(VGA_RED);

		msgBox.setText(errorString.c_str());
	}
	else if (warningString != "")
	{
		msgBox.setColorValue(VGA_YELLOW);
		msgBox.setText(warningString.c_str());
	}
	else if (infoString != "")
	{
		msgBox.setColorValue(vgaValueColor);
		msgBox.setText(infoString.c_str());
	}


	GD.swap();

	if (byla_zmiana && (millis() - czas_zmiany > CZAS_REAKCJI))
	{
	    EEPROM.write(31, current_band);		// writing current band into eeprom
	    EEPROM.write(32, mode);			// zapis trybu (manual/auto)
		byla_zmiana = false;
#ifdef DEBUG
		Serial1.println("writing current settings to EEPROM: ");
		Serial1.print("current_band: ");
		Serial1.println(current_band);
		Serial1.print("mode: ");
		Serial1.println(mode);
#endif
	}

	//PORTD ^= (1<<PD3);		// D18 -> TxD1; do pomiaru czasu pętli podczas normalnej pracy
#ifdef CZAS_PETLI
	PORTD ^= (1<<PD2);		// nr portu na sztywno! = D19 -> RxD1; czas na razie 19,9ms
#else
	// Keep the cycle time constant
	timeAtCycleEnd = millis();
	actualCycleTime = timeAtCycleEnd - timeAtCycleStart;

	if (actualCycleTime < cycleTime)
	{
		delay(cycleTime - actualCycleTime);
	}
#endif
}

int getFontXsize(byte font)
{
	switch (font)
	{
		case 18:
			return 8;
			break;
		case 21:
			return 13;
			break;
		case 22:
			return 17;
			break;
		case 23:
			return 18;
			break;
		case 24:
			return 25;
			break;
		case 31:
			return 27;
			break;
		default:
			return 8;
			break;
	}
}
int getFontYsize(byte font)
{
	switch (font)
	{
		case 0:		// franklin gothic
			return 16;
			break;
		case 18:
			return 16;
			break;
		case 21:
			return 17;
			break;
		case 22:
			return 20;
			break;
		case 23:
			return 22;
			break;
		case 24:
			return 29;
		case 31:
			return 49;
			break;
		default:
			return 8;
			break;
	}
}
void fillRect(int x1, int y1, int x2, int y2, long color)
{
	GD.ColorRGB(color);
	GD.Begin(RECTS);
	GD.Vertex2f(x1*16, y1*16);
	GD.Vertex2f(x2*16, y2*16);
}
void drawLine(int x1, int y1, int x2, int y2)
{
	GD.Begin(LINES);
	GD.Vertex2f(x1*16, y1*16);
	GD.Vertex2f(x2*16, y2*16);
}
void printNumI(float value, int x, int y, byte font)
{
	GD.cmd_number(x, y, font, 0, value);
}
void printNumF(float value, byte dec, int x, int y, byte font)
{
	int jedno = (int) value;
	int sub;
    sub = ((int)(value*10))%10;
    sprintf(work_str,"%1u.%01u", jedno, sub);
    GD.cmd_text(x, y, font, 0, work_str);
}
float getTemperatura0(uint8_t pin)
{
	int t_in = analogRead(pin);
	float temp = Vref*100.0*t_in/1023;
#ifdef DEBUG
	Serial1.print("t_in: ");
	Serial1.println(t_in);
	Serial1.print("temp: ");
	Serial1.println(temp);
#endif
	return temp;
}
int getTempInt(uint8_t pin)
{
	int t_in = analogRead(pin);
	//unsigned int tt = 500*t_in;
	int temp = 500L*t_in/1023;
	return temp;
}
void read_inputs()
{
	//-----------------------------------------------------------------------------
	// Read all inputs
	//forwardValue = analogRead(FWD_PIN);
	//pwrForwardValue = sq(forwardValue * pwrForwardFactor) / 50;
	get_pwr();
	//pwrForwardValue = PWR;
	// returnValue = analogRead(REF_PIN);
	// pwrReturnValue = sq(returnValue * pwrReturnFactor) / 50;
#ifdef DEBUGi
	Serial1.print("forwardValue: ");
	Serial1.println(forwardValue);
	//Serial1.print("returnValue: ");
	//Serial1.println(returnValue);
#endif
	// temperaturValueI1 = getTempInt(TEMP1_PIN);
	temperaturValueI1 = getTemperatura(TEMP1_PIN, Rf1);		// z termistora na tranzystorze Q1
	//temperaturValueI2 = getTempInt(TEMP2_PIN);
	temperaturValueI2 = getTemperatura(TEMP2_PIN, Rf2);	// temperatura tranzystora Q2
	temperaturValueI3 = getTemperatura(TEMP3_PIN, Rf3);	// temperatura radiatora
	pttValue = not digitalRead(WE_PTT_PIN);					// aktywny stan niski
	pa1AmperValue = (analogRead(IDD_PIN) - pa1AmperOffset)*pa1AmperFactor;
	if (pa1AmperValue < 0)
		pa1AmperValue = 0;
	/*
	 * informacja o przekroczeniu IDD jest brana z zabezpieczenia prądowego -> gdy zadziała komparator i ustawi przerzutnik:
	 * aktywny stan wysoki
	 */
	ImaxValue = digitalRead(ALARM_OD_IDD_PIN);

	/*
	forwardValue = analogRead(aiPin_pwrForward);
	pwrForwardValue = sq(forwardValue * pwrForwardFactor) / 50;
	returnValue = analogRead(aiPin_pwrReturn);
	pwrReturnValue = sq(returnValue * pwrReturnFactor) / 50;
	drainVoltageValue = analogRead(aiPin_drainVoltage) * drainVoltageFactor;
	aux1VoltageValue = analogRead(aiPin_aux1Voltage) * aux1VoltageFactor;
	pa1AmperValue = (analogRead(aiPin_pa1Amper) - pa1AmperOffset)*pa1AmperFactor;
	if (pa1AmperValue < 0)
		pa1AmperValue = 0;
	temperaturValue1 = getTemperatura(aiPin_temperatura1, Rf1);
	temperaturValue2 = getTemperatura(aiPin_temperatura2, Rf2);
	temperaturValue3 = getTemperatura(aiPin_temperatura3, Rf3);

	ImaxValue = not digitalRead(diPin_Imax);				// aktywny stan niski
	PmaxValue = not digitalRead(diPin_Pmax);				// aktywny stan niski
	SWRmaxValue = not (digitalRead(diPin_SWRmax) or digitalRead(diPin_blok_Alarm_SWR));			// aktywny stan niski (dla diPin_SWRmax)
	SWRLPFmaxValue = not digitalRead(diPin_SWR_LPF_max) and digitalRead(diPin_blok_Alarm_SWR);	// aktywny stan niski (dla diPin_SWR_LPF_max)
	SWR_ster_max = not digitalRead(diPin_SWR_ster_max);		// aktywny stan niski
	TermostatValue = not digitalRead(diPin_Termostat);		// aktywny stan niski
	*/
}
void FanController(byte co)
{
	switch (co)
	{
		case 0:
			digitalWrite(FAN_ON_PIN, LOW);
			isFanOn = false;
			break;
		case 1:
			digitalWrite(FAN1_PIN, HIGH);
			digitalWrite(FAN2_PIN, LOW);
			digitalWrite(FAN3_PIN, LOW);
			digitalWrite(FAN_ON_PIN, HIGH);
			isFanOn = true;
			break;
		case 2:
			digitalWrite(FAN1_PIN, LOW);
			digitalWrite(FAN2_PIN, HIGH);
			digitalWrite(FAN3_PIN, LOW);
			digitalWrite(FAN_ON_PIN, HIGH);
			isFanOn = true;
			break;
		case 3:
			digitalWrite(FAN1_PIN, LOW);
			digitalWrite(FAN2_PIN, LOW);
			digitalWrite(FAN3_PIN, HIGH);
			digitalWrite(FAN_ON_PIN, HIGH);
			isFanOn = true;
			break;
		default:
			break;
	}
}
float calc_SWR(int forward, int ref)
{
#define MAX_SWR	9.9
	float swr;
	if (forward > 0)
	{
		if (forward <= ref)
		{
			swr = MAX_SWR;
		}
		else
		{
			swr = (float)(forward + ref)/(forward - ref);
			if (swr > MAX_SWR)
			{
				swr = MAX_SWR;
			}
		}
	}
	else
	{
		swr = 1;
	}
	return swr;
}
int getTemperatura(uint8_t pin, int Rf)
{
	int T;
	float R = 0.0;
	int u = analogRead(pin);
	float U = Vref*u/1023;
	R = Rf*U/(Uref - U);
	T = (int)(1/(log(R/R25)/beta + 1/298.15) - 273.15 + 0.5);
#ifdef DEBUGi
	Serial1.print("analogRead: ");
	Serial1.println(u);
	Serial1.print("U: ");
	Serial1.println(U);
	Serial1.print("R: ");
	Serial1.println(R);
	Serial1.print("T: ");
	Serial1.println(T);
#endif
	return T;
}
int get_forward()
{
    int forward;
    forward = analogRead(FWD_PIN);
    return forward * 4.883; // zwraca napięcie w mV
}
int get_reverse()
{
	int reverse;
	reverse = analogRead(REF_PIN);
	return reverse*4.883; // zwraca napięcie w mV
}
void get_pwr()
{
    long Forward, Reverse;
    float p;
    //
    Forward = get_forward();
    Reverse = get_reverse();
#ifdef DEBUG
    if (Forward > 0)
    {
    Serial.print("Forward: ");
    Serial.println(Forward);
    }
    if (Reverse > 0)
    {
    Serial.print("Reverse: ");
    Serial.println(Reverse);
    }
#endif
    p = correction(Forward * 3);
#ifdef DEBUGi
    if (p > 0)
    {
    Serial.print("p: ");
    Serial.println(p);
    }
#endif

    if (Reverse >= Forward)
        Forward = 999;
    else
    {
        Forward = ((Forward + Reverse) * 100) / (Forward - Reverse);
        Serial.print("Forward2: ");
        Serial.println(Forward);

        if (Forward > 999)
            Forward = 999;
    }
    // odtąd Forward to jest wyliczony lub ustalony SWR!
    //
    p = p * K_Mult / 1000.0; // mV to Volts on Input
    p = p / 1.414;
    p = p * p / 50; // 0 - 1500 ( 1500 Watts)
    p = p + 0.5; // rounding to 0.1 W
    //
    PWR = p;
#ifdef DEBUGi
    if (PWR > 0)
    {
    Serial.print("PWR: ");
    Serial.println(PWR);
    }
#endif
    if (PWR < 5)
        SWR = 1;
    else if (Forward < 100)
        SWR = 999;
    else
        SWR = Forward;
#ifdef DEBUG
    if (PWR > 50)
    {
        Serial.print("SWR: ");
        Serial.println(SWR);
    }
#endif
    return;
}
int correction(int input)
{
    if (input <= 80)
        return 0;
    if (input <= 171)
        input += 244;
    else if (input <= 328)
        input += 254;
    else if (input <= 582)
        input += 280;
    else if (input <= 820)
        input += 297;
    else if (input <= 1100)
        input += 310;
    else if (input <= 2181)
        input += 430;
    else if (input <= 3322)
        input += 484;
    else if (input <= 4623)
        input += 530;
    else if (input <= 5862)
        input += 648;
    else if (input <= 7146)
        input += 743;
    else if (input <= 8502)
        input += 800;
    else if (input <= 10500)
        input += 840;
    else
        input += 860;
    //
    return input;
}
void switch_bands()
{
#ifdef D_BAND
	Serial.print("prev_band: ");
	Serial.println(pasma[prev_band]);
	Serial.print("current_band: ");
	Serial.println(pasma[current_band]);
#endif
	if (Band_PIN[current_band] != Band_PIN[prev_band])
	{
		if (prev_band != BAND_160)
		{
			digitalWrite(Band_PIN[prev_band], LOW);
		}
		if (current_band != BAND_160)
		{
			digitalWrite(Band_PIN[current_band], HIGH);
		}
	}
	prev_band = current_band;
}
byte readDataPort()
{
    /*
     *
    DataPort Codes
    Band    Code
            DCBA
    160m    0001
    80m     0010    // 3,8 MHz
    40m     0011
    30m     0100
    20m     0101
    17m     0110
    15m     0111
    12m     1000
    10m     1001
    6m      1010
    60m     1011

    80m     1100    // 3,5MHz na pasmo 80m dwa kody -> dla skrzynki antenowej -> dwa ustawienia

    D -> BAND3
    C -> BAND2
    B -> BAND1
    A -> BAND0
     */
    swaper band;
    byte bandCode;
    byte kod;
    band.bit.b0 = digitalRead(BAND0_PIN);
    band.bit.b1 = digitalRead(BAND1_PIN);
    band.bit.b2 = digitalRead(BAND2_PIN);
    band.bit.b3 = digitalRead(BAND3_PIN);
    bandCode = (~band.bajt) & 0x0F;
    switch (bandCode)
    {
		case 1:
			kod = BAND_160;
			break;
		case 2:
			kod = BAND_80;
			break;
		case 3:
			kod = BAND_40;
			break;
		case 4:
			kod = BAND_30;
			break;
		case 5:
			kod = BAND_20;
			break;
		case 6:
			kod = BAND_17;
			break;
		case 7:
			kod = BAND_15;
			break;
		case 8:
			kod = BAND_12;
			break;
		case 9:
			kod = BAND_10;
			break;
		case 10:
			kod = BAND_6;
			break;
		case 11:
			kod = BAND_60;
			break;
		case 12:
			kod = BAND_80;
			break;
		default:
			kod = BAND_80;
			break;
	}
    return kod;
}

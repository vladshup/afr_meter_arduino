#include <stdint.h>
#include "Arduino.h"
#include "Wire.h"

#ifndef XTAL_FREQ
#define XTAL_FREQ	27000000			// Crystal frequency
#endif

#ifndef SI5351_H_INCLUDED
#define SI5351_H_INCLUDED

#define SI5351a_CLK0_CONTROL	16			// Register definitions
#define SI5351a_CLK1_CONTROL	17
#define SI5351a_CLK2_CONTROL	18
#define SI5351a_SYNTH_PLL_A	    26
#define SI5351a_SYNTH_PLL_B  	34
#define SI5351a_SYNTH_MS_0		42
#define SI5351a_SYNTH_MS_1		50
#define SI5351a_SYNTH_MS_2		58
#define SI5351a_PLL_RESET		177
#define SI5351a_PLL_LOADCAP		183
#define SI5351a_FANOUT		187

#define SI5351a_R_DIV_1		0x00			// R-division ratio definitions
#define SI5351a_R_DIV_2		0x10
#define SI5351a_R_DIV_4		0x20
#define SI5351a_R_DIV_8		0x30
#define SI5351a_R_DIV_16	0x40
#define SI5351a_R_DIV_32	0x50
#define SI5351a_R_DIV_64	0x60
#define SI5351a_R_DIV_128	0x70

#define SI5351a_CLK_SRC_PLL_A	0x00
#define SI5351a_CLK_SRC_PLL_B	0x20

#define SI53xx_I2C_WRITE 0x60		// I2C address for writing to the Si5351A

static void
si535x_SendRegister(uint8_t reg, uint8_t data)
{
	Wire.beginTransmission(SI53xx_I2C_WRITE);
	Wire.write(reg);
	Wire.write(data);
	//i2c_waitsend();
	Wire.endTransmission();
}
//
// Set up specified PLL with mult, num and denom
// mult is 15..90
// num is 0..1,048,575 (0xFFFFF)
// denom is 0..1,048,575 (0xFFFFF)
//
static void
si535x_setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
	const float d = (float) num / (float) denom;
	const uint32_t Pz = (uint32_t) (128 * (d));

	const uint32_t P1 = (uint32_t) (128 * (uint32_t) mult + Pz - 512);
	const uint32_t P2 = (uint32_t) (128 * num - denom * Pz);
	const uint32_t P3 = denom;

	// Write Operation - Burst (Auto Address Increment)
	Wire.beginTransmission(SI53xx_I2C_WRITE);
	Wire.write(pll);

	Wire.write((P3 & 0x0000FF00) >> 8);
	Wire.write((P3 & 0x000000FF));
	Wire.write((P1 & 0x00030000) >> 16);
	Wire.write((P1 & 0x0000FF00) >> 8);
	Wire.write((P1 & 0x000000FF));
	Wire.write(((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
	Wire.write((P2 & 0x0000FF00) >> 8);
	Wire.write((P2 & 0x000000FF));

	//i2c_waitsend();
	Wire.endTransmission();
}

//
// Set up MultiSynth with integer divider and R divider
// R divider is the bit value which is OR'ed onto the appropriate register, it is a #define in si5351a.h
//
static void
si535x_setupMultisynth(uint8_t synth, uint32_t divider, uint8_t rDiv)
{
	const uint32_t P1 = 128 * divider - 512;	// 18-bit number is an encoded representation of the integer part of the Multi-SynthX divider
	const uint32_t P2 = 0;						// P2 = 0, P3 = 1 forces an integer value for the divider
	const uint32_t P3 = 1;

	// Write Operation - Burst (Auto Address Increment)
	Wire.beginTransmission(SI53xx_I2C_WRITE);
	Wire.write(synth);

	Wire.write((P3 & 0x0000FF00) >> 8);				// MSx_P3[15:8]
	Wire.write((P3 & 0x000000FF));					// MSx_P3[7:0]
	Wire.write(((P1 & 0x00030000) >> 16) | rDiv | (divider == 4 ? 0x0c : 0x00));	// Rx_DIV[2:0], MSx_DIVBY4[1:0], MSx_P1[17:16]
	Wire.write((P1 & 0x0000FF00) >> 8);				// MSx_P1[15:8]
	Wire.write((P1 & 0x000000FF));					// MSx_P1[7:0]
	Wire.write(((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));	// MSx_P3[19:16], MSx_P2[19:16]
	Wire.write((P2 & 0x0000FF00) >> 8);				// MSx_P2[15:8]
	Wire.write((P2 & 0x000000FF));					// MSx_P2[7:0]

	//i2c_waitsend();
	Wire.endTransmission();
}

struct FREQ {
  uint8_t plldiv;	// должно быть чётное число
  uint8_t outdiv;	// Rx Output Divider code (SI5351a_R_DIV_1..SI5351a_R_DIV_128)
  uint16_t divider;	// общий делитель
  uint32_t fmin;
  uint32_t fmax;
};

static const uint8_t pllbase [3] =
{
	SI5351a_SYNTH_PLL_A,
	SI5351a_SYNTH_PLL_B,
	SI5351a_SYNTH_PLL_A,
};

static const uint8_t multisynchbase [3] =
{
	SI5351a_SYNTH_MS_0,
	SI5351a_SYNTH_MS_1,
	SI5351a_SYNTH_MS_2,
};

static uint8_t si5351aSetFrequencyX(uint8_t clkout, uint32_t frequency)
{
	uint32_t pllFreq;
	const uint32_t xtalFreq = XTAL_FREQ;
	uint32_t l;
	uint64_t f;
	uint8_t mult;
	uint32_t num;
	uint32_t denom;
	uint32_t divider;
	uint32_t r_div;

if (frequency > 222000000) {frequency = 222000000;}//работает до 222MHz
//Set R-divider
if (frequency > 1000000){r_div = SI5351a_R_DIV_1;}
else {
	if ( frequency <=1000000 && frequency > 500000) {r_div = SI5351a_R_DIV_2; frequency = frequency*2;}
	if ( frequency <=500000 && frequency > 250000) {r_div = SI5351a_R_DIV_4; frequency = frequency*4;}
	if ( frequency <=250000 && frequency > 125000) {r_div = SI5351a_R_DIV_8; frequency = frequency*8;}
	if ( frequency <=125000 && frequency > 62500) {r_div = SI5351a_R_DIV_16; frequency = frequency*16;}
	if ( frequency <=62500 && frequency > 31250) {r_div = SI5351a_R_DIV_32; frequency = frequency*32;}
	if ( frequency <=31250 && frequency > 15625) {r_div = SI5351a_R_DIV_64; frequency = frequency*64;}
	if ( frequency <=15625 && frequency > 8000) {r_div = SI5351a_R_DIV_128; frequency = frequency*128;}
}



	divider = 900000000uL / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal, PLL frequency: 900MHz
	if (divider % 2)
		divider -= 1;		// Ensure an even integer division ratio

	pllFreq = divider * frequency;	// Calculate the pllFrequency: the divider * desired output frequency
if (frequency > 150000000){divider = 4; pllFreq = 4 * frequency;}
	denom = 0x000FFFFF;				// For simplicity we set the denominator to the maximum 0x000FFFFF
	mult = pllFreq / xtalFreq;		// Determine the multiplier to get to the required pllFrequency
	l = pllFreq % xtalFreq;			// It has three parts:
	f = l;							// mult is an integer that must be in the range 15..90
	f *= (uint64_t) denom;					// num and denom are the fractional parts, the numerator and denominator
	f /= xtalFreq;					// each is 20 bits (range 0..0x000FFFFF)
	num = f;						// the actual multiplier is  mult + num / denom

									// Set up PLL B with the calculated multiplication ratio
	si535x_setupPLL(pllbase [clkout], mult, num, denom);

	// Set up MultiSynth divider 1, with the calculated divider.
	// The final R division stage can divide by a power of two, from 1..128.
	// reprented by constants SI5351a_R_DIV1 to SI5351a_R_DIV128 (see si5351a.h header file)
	// If you want to output frequencies below 1MHz, you have to use the
	// final R division stage
	si535x_setupMultisynth(multisynchbase [clkout], divider, r_div);
	return mult;
}

//
// Set CLK0 output ON and to the specified frequency
// Frequency is in the range 1MHz to 150MHz
// Example: si5351aSetFrequency(10000000);
// will set output CLK0 to 10MHz
//
// This example sets up PLL A
// and MultiSynth 0
// and produces the output on CLK0
//
static void si5351aSetFrequencyA(uint32_t frequency)
{
	static uint8_t skipreset;
	static uint8_t oldmult;

	if (0 == frequency)
	{
		si535x_SendRegister(SI5351a_CLK0_CONTROL, 0x80 | 0x5F | SI5351a_CLK_SRC_PLL_A);

		skipreset = 0;	// запрос на переинициализацию выхода
		return;
	}

	const uint8_t mult = si5351aSetFrequencyX(0, frequency);
//si535x_setupMultisynth(multisynchbase [2], 4, 0);
si535x_SendRegister(SI5351a_CLK2_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_A);

	if (skipreset == 0 || mult != oldmult)
	{
		si535x_SendRegister(SI5351a_PLL_RESET, 0x20);	// PLL A reset
		// Finally switch on the CLK1 output (0x4F)
		// and set the MultiSynth0 input to be PLL B
		si535x_SendRegister(SI5351a_CLK0_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_A);
		//si535x_setupMultisynth(multisynchbase [2], 4, 0);
		si535x_SendRegister(SI5351a_CLK2_CONTROL, 0x4F);

		skipreset = 1;
		oldmult = mult;
	}
}

static void si5351aSetFrequencyB(uint32_t frequency)
{
	static uint8_t skipreset;
	static uint8_t oldmult;


	if (0 == frequency)
	{
		si535x_SendRegister(SI5351a_CLK1_CONTROL, 0x80 | 0x4F | SI5351a_CLK_SRC_PLL_B);

		skipreset = 0;	// запрос на переинициализацию выхода
		return;
	}

	const uint8_t mult = si5351aSetFrequencyX(1, frequency);


	// Reset the PLL. This causes a glitch in the output. For small changes to
	// the parameters, you don't need to reset the PLL, and there is no glitch
	if (skipreset == 0 || mult != oldmult)
	{
		si535x_SendRegister(SI5351a_PLL_RESET, 0x80);	// PLL B reset
		// Finally switch on the CLK1 output (0x4F)
		// and set the MultiSynth0 input to be PLL B
		si535x_SendRegister(SI5351a_CLK1_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_B);


		skipreset = 1;
		oldmult = mult;
	}

}


static void si5351aSetFrequencyC(uint32_t frequency)
{
	static uint8_t skipreset;
	static uint8_t oldmult;


	if (0 == frequency)
	{
		si535x_SendRegister(SI5351a_CLK2_CONTROL, 0x80 | 0x5F | SI5351a_CLK_SRC_PLL_A); //INV 180 deg

		skipreset = 0;	// запрос на переинициализацию выхода
		return;
	}

	const uint8_t mult = si5351aSetFrequencyX(2, frequency);


	// Reset the PLL. This causes a glitch in the output. For small changes to
	// the parameters, you don't need to reset the PLL, and there is no glitch
	if (skipreset == 0 || mult != oldmult)
	{
		si535x_SendRegister(SI5351a_PLL_RESET, 0x20);	// PLL A reset
		// Finally switch on the CLK1 output (0x4F)
		// and set the MultiSynth0 input to be PLL B
		si535x_SendRegister(SI5351a_CLK2_CONTROL, 0x5F | SI5351a_CLK_SRC_PLL_A);


		skipreset = 1;
		oldmult = mult;
	}

}

static void si5351_Init(void)
{
	Wire.begin();
	Wire.setClock(400000L);
	si535x_SendRegister(SI5351a_FANOUT, 0xC0);
	si535x_SendRegister(SI5351a_PLL_LOADCAP, 0xC0 | 0x12);
	si535x_SendRegister(SI5351a_PLL_RESET, 0x20);	// PLL A reset
	// Finally switch on the CLK0 output (0x4F)
	// and set the MultiSynth0 input to be PLL A
	si535x_SendRegister(SI5351a_CLK0_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_A);


	si535x_SendRegister(SI5351a_CLK2_CONTROL, 0x5F | SI5351a_CLK_SRC_PLL_A); // INV180deg


	si535x_SendRegister(SI5351a_PLL_RESET, 0x80);	// PLL B reset
	// Finally switch on the CLK1 output (0x4F)
	// and set the MultiSynth0 input to be PLL B
	si535x_SendRegister(SI5351a_CLK1_CONTROL, 0x4F | SI5351a_CLK_SRC_PLL_B);

}


#endif /* SI5351a_C_INCLUDED */

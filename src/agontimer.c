/*
 * Title:			AGON timer interface
 * Author:			Jeroen Venema
 * Created:			06/11/2022
 * Last Updated:	12/04/2025
 * 
 * Modinfo:
 * 06/11/2022:		Initial version
 * 12/04/2025:      Updated for agondev
 */

#include "ez80f92.h"
#include <stdint.h>

#define TMR0_COUNTER_1ms	(unsigned short)(((18432000 / 1000) * 1) / 16)

void delayms(int ms)
{
	unsigned short rr;
	uint16_t timer0;

	rr = TMR0_COUNTER_1ms - 19;	// 1,7% correction for cycles during while(ms) loop
	
	IO(TMR0_CTL) = 0x00;	// disable timer0
	
	while(ms)
	{	
		IO(TMR0_RR_H) = (unsigned char)(rr >> 8);
		IO(TMR0_RR_L) = (unsigned char)(rr);
				
		IO(TMR0_CTL) = 0x87; // enable, single pass, stop at 0, start countdown immediately
		do
		{
			timer0 = IO(TMR0_DR_L);
			timer0 |= (IO(TMR0_DR_H) << 8);		
		}
		while(timer0);
		ms--;
	}
}

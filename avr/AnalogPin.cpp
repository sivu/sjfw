/*
 * Copyright 2010 by Adam Mayer	 <adam@makerbot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "AnalogPin.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#if ((defined __AVR_ATmega2560__) || (defined __AVR_ATmega1280__))
#define USE_MOAR_ANALOG
#endif

// Address to write the sampled data to
volatile int16_t* adc_destination;

// Flag to set once the data is sampled
volatile bool* adc_finished;

// We are using the AVcc as our reference.  There's a 100nF cap
// to ground on the AREF pin.
const uint8_t ANALOG_REF = 0x01;


void initAnalogPin(uint8_t pin) {
	// Only analog pins 0-5 need to be initialized, 6 and 7 are dedicated purpose.
	if (pin < 8) {
		DDRC &= ~(_BV(pin));
		PORTC &= ~(_BV(pin));
	}
#ifdef USE_MOAR_ANALOG  
  else
  {
    DDRK &= ~(_BV(pin));
		PORTK &= ~(_BV(pin));
  }
#endif  

	// enable a2d conversions, interrupt on completion
	ADCSRA |= _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0) |
			_BV(ADEN) | _BV(ADIE);
}

bool startAnalogRead(uint8_t pin, volatile int16_t* destination, volatile bool* finished) {
	// ADSC is cleared when the conversion finishes.
	// We should not start a new read while an existing one is in progress.
	if ((ADCSRA & _BV(ADSC)) != 0) {
		return false;
	}
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		adc_destination = destination;
		adc_finished = finished;
		*adc_finished = false;

		// set the analog reference (high two bits of ADMUX) and select the
		// channel (low 4 bits).  this also sets ADLAR (left-adjust result)
		// to 0 (the default).
		ADMUX = (ANALOG_REF << 6) | (pin & 0b00000111);

#ifdef USE_MOAR_ANALOG  
    if(pin > 7)
      ADCSRB |= _BV(MUX5);
    else
      ADCSRB &= ~(_BV(MUX5));
#endif      
      
		// start the conversion.
		ADCSRA |= _BV(ADSC);
	}
	// An interrupt will signal conversion completion.
	return true;
}

ISR(ADC_vect)
{
	uint8_t low_byte, high_byte;
	// we have to read ADCL first; doing so locks both ADCL
	// and ADCH until ADCH is read.  reading ADCL second would
	// cause the results of each conversion to be discarded,
	// as ADCL and ADCH would be locked when it completed.
	low_byte = ADCL;
	high_byte = ADCH;

	// combine the two bytes
	*adc_destination = (high_byte << 8) | low_byte;
	*adc_finished = true;
}


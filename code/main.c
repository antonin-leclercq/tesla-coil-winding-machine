/*
 * tesla_spueler.c
 *
 * Created: 28/06/2024 16:40:25
 * Author : LECLERCQ Antonin
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

// outputs
#define DRIVER_IN1 PORTA0
#define DRIVER_IN2 PORTA1
#define DRIVER_IN3 PORTA2
#define DRIVER_IN4 PORTA3
#define SPEED_LED PORTB0

// inputs
#define DIRECTION  PINB1
#define SPEED_POT  PINA7

uint8_t driver_in1 = DRIVER_IN1;
uint8_t driver_in2 = DRIVER_IN2;

uint16_t on_time_us = 0xFFFF;

void GPIO_Config(void)
{
	// Driver pins as outputs
	DDRA |= (1 << DRIVER_IN1) | (1 << DRIVER_IN2) | (1 << DRIVER_IN3) | (1 << DRIVER_IN4);
	PORTA &= ~((1 << DRIVER_IN1) | (1 << DRIVER_IN2) | (1 << DRIVER_IN3) | (1 << DRIVER_IN4));
	
	// speed indicator LED as outputs
	DDRB |= (1 << SPEED_LED);
	 
	// direction and speed control as inputs
	DDRB &= ~((1 << SPEED_POT) | (1 <<  DIRECTION));
}

void ADC_Config(void)
{
	// Use Vcc as reference voltage
	ADMUX &= ~((1 << REFS0) | (1 << REFS1));
	
	// Select channel (PINA7 - ADC7)
	ADMUX |= (0x7 << MUX0);
	
	// Free running mode
	ADCSRB &= ~((1 << ADTS0) | (1 << ADTS1) | (1 << ADTS2));
	
	// Set prescaler to /8
	ADCSRA |= (1 << ADPS1) | (1 << ADPS0);
	ADCSRA &= ~(1 << ADPS2);
	
	// Disable digital buffer to reduce power consumption
	DIDR0 |= (1 << ADC7D);
	
	// Enable ADC
	ADCSRA |= (1 << ADEN) | (1 << ADATE);
	
	// Start conversion
	ADCSRA |= (1 << ADSC);
}

void TIMER_Config(void)
{
	// No outputting on a GPIO (no PWM)
	TCCR1A &= ~((1 << COM1A0) | (1 << COM1A1) | (1 << COM1B0) | (1 << COM1B1));
	
	// Select CTC mode (clear timer on compare)
	TCCR1A &= ~(1 << WGM10);
	TCCR1A &= (1 << WGM11);
	TCCR1B |= (1 << WGM12); 
	TCCR1B &= ~(1 << WGM13);
	
	// Set prescaler to /1
	TCCR1B &= ~((1 << CS12) | (1 << CS11));
	TCCR1B |= (1 << CS10);
	
	// Enable Compare match interrupt (Channel A)
	TIMSK1 |= (1 << OCIE1A);
}

void step(const uint8_t n)
{
	switch(n) {
		case 0:
		PORTA |= (1 << driver_in1) | (1 << DRIVER_IN4);
		PORTA &= ~((1 << driver_in2) | (1 << DRIVER_IN3));
		break;
		case 1:
		PORTA |= (1 << driver_in2) | (1 << DRIVER_IN4);
		PORTA &= ~((1 << driver_in1) | (1 << DRIVER_IN3));
		break;
		case 2:
		PORTA |= (1 << driver_in2) | (1 << DRIVER_IN3);
		PORTA &= ~((1 << driver_in1) | (1 << DRIVER_IN4));
		break;
		case 3:
		PORTA |= (1 << driver_in1) | (1 << DRIVER_IN3);
		PORTA &= ~((1 << driver_in2) | (1 << DRIVER_IN4));
		break;
		default:
		break;
	}
	
}

uint16_t calculate_on_time_from_adc()
{
	// wait for conversion to complete
	while((ADCSRA & (1 << ADIF)) != (1 << ADIF));
	uint16_t raw = ADCL;
	raw |= (ADCH << 8);
	raw <<= 3; // multiply by 8
	
	ADCSRA |= (1 << ADIF);
	
	return raw;
}

ISR(TIM1_COMPA_vect) {
	// flag is auto-cleared
	static volatile uint8_t step_counter = 0;
	static volatile uint16_t speed = 0;
	step(step_counter);
	if(++step_counter > 3) {
		step_counter = 0;
		OCR1A = on_time_us;
	}	
	if(++speed > (on_time_us >> 4)) {
		speed = 0;
		PORTB ^= (1 << SPEED_LED);
	}
	
}

int main(void)
{
	GPIO_Config();
	ADC_Config();
	TIMER_Config();
	
	DDRB |= (1 << DDB0);
	
	sei();
	
	while (1)
    {
		// Direction control
		if((PINB & (1 << DIRECTION)) == (1 << DIRECTION))
		{
			driver_in1 = DRIVER_IN2;
			driver_in2 = DRIVER_IN1;	
		}
		else
		{
			driver_in1 = DRIVER_IN1;
			driver_in2 = DRIVER_IN2;
		}
		
		on_time_us = calculate_on_time_from_adc();	
    }
}


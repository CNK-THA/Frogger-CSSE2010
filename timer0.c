/*
* timer0.c
*
* Author: Peter Sutton
*
* We setup timer0 to generate an interrupt every 1ms
* We update a global clock tick variable - whose value
* can be retrieved using the get_clock_ticks() function.
*/

#include <avr/io.h>
#include <avr/interrupt.h>


#include "timer0.h"

/* Our internal clock tick count - incremented every
* millisecond. Will overflow every ~49 days. */
static volatile uint32_t clockTicks;
volatile uint8_t pause_game;
volatile uint8_t displaycc = 0;
volatile uint16_t count = 15;
uint8_t seven_seg_data[10] = {63,6,91,79,102,109,125,7,127,111};

static uint8_t flag = 0; //Is the timer less than 1? To show miliseconds. 0 == No

#define F_CPU 8000000L
#include <util/delay.h>

///BUZZER
uint16_t freq = 200;	// Hz
float dutycycle = 2;	// %
uint16_t clockperiod;
uint16_t pulsewidth;


/* Set up timer 0 to generate an interrupt every 1ms.
* We will divide the clock by 64 and count up to 124.
* We will therefore get an interrupt every 64 x 125
* clock cycles, i.e. every 1 milliseconds with an 8MHz
* clock.
* The counter will be reset to 0 when it reaches it's
* output compare value.
*/
void init_timer0(void) {
	/* Reset clock tick count. L indicates a long (32 bit)
	* constant.
	*/
	clockTicks = 0L;
	
	/* Clear the timer */
	TCNT0 = 0;

	/* Set the output compare value to be 124 */
	OCR0A = 124;
	
	/* Set the timer to clear on compare match (CTC mode)
	* and to divide the clock by 64. This starts the timer
	* running.
	*/
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS01)|(1<<CS00);

	/* Enable an interrupt on output compare match.
	* Note that interrupts have to be enabled globally
	* before the interrupts will fire.
	*/
	TIMSK0 |= (1<<OCIE0A);
	
	/* Make sure the interrupt flag is cleared by writing a
	* 1 to it.
	*/
	TIFR0 &= (1<<OCF0A);

	//Buzzer
	clockperiod = (1000000UL / freq);
	pulsewidth = (dutycycle * clockperiod) / 100;

	// Set the maximum count value for timer/counter 1 to be one less than the clockperiod
	OCR2A = clockperiod - 1;
	
	// Set the count compare value based on the pulse width. The value will be 1 less
	// than the pulse width - unless the pulse width is 0.
	if(pulsewidth == 0) {
		OCR2B = 0;
		} else {
		OCR2B = pulsewidth - 1;
	}
	
	// Set up timer/counter 1 for Fast PWM, counting from 0 to the value in OCR1A
	// before reseting to 0. Count at 1MHz (CLK/8).
	// Configure output OC1B to be clear on compare match and set on timer/counter
	// overflow (non-inverting mode).
	TCCR2A = (1<<COM2A1)|(0<<COM2A0)|(1 << COM0B1) | (0 <<COM0B0)| (1 <<WGM21) | (1 << WGM20);
	TCCR2B = (1 << WGM22) | (0 << CS22) | (1 << CS21) | (0 << CS20);
	
}

uint32_t get_current_time(void) {
	uint32_t returnValue;

	/* Disable interrupts so we can be sure that the interrupt
	* doesn't fire when we've copied just a couple of bytes
	* of the value. Interrupts are re-enabled if they were
	* enabled at the start.
	*/
	uint8_t interruptsOn = bit_is_set(SREG, SREG_I);
	cli();
	returnValue = clockTicks;
	if(interruptsOn) {
		sei();
	}
	return returnValue;
}

void pause(void){ //Stop all game function
	pause_game = 1;
}
void unpause(void){ //resume all game function from the point of stopping
	pause_game = 0;
}

ISR(TIMER0_COMPA_vect) {
	/* Increment our clock tick count */
	if(!pause_game){
		clockTicks++;
	}
	
}

void game_count(void){
	//SET UP FOR 7 display

	DDRC |= 0xFF;
	DDRD |= 0x04;

	OCR1A = 9999; /* Clock divided by 8 - count for 10000 cycles */
	TCCR1A = 0; /* CTC mode */
	TCCR1B = (1<<WGM12)|(1<<CS12)|(1<<CS10); /* Divide clock by 256 */

	///* Enable interrupt on timer on output compare match
	//*/
	TIMSK1 = (1<<OCIE1A);

	/* Ensure interrupt flag is cleared */
	TIFR1 = (1<<OCF1A);


	///* Turn on global interrupts */
	sei();
}

void show_count(void){
	if(!flag){ //Is it  less than 1 second? flag = 1 == yes it is less than 1 second. Show 0.x on display.
		if(displaycc == 0) {
			/* Display rightmost digit - tenths of seconds */
			PORTC = seven_seg_data[count%10];
			PORTD = 0x00;
			
			} else {
			/* Display leftmost digit - seconds + decimal point */
			PORTC = seven_seg_data[count/10];
			PORTD = 0x04;
		}
		}else{ // No timer is more than 1 second.
		if(displaycc == 0) {
			/* Display rightmost digit - tenths of seconds */
			PORTC = seven_seg_data[count%10];
			PORTD = 0x00;
			
			} else {
			/* Display leftmost digit - seconds + decimal point */
			PORTC = seven_seg_data[0]|0b10000000;
			PORTD = 0x04;
		}
	}
	
	displaycc = 1 ^ displaycc;
	/* Output the digit selection (CC) bit */
}

void reset_count(void){ 
	count = 15;
}

uint16_t get_count(void){
	return count;
}

ISR(TIMER1_COMPA_vect) {
	if(!pause_game){
		count--;
		if((count == 1) && (!flag)){ //Display milliseconds
			TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10);
			flag = 1;
			count = 9;
		}
		else if(count<=0){ //Display seconds
			TCCR1B = (1<<WGM12)|(1<<CS12)|(1<<CS10);
			flag = 0;
		}
	}
}

void make_noise(uint16_t tone){ //Make noise for buzzer. Parameter is frequency.
	for(int i =0; i < 3; i++){
		if(PIND & 0x08) {
			DDRD |= (1<<6);
			freq = tone;
		}
		if(PINA & 0x01) {
			dutycycle = 40;
			}else{
			dutycycle = 2.3;
		}
		
		// Work out the clock period and pulse width
		clockperiod = (1000000UL / freq);
		pulsewidth = (dutycycle * clockperiod) / 100;
		
		// Update the PWM registers
		if(pulsewidth > 0) {
			// The compare value is one less than the number of clock cycles in the pulse width
			OCR2B = pulsewidth - 1;
			} else {
			OCR2B = 0;
		}
		// Set the maximum count value for timer/counter 1 to be one less than the clockperiod
		OCR2A = clockperiod - 1;
		_delay_ms(200);
	}
	DDRD &= 0xBF;
}



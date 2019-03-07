/*
* FroggerProject.c
*
* Main file
*
* Author: Peter Sutton. Modified by Chanon Kachornvuthidej 44456553
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#define F_CPU 8000000L
#include <util/delay.h>

#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);
void handle_next_level(void);

//Speed timer. Higher means slower.
volatile int speed0 = 1300;
volatile int speed1 = 1250;
volatile int speed2 = 1200;
volatile int speed3 = 1150;
volatile int speed4 = 1100;

// ASCII code for Escape character
#define ESCAPE_CHAR 27

volatile uint8_t x_or_y = 0;	/* 0 = x, 1 = y */
volatile uint32_t xcoord = 530;
volatile uint32_t ycoord = 530;

/////////////////////////////// main //////////////////////////////////
int main(void) {

	// Setup hardware and call backs. This will turn on
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	while(1) {
		new_game();
		play_game();
		if(is_riverbank_full()){
			handle_next_level();
			}else{
			handle_game_over();
		}
	}
}

void initialise_hardware(void) {
	
	// Set up ADC - AVCC reference, right adjust
	// Input selection doesn't matter yet - we'll swap this around in the while
	// loop below.
	ADMUX = (1<<REFS0);
	// Turn on the ADC (but don't start a conversion yet). Choose a clock
	// divider of 64. (The ADC clock must be somewhere
	// between 50kHz and 200kHz. We will divide our 8MHz clock by 64
	// to give us 125kHz.)
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1);


	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	
	//FOR LED LIVES
	DDRA |= 0b11111000;
	
	//For buzzer
	DDRD |= (0<<3);
	
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Frogger"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Chanon Kachornvuthidej - 44456553"));
	init_score(); //Demonstration purposes
	show_high();
	
	
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("FROGGER 44456553", COLOUR_GREEN);
		// Scroll the message until it has scrolled off the
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}
}

void new_game(void) {
	
	// Initialise the game and display
	initialise_game();
	
	// Clear the serial terminal
	clear_terminal();

	// Initialise the score
	init_score();
	move_cursor(60,12);
	printf_P(PSTR("Your score is: "));
	show_score();
	move_cursor(60,13);
	printf_P(PSTR("This is level: "));
	show_level();
	unpause();
	game_count();
	
	
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	uint32_t current_time, last_move_time, last_move_time1, last_move_time2, last_move_time2_0, last_move_time2_1,
	last_move_frog;
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;

	
	// Get the current time and remember this as the last time the vehicles
	// and logs were moved.
	current_time = get_current_time();
	last_move_time = current_time;
	last_move_time1 = current_time;
	last_move_time2 = current_time;
	last_move_time2_0 = current_time;
	last_move_time2_1 = current_time;

	last_move_frog = current_time;
	
	// We play the game while the frog is alive and we haven't filled up the
	// far riverbank
	while(!is_frog_dead() && !is_riverbank_full()) {
		show_count();
		if(x_or_y == 0) { //For joystick
			ADMUX = 0b01000001;
			} else {
			ADMUX = 0b01000010;
		}
		// Start the ADC conversion
		ADCSRA |= (1<<ADSC);
		
		while(ADCSRA & (1<<ADSC)) {
			; /* Wait until conversion finished */
		}

		if(!is_frog_dead() && frog_has_reached_riverbank()) {
			// Frog reached the other side successfully but the
			// riverbank isn't full, put a new frog at the start
			put_frog_in_start_position();
		}
		
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority over serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;

		button = button_pushed();
		
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
					} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
					} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
					// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
					} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}

		if(current_time >= last_move_frog + 500){ //read joystick.
			if(x_or_y == 0) {
				xcoord = ADC;
				} else {
				ycoord = ADC;
			}
		}
		// Next time through the loop, do the other direction
		x_or_y ^= 1;
		
		// Process the input.
		if(button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l'|| ((xcoord < 300 && xcoord > 0) &&
		(ycoord < 570 && ycoord > 470))) {
			move_frog_to_left();
			xcoord = 0;
			last_move_frog = current_time;
			} else if(button==2 || escape_sequence_char=='A' || serial_input=='U' || serial_input=='u' || ycoord > 600) {
			// Attempt to move forward
			if((xcoord < 480 && xcoord > 0)){ //diagonal movement
				move_left_up();
				}else if (xcoord > 570){
				move_right_up();
				}else{
				move_frog_forward();
			}
			show_score();
			xcoord = 0;
			ycoord = 0;
			last_move_frog = current_time;
			} else if(button==1 || escape_sequence_char=='B' || serial_input=='D' || serial_input=='d' || (ycoord < 400 && ycoord > 0)) {
			// Attempt to move down
			if((xcoord < 480 && xcoord > 0)){ //diagonal movement
				move_left_down();
				}else if(xcoord > 570){
			move_right_down();}
			else{
				move_frog_backward();
			}
			ycoord = 0;
			xcoord = 0;
			last_move_frog = current_time;
			} else if(button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r' || (xcoord > 800 && (ycoord < 570 && ycoord > 470))) {
			move_frog_to_right();
			xcoord = 0;
			last_move_frog = current_time;
		}
		else if(serial_input == 'p' || serial_input == 'P') {
			pause();
			DDRD &= 0xBF;
			while(1){
				show_count();
				if(serial_input_available()){
					serial_input = fgetc(stdin);
					if(serial_input == 'p' || serial_input == 'P'){
						clear_push();
						break;
					}
				}
			}
			unpause();
		}
		// else - invalid input or we're part way through an escape sequence -
		// do nothing
		
		current_time = get_current_time();

		if(!is_frog_dead() && current_time >= last_move_time + speed0){ //Move row at different speed
			scroll_vehicle_lane(0, 1);
			last_move_time = current_time;
		}
		if(!is_frog_dead() && current_time >= last_move_time1 + speed1) {
			scroll_vehicle_lane(1, -1);
			last_move_time1 = current_time;
		}
		if(!is_frog_dead() && current_time >= last_move_time2 + speed2) {
			scroll_vehicle_lane(2, 1);
			last_move_time2 = current_time;
		}
		if(!is_frog_dead() && current_time >= last_move_time2_0 + speed3) {
			scroll_river_channel(0, -1);
			last_move_time2_0 = current_time;
		}
		
		if(!is_frog_dead() && current_time >= last_move_time2_1 + speed4) {
			scroll_river_channel(1, 1);
			last_move_time2_1 = current_time;
		}

		//CODE for showing LED lives
		switch(5 - get_time_died()){
			case 1:
			PORTA = 0x08;
			break;
			case 2:
			PORTA = 0x18;
			break;
			case 3:
			PORTA = 0x38;
			break;
			case 4:
			PORTA = 0x78;
			break;
			case 5:
			PORTA = 0xF8;
			break;
		}

	}
	show_score();
	PORTA = 0x00;
	
	// We get here if the frog is dead or the riverbank is full
	// The game is over.
}

void handle_game_over() {
	//Reset all value back to original
	reset_level();
	reset_count();
	speed0 = 1300;
	speed1 = 1250;
	speed2 = 1200;
	speed3 = 1150;
	speed4 = 1100;

	int column = 0;
	char name[11];
	char serial_input = -1;
	move_cursor(10,3);
	printf_P(PSTR("GAME OVER"));
	
	if(get_score_collect()[4] < get_score()){ //Get the name of player if score is higher than the lowest rank
		move_cursor(10,7);
		printf_P(PSTR("Congratulations you scored a high score!! Please type your name: "));
		move_cursor(column,9);
		while(column < 10){
			serial_input = -1;
			if(serial_input_available()) {
				serial_input = fgetc(stdin);
				if((serial_input == 'D') && (column > 0)){
					column--;
					move_cursor(column,9);
				}
				else if(serial_input == 10){
					break;
				}
				else if(((serial_input>= 65) && (serial_input <=90))| ((serial_input>=97) && (serial_input <= 122)) |
				(serial_input==32)){
					name[column] = serial_input;
					printf("%c",serial_input);
					column++;
				}
			}
		}
		name[10] = '\0';
		add_data(name);
		save_game();
	}
	
	move_cursor(10,5);
	printf_P(PSTR("Press a button to start again"));
	show_high();
	clear_push(); //clear any button queue
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}

}
void handle_next_level(void){ //If river bank full then move to next level
	next_level();
	speed0 -= 80;
	speed1 -= 80;
	speed2 -= 80;
	speed3 -= 80;
	speed4 -= 80;
	for(int i =0; i<16;i++){ //Shift display
		_delay_ms(90);
		ledmatrix_shift_display_left();
	}
	make_noise(2000);
}

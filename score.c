/*
* score.c
*
* Written by Peter Sutton
*/

#include "score.h"
#include <string.h>
#include <avr/eeprom.h>

uint32_t score;
uint32_t temp_score = 0;
char name_collect[5][11];
uint32_t score_collect[5];

void init_score(void) {
	score_collect[0] = 5; //Dummy value
	score_collect[1] = 4; //Dummy value
	score_collect[2] = 3; //Dummy value
	score_collect[3] = 2; //Dummy value
	score_collect[4] = 1; //Dummy value
	score = 0;
}

void add_to_score(uint16_t value) {
	score += value;
}
void update_temp(void){ // If successfully reached the other side save this score incase the frog dies
	temp_score = score;
}
void reset_to_temp(void){ // Set score back to last successful cross
	score = temp_score;
}

uint32_t get_score(void) {
	return score;
}

void add_data(char* name){ //Add score and name of player in case of high-score achieved

	uint32_t current, next;
	char c_name[11],n_name[11];
	for(int i = 0;i<5;i++){  //Check position of high-score on the ranking and replace it with the new one
		if(score_collect[i] < score){
			current = score_collect[i];
			score_collect[i] = score;
			strncpy(c_name,name_collect[i],11);
			strcat(name_collect[i], name);

			for(int j = i+1; j < 5; j++){ //shift lower scores down by 1 step and the last one discarded
				next = score_collect[j];
				score_collect[j] = current;
				current = next;
				strncpy(n_name,name_collect[j],11);
				strcat(name_collect[j], c_name);
				strncpy(c_name,n_name,11);
			}
			break;
		}
	}
	
}

uint32_t* get_score_collect(void){
	return score_collect;
}

char* get_name_collect(void){
	return *name_collect;
}

void load_game(void){ //Load from EEPROM
	for(int i = 0 ;i<5;i++){
		score_collect[i] = eeprom_read_dword((uint32_t*)i);
		eeprom_read_block((void*)name_collect[i], (const void*) i+5, 11);
	}
}

void save_game(void){ //Save to EEPROM	for(int i = 0;i<5;i++){		eeprom_update_dword((uint32_t*) i,score_collect[i]);		eeprom_update_block((const void*)name_collect+i, (void*)i+5,11);	}
}



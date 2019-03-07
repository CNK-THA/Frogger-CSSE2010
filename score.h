/*
* score.h
*
* Author: Peter Sutton
*/

#ifndef SCORE_H_
#define SCORE_H_

#include <stdint.h>

void init_score(void);
void add_to_score(uint16_t value);
uint32_t get_score(void);
void add_data(char* name);

void reset_to_temp(void);
void update_temp(void);

void save_game(void);
void load_game(void);

uint32_t* get_score_collect(void);
char* get_name_collect(void);


#endif /* SCORE_H_ */
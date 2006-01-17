/*
 * AweMUD NG - Next Generation AwesomePlay MUD
 * Copyright (C) 2000-2005  AwesomePlay Productions, Inc.
 * See the file COPYING for license details
 * http://www.awemud.net
 */

#ifndef RAND_H
#define RAND_H

void init_random (void);		// initializes random numbers
int get_random (int range);		// returns 0 - range
int roll_dice (int dice, int sides);	// returns <dice>d<sides> roll

#endif

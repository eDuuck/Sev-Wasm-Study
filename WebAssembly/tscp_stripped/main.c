/*
 *	MAIN.C
 *	Tom Kerrigan's Simple Chess Program (TSCP)
 *
 *	Copyright 2025 Tom Kerrigan
 */


//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "data.h"
#include "protos.h"


int one_move(int depth)
{
	int computer_side;
	char s[256];
	int m;
	int i;

	/*Position to move 17 of Bobby Fischer vs. J. Sherwin,
   	New Jersey State Open Championship, 9/2/1957.*/

	int bench_color[64] = {
	EMPTY,  DARK ,  DARK ,  EMPTY,  EMPTY,  DARK ,  DARK ,  EMPTY, 
	DARK ,  EMPTY,  EMPTY,  EMPTY,  EMPTY,  DARK ,  DARK ,  DARK , 
	EMPTY,  DARK ,  EMPTY,  DARK ,  DARK ,  EMPTY,  DARK ,  EMPTY, 
	EMPTY,  EMPTY,  EMPTY,  DARK ,  EMPTY,  EMPTY,  LIGHT,  EMPTY, 
	EMPTY,  EMPTY,  DARK ,  LIGHT,  EMPTY,  EMPTY,  EMPTY,  EMPTY, 
	EMPTY,  EMPTY,  LIGHT,  EMPTY,  EMPTY,  EMPTY,  LIGHT,  EMPTY, 
	LIGHT,  LIGHT,  LIGHT,  EMPTY,  EMPTY,  LIGHT,  LIGHT,  LIGHT, 
	LIGHT,  EMPTY,  LIGHT,  EMPTY,  LIGHT,  EMPTY,  LIGHT,  EMPTY
	};

	int bench_piece[64] = {
	EMPTY , ROOK  , BISHOP, EMPTY , EMPTY , ROOK  , KING  , EMPTY ,
	PAWN  , EMPTY , EMPTY , EMPTY , EMPTY , PAWN  , PAWN  , PAWN  ,
	EMPTY , PAWN  , EMPTY , QUEEN , PAWN  , EMPTY , KNIGHT, EMPTY ,
	EMPTY , EMPTY , EMPTY , KNIGHT, EMPTY , EMPTY , KNIGHT, EMPTY ,
	EMPTY , EMPTY , PAWN  , PAWN  , EMPTY , EMPTY , EMPTY , EMPTY ,
	EMPTY , EMPTY , PAWN  , EMPTY , EMPTY , EMPTY , PAWN  , EMPTY ,
	PAWN  , PAWN  , QUEEN , EMPTY , EMPTY , PAWN  , BISHOP, PAWN  ,
	ROOK  , EMPTY , BISHOP, EMPTY , ROOK  , EMPTY , KING  , EMPTY 
	};


	/*printf("\n");
	printf("Tom Kerrigan's Simple Chess Program (TSCP)\n");
	printf("version 1.83, 2/10/25\n");
	printf("Very stripped down version, calculating one move.\n");
	printf("\n");*/

	init_hash();
	max_depth = depth;
	for (i = 0; i < 64; ++i) {
		color[i] = bench_color[i];
		piece[i] = bench_piece[i];
	}
	side = LIGHT;
	xside = DARK;
	castle = 0;
	ep = -1;
	fifty = 0;
	ply = 0;
	hply = 0;
	set_hash();
	//print_board();
	//think(1);
	think_stripped();
	//printf("Computer's move: %s\n", move_str(pv[0][0].b));
	//printf("%d\n",move_byte_to_int(pv[0][0].b));
	return move_byte_to_int(pv[0][0].b);
	/*
			Should return the following values:
				depth = 1 => 11322 (0x2c3a)
				depth = 2 =>  9246 (0x241e)
				depth = 3 =>  9246 (0x241e)
				depth = 4 =>  9246 (0x241e)
				depth = 5 =>  8242 (0x2032)
	*/
}

int main(void){
	return one_move(5);
}


int move_byte_to_int(move_bytes m){
	int val = 0;
	val += m.from    << (sizeof(char) * 8 * 0);
	val += m.to      << (sizeof(char) * 8 * 1);
	val += m.promote << (sizeof(char) * 8 * 2);
	val += m.bits    << (sizeof(char) * 8 * 3);   
	return val;
}

/* move_str returns a string with move m in coordinate notation 

char *move_str(move_bytes m)
{
	static char str[8];

	char c;

	if (m.bits & 32) {
		switch (m.promote) {
			case KNIGHT:
				c = 'n';
				break;
			case BISHOP:
				c = 'b';
				break;
			case ROOK:
				c = 'r';
				break;
			default:
				c = 'q';
				break;
		}
		sprintf(str, "%c%d%c%d%c",
				COL(m.from) + 'a',
				8 - ROW(m.from),
				COL(m.to) + 'a',
				8 - ROW(m.to),
				c);
	}
	else
		sprintf(str, "%c%d%c%d",
				COL(m.from) + 'a',
				8 - ROW(m.from),
				COL(m.to) + 'a',
				8 - ROW(m.to));
	return str;
} */


/* print_board() prints the board 

void print_board()
{
	int i;
	
	printf("\n8 ");
	for (i = 0; i < 64; ++i) {
		switch (color[i]) {
			case EMPTY:
				printf(" .");
				break;
			case LIGHT:
				printf(" %c", piece_char[piece[i]]);
				break;
			case DARK:
				printf(" %c", piece_char[piece[i]] + ('a' - 'A'));
				break;
		}
		if ((i + 1) % 8 == 0 && i != 63)
			printf("\n%d ", 7 - ROW(i));
	}
	printf("\n\n   a b c d e f g h\n\n");
}
*/
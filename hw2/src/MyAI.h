#ifndef MYAI_INCLUDED
#define MYAI_INCLUDED 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include "pcg_basic.h"
#include <array>
#include <vector>
#include <deque>
#include <random>
#include <cfloat>

#define RED 0
#define BLACK 1
#define CHESS_COVER -1
#define CHESS_EMPTY -2
#define COMMAND_NUM 18

class ChessBoard{
public:
	std::array<int, 32> Board;
	std::array<int, 32> Prev;
	std::array<int, 32> Next;
	int RedHead, BlackHead;
	std::array<int, 14> CoverChess;
	std::array<int, 14> AliveChess;
	int Red_Chess_Num, Black_Chess_Num;
	int NoEatFlip;
	std::vector<int> History;
	std::array<bool, 2> cantWin;
};

class MoveInfo{
public:
	int from_chess_no;
	int to_chess_no;
	int from_location_no;
	int to_location_no;
	MoveInfo(){};
	MoveInfo(const std::array<int, 32>& board, int from, int to){
		from_location_no = from;
		to_location_no = to;
		from_chess_no = board[from];
		to_chess_no = board[to];
	};
	int num() const {
		return from_location_no * 100 + to_location_no;
	};
};

class MyAI  
{
	const char* commands_name[COMMAND_NUM] = {
		"protocol_version",
		"name",
		"version",
		"known_command",
		"list_commands",
		"quit",
		"boardsize",
		"reset_board",
		"num_repetition",
		"num_moves_to_draw",
		"move",
		"flip",
		"genmove",
		"game_over",
		"ready",
		"time_settings",
		"time_left",
  	"showboard"
	};
public:
	MyAI(void);
	~MyAI(void);

	// commands
	bool protocol_version(const char* data[], char* response);// 0
	bool name(const char* data[], char* response);// 1
	bool version(const char* data[], char* response);// 2
	bool known_command(const char* data[], char* response);// 3
	bool list_commands(const char* data[], char* response);// 4
	bool quit(const char* data[], char* response);// 5
	bool boardsize(const char* data[], char* response);// 6
	bool reset_board(const char* data[], char* response);// 7
	bool num_repetition(const char* data[], char* response);// 8
	bool num_moves_to_draw(const char* data[], char* response);// 9
	bool move(const char* data[], char* response);// 10
	bool flip(const char* data[], char* response);// 11
	bool genmove(const char* data[], char* response);// 12
	bool game_over(const char* data[], char* response);// 13
	bool ready(const char* data[], char* response);// 14
	bool time_settings(const char* data[], char* response);// 15
	bool time_left(const char* data[], char* response);// 16
	bool showboard(const char* data[], char* response);// 17

private:
	int Color;
	int Red_Time, Black_Time;
	ChessBoard main_chessboard;

#ifdef WINDOWS
	clock_t begin;
#else
	struct timeval begin;
#endif

	pcg32_random_t rng;

	// Utils
	int GetFin(char c);
	int ConvertChessNo(int input);
	bool isTimeUp();
	uint32_t randIndex(uint32_t max);

	// Board
	void initBoardState();
	void generateMove(char move[6]);
	void MakeMove(ChessBoard* chessboard, const int move, const int chess);
	void MakeMove(ChessBoard* chessboard, const char move[6]);
	bool Referee(const std::array<int, 32>& board, const int Startoint, const int EndPoint, const int color);
	void Expand(const ChessBoard *chessboard, const int color, std::deque<MoveInfo> &Result, int* num_eats);
	double Evaluate(const ChessBoard *chessboard, const std::deque<MoveInfo> &Moves, const int color, const int depth);
	double Simulate(ChessBoard chessboard, const int color, const int max_depth);
	bool isDraw(const ChessBoard* chessboard);
	bool isFinish(const ChessBoard *chessboard, const std::deque<MoveInfo> &Moves, const int eatNum, const int color, const int remain_depth);
	double SEE(const ChessBoard *chessboard, const int position, const int color);
	void fastForward(ChessBoard* chessboard, std::vector<MoveInfo>& history);

	// Display
	void Pirnf_Chess(int chess_no,char *Result);
	void Pirnf_Chessboard();
};

#endif


#ifndef MYAI_INCLUDED
#define MYAI_INCLUDED 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <array>
#include <vector>

#define RED 0
#define BLACK 1
#define CHESS_COVER -1
#define CHESS_EMPTY -2
#define COMMAND_NUM 19

using namespace std;

class ChessBoard{
public:
	array<int, 32> Board;
	array<int, 32> Prev;
	array<int, 32> Next;
	array<int, 14> CoverChess;
	array<int, 14> AliveChess;
	array<int, 3> Heads;
	array<int, 3> Chess_Nums;

	int NoEatFlip;
	vector<int> History;
};

class MoveInfo{
public:
	int from_chess_no;
	int to_chess_no;
	int from_location_no;
	int to_location_no;
	int priority;
	int num;
	MoveInfo(const array<int, 32>& board, int from, int to);
	friend bool operator > (const MoveInfo& l, const MoveInfo& r);
};

// class WallHistory{
// 	array<double, 5> wall;
// 	double wall_sum, wall_sum2;
// 	int history;
// 	int capacity;
// public:
// 	WallHistory():wall_sum(0),wall_sum2(0),history(0),capacity(wall.size()){}
// 	int add(double w){
// 		int index = history % capacity;
// 		if(history >= capacity){
// 			wall_sum -= wall[index];
// 			wall_sum2 -= wall[index]*wall[index];
// 		}
// 		wall[index] = w;
// 		wall_sum += w;
// 		wall_sum2 += w*w;
// 		history++;
// 		return history;
// 	}
// 	double mean(){
// 		return history < capacity ? 0 : (wall_sum / capacity);
// 	}
// 	double std(double mu){
// 		return history < capacity ? 0 : sqrt(wall_sum2 / capacity - mu*mu);
// 	}
// };

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
  	"showboard",
		"init_board"
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
	bool init_board(const char* data[], char* response);// 18

private:
	int Color;
	int Red_Time, Black_Time;
	ChessBoard main_chessboard;
	bool timeIsUp;

#ifdef WINDOWS
	clock_t begin;
	clock_t origin;
#else
	struct timeval begin;
	struct timeval origin;
#endif
	int num_plys;

	// next ply
	double ply_time;
	double ply_elapsed;

	// statistics
	int node;
	// array<WallHistory, 34> depth_wall;

	// Utils
	int GetFin(char c);
	int ConvertChessNo(int input);

	// Board
	void initBoardState();
	void initBoardState(const char* data[]);
	void generateMove(char move[6]);
	void MakeMove(ChessBoard* chessboard, const int move, const int chess);
	void MakeMove(ChessBoard* chessboard, const char move[6]);
	bool Referee(const array<int, 32>& board, const int Startoint, const int EndPoint, const int color);
	void Expand(const ChessBoard *chessboard, const int color, vector<MoveInfo> &Result);
	// double Evaluate(const ChessBoard* chessboard, const int legal_move_count, const int color);
	double Evaluate(const ChessBoard *chessboard, const vector<MoveInfo> &Moves, const int color);
	double Nega_scout(const ChessBoard chessboard, int* move, const int color, const int depth, const int remain_depth, const double alpha, const double beta);
	double Star0_EQU(const ChessBoard& chessboard, int move, const vector<int>& Choice, const int color, const int depth, const int remain_depth);
	bool isDraw(const ChessBoard* chessboard);

	bool isTimeUp();
	double estimatePlyTime();

	// Display
	void Pirnf_Chess(int chess_no,char *Result);
	void Pirnf_Chessboard();
	
};

#endif


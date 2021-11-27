#ifndef MYAI_INCLUDED
#define MYAI_INCLUDED 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include "pcg_basic.h"
#include <vector>
#include <deque>
#include <random>
#include <cassert>

#define RED 0
#define BLACK 1
#define CHESS_COVER -1
#define CHESS_EMPTY -2
#define COMMAND_NUM 18

class ChessBoard{
public:
	int Board[32];
	int Prev[32];
	int Next[32];
	int RedHead, BlackHead;
	int CoverChess[14];
	int AliveChess[14];
	int Red_Chess_Num, Black_Chess_Num;
	int NoEatFlip;
	std::vector<int> History;
};

class MoveInfo{
public:
	int from_chess_no;
	int to_chess_no;
	int from_location_no;
	int to_location_no;
	MoveInfo(){};
	MoveInfo(const int *board, int from, int to){
		from_location_no = from;
		to_location_no = to;
		from_chess_no = board[from];
		to_chess_no = board[to];
	};
	int num(){
		return from_location_no * 100 + to_location_no;
	};
};

class MCTree{
public:
	int max_depth;
	std::vector<std::vector<int>> histories;
	std::vector<double> sum_scores;
	std::vector<int> n_trials;
	std::vector<int> parent_id;
	std::vector<int> depth;
	std::vector<std::vector<int>> children_ids;
	MCTree():max_depth(0),histories(),sum_scores(),n_trials(),parent_id(),depth(),children_ids(){};

	double score(int i) {
		return sum_scores[i] / n_trials[i];
	}
	int expand(int parent, int new_move){
		int id = histories.size();
		std::vector<int> new_hist;
		if (parent != -1){
			new_hist = histories[parent]; // copy
			new_hist.push_back(new_move);
			assert(new_hist.size() == histories[parent].size() + 1);			
		}
		histories.push_back(new_hist);
		sum_scores.push_back(0);
		n_trials.push_back(0);
		parent_id.push_back(parent);
		depth.push_back(parent == -1 ? 0 : (depth[parent]+1));
		children_ids.push_back(std::vector<int>());
		if (parent != -1)
			children_ids[parent].push_back(id);
		max_depth = std::max(depth.back(), max_depth);
		return id;
	}
	void update(int i, double sum, int trials){
		// TODO: use while
		if (i == -1) return;
		sum_scores[i] += sum;
		n_trials[i] += trials;
		update(parent_id[i], sum, trials);
	}
	int pv_leaf(){
		/* Minimax:
			0: max node
			1: min node
			etc.
		*/
		int par = 0;
		while(!children_ids[par].empty()){	
			double co = (depth[par]&1 ? -1 : 1);
			int best_ch = children_ids[par].front();
			for(auto& ch: children_ids[par]){
				if(co*score(ch) > co*score(best_ch))
					best_ch = ch;
			}
			par = best_ch;
		}
		return par;
	}
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
	template<class T>
	T randItem(const std::deque<T>& container){
		uint32_t i = randIndex(container.size());
		return container[i];
	};

	// Board
	void initBoardState();
	void generateMove(char move[6]);
	void MakeMove(ChessBoard* chessboard, const int move, const int chess);
	void MakeMove(ChessBoard* chessboard, const char move[6]);
	bool Referee(const int *board, const int Startoint, const int EndPoint, const int color);
	void Expand(const ChessBoard *chessboard, const int color, std::deque<MoveInfo> &Result, int* num_eats);
	double Evaluate(const ChessBoard *chessboard, const std::deque<MoveInfo> &Moves, const int color);
	double Simulate(ChessBoard chessboard, const int color);
	bool isDraw(const ChessBoard* chessboard);
	bool isFinish(const ChessBoard *chessboard, const std::deque<MoveInfo> &Moves, const int eatNum, const int color);
	double SEE(const ChessBoard *chessboard, const int position, const int color);
	void fastForward(ChessBoard* chessboard, std::vector<int>& history);

	// Display
	void Pirnf_Chess(int chess_no,char *Result);
	void Pirnf_Chessboard();
};

#endif


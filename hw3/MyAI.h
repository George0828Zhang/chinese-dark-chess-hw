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
#include <tsl/robin_map.h>
#include <unordered_map>
#include <random>

#define RED 0
#define BLACK 1
#define CHESS_COVER -1
#define CHESS_EMPTY -2
#define COMMAND_NUM 19

#define MAX_DEPTH 31

using namespace std;

/* courtesy of https://codereview.stackexchange.com/questions/109260/seed-stdmt19937-from-stdrandom-device */
template<class T = std::mt19937, std::size_t N = T::state_size * sizeof(typename T::result_type)>
auto ProperlySeededRandomEngine () -> typename std::enable_if<N, T>::type {
    std::random_device source;
    std::random_device::result_type random_data[(N - 1) / sizeof(source()) + 1];
    std::generate(std::begin(random_data), std::end(random_data), std::ref(source));
    std::seed_seq seeds(std::begin(random_data), std::end(random_data));
    return T(seeds);
}

class ChessBoard{
public:
	array<int, 32> Board;
	array<int, 32> Prev;
	array<int, 32> Next;
	array<int, 14> CoverChess;
	array<int, 14> AliveChess;
	array<int, 3> Heads;
	array<int, 3> Chess_Nums;
	array<bool, 2> cantWin;

	int NoEatFlip;
	vector<int> History;
};

class MoveInfo{
public:
	int from_chess_no;
	int to_chess_no;
	int from_location_no;
	int to_location_no;
	int raw_priority;
	int priority;
	int rank;
	int num;
	MoveInfo(){}
	MoveInfo(const array<int, 32>& board, int from, int to);
};

#define ENTRY_EXACT 0
#define ENTRY_LOWER 1
#define ENTRY_UPPER 2

using key128_t = __uint128_t;

class TableEntry{
public:
	// key128_t p;
	double value;
	// int depth;
	int rdepth;
	int vtype;
	MoveInfo child_move;
	vector<MoveInfo> all_moves;
	vector<int> flip_choices;
};

class TransPosition{
	static const int POSITIONS = 32; 
	static const int TYPES = 15;
	array<key128_t, POSITIONS*TYPES> salt; // 32 pos, 14+1 types
	array<tsl::robin_map<key128_t, TableEntry>, 2> tables;
public:
	void init(mt19937_64& rng);
	static inline int Convert(int chess);
	key128_t compute_hash(const ChessBoard& chessboard) const;
	key128_t MakeMove(const key128_t& other, const MoveInfo& move, const int chess = 0) const;
	bool query(const key128_t& key, const int color, TableEntry& result);
	bool insert(const key128_t& key, const int color, const TableEntry& update);
	void clear_tables(const vector<int>& ids);
	void clone(const int to, const int from);
};

class KillerTable{
	// [depth][m] -> depth*2 + i
	array<MoveInfo, (MAX_DEPTH+1)*2> table;
	array<bool, MAX_DEPTH+1> next_id;
public:
	KillerTable(){
		next_id.fill(0);
	}
	static inline bool same(const MoveInfo& a, const MoveInfo& b){
		return (a.from_location_no == b.from_location_no) &&
			(a.to_location_no == b.to_location_no);
	}
	bool is_killer(const MoveInfo& move, const int depth){
		return same(table[depth*2], move) || same(table[depth*2+1], move);
	}
	void insert(const MoveInfo& move, const int depth){
		table[depth*2 + next_id[depth]] = move;
		next_id[depth] = !next_id[depth];
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

	// predict
	MoveInfo prediction;

	// killer
	KillerTable killer;

	// statistics
	int node;

	// Utils
	int GetFin(char c);
	int ConvertChessNo(int input);

	// Board
	void initBoardState();
	void initBoardState(const char* data[]);
	void generateMove(char move[6]);
	void openingMove(char move[6]);
	void MakeMove(ChessBoard* chessboard, const int move, const int chess);
	void MakeMove(ChessBoard* chessboard, const char move[6]);
	bool Referee(const array<int, 32>& board, const int Startoint, const int EndPoint, const int color);
	bool Referee_debug(const array<int, 32>& board, const int Startoint, const int EndPoint, const int color, int* fail_no);
	void Expand(const ChessBoard *chessboard, const int color, vector<MoveInfo> &Result);
	double evalColor(const ChessBoard *chessboard, const vector<MoveInfo> &Moves, const int color);
	double Evaluate(const ChessBoard *chessboard, const vector<MoveInfo> &Moves, const int color);
	double Nega_scout(const ChessBoard chessboard, const key128_t& boardkey, MoveInfo& move, const int n_flips, const int prev_flip, const int color, const int depth, const int remain_depth, const double alpha, const double beta);
	double Star0_EQU(const ChessBoard& chessboard, const key128_t& boardkey, const MoveInfo& move, const int n_flips, const vector<int>& Choice, const int color, const int depth, const int remain_depth);
	// double SEE(const ChessBoard *chessboard, const int position, const int color);
	bool searchExtension(const ChessBoard& chessboard, const vector<MoveInfo> &Moves, const int color);
	bool isDraw(const ChessBoard* chessboard);
	void moveOrdering(const key128_t& boardkey, vector<MoveInfo>& Moves, const int depth);
	bool skipDraw(const ChessBoard& new_chessboard, const key128_t& newkey, const int depth, const int num_moves, const int move_i, const double cur_best);

	bool isTimeUp();
	double estimatePlyTime();
	bool cantWinCheck(const ChessBoard *chessboard, const int color, const bool is_next);

	// Display
	void Pirnf_Chess(int chess_no,char *Result);
	void Pirnf_Chessboard();

	TransPosition transTable;

};

const array<int, 32*10> cannon_shoot = {
	1, 2, 3, 4, 8, 12, 16, 20, 24, 28, 
	0, 2, 3, 5, 9, 13, 17, 21, 25, 29, 
	0, 1, 3, 6, 10, 14, 18, 22, 26, 30, 
	0, 1, 2, 7, 11, 15, 19, 23, 27, 31, 
	0, 5, 6, 7, 8, 12, 16, 20, 24, 28, 
	1, 4, 6, 7, 9, 13, 17, 21, 25, 29, 
	2, 4, 5, 7, 10, 14, 18, 22, 26, 30, 
	3, 4, 5, 6, 11, 15, 19, 23, 27, 31, 
	0, 4, 9, 10, 11, 12, 16, 20, 24, 28, 
	1, 5, 8, 10, 11, 13, 17, 21, 25, 29, 
	2, 6, 8, 9, 11, 14, 18, 22, 26, 30, 
	3, 7, 8, 9, 10, 15, 19, 23, 27, 31, 
	0, 4, 8, 13, 14, 15, 16, 20, 24, 28, 
	1, 5, 9, 12, 14, 15, 17, 21, 25, 29, 
	2, 6, 10, 12, 13, 15, 18, 22, 26, 30, 
	3, 7, 11, 12, 13, 14, 19, 23, 27, 31, 
	0, 4, 8, 12, 17, 18, 19, 20, 24, 28, 
	1, 5, 9, 13, 16, 18, 19, 21, 25, 29, 
	2, 6, 10, 14, 16, 17, 19, 22, 26, 30, 
	3, 7, 11, 15, 16, 17, 18, 23, 27, 31, 
	0, 4, 8, 12, 16, 21, 22, 23, 24, 28, 
	1, 5, 9, 13, 17, 20, 22, 23, 25, 29, 
	2, 6, 10, 14, 18, 20, 21, 23, 26, 30, 
	3, 7, 11, 15, 19, 20, 21, 22, 27, 31, 
	0, 4, 8, 12, 16, 20, 25, 26, 27, 28, 
	1, 5, 9, 13, 17, 21, 24, 26, 27, 29, 
	2, 6, 10, 14, 18, 22, 24, 25, 27, 30, 
	3, 7, 11, 15, 19, 23, 24, 25, 26, 31, 
	0, 4, 8, 12, 16, 20, 24, 29, 30, 31, 
	1, 5, 9, 13, 17, 21, 25, 28, 30, 31, 
	2, 6, 10, 14, 18, 22, 26, 28, 29, 31, 
	3, 7, 11, 15, 19, 23, 27, 28, 29, 30
};

const array<int, 32*32> rel_distance = {
	0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 7, 8, 9, 10, 
	1, 0, 1, 2, 2, 1, 2, 3, 3, 2, 3, 4, 4, 3, 4, 5, 5, 4, 5, 6, 6, 5, 6, 7, 7, 6, 7, 8, 8, 7, 8, 9, 
	2, 1, 0, 1, 3, 2, 1, 2, 4, 3, 2, 3, 5, 4, 3, 4, 6, 5, 4, 5, 7, 6, 5, 6, 8, 7, 6, 7, 9, 8, 7, 8, 
	3, 2, 1, 0, 4, 3, 2, 1, 5, 4, 3, 2, 6, 5, 4, 3, 7, 6, 5, 4, 8, 7, 6, 5, 9, 8, 7, 6, 10, 9, 8, 7, 
	1, 2, 3, 4, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 
	2, 1, 2, 3, 1, 0, 1, 2, 2, 1, 2, 3, 3, 2, 3, 4, 4, 3, 4, 5, 5, 4, 5, 6, 6, 5, 6, 7, 7, 6, 7, 8, 
	3, 2, 1, 2, 2, 1, 0, 1, 3, 2, 1, 2, 4, 3, 2, 3, 5, 4, 3, 4, 6, 5, 4, 5, 7, 6, 5, 6, 8, 7, 6, 7, 
	4, 3, 2, 1, 3, 2, 1, 0, 4, 3, 2, 1, 5, 4, 3, 2, 6, 5, 4, 3, 7, 6, 5, 4, 8, 7, 6, 5, 9, 8, 7, 6, 
	2, 3, 4, 5, 1, 2, 3, 4, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8, 
	3, 2, 3, 4, 2, 1, 2, 3, 1, 0, 1, 2, 2, 1, 2, 3, 3, 2, 3, 4, 4, 3, 4, 5, 5, 4, 5, 6, 6, 5, 6, 7, 
	4, 3, 2, 3, 3, 2, 1, 2, 2, 1, 0, 1, 3, 2, 1, 2, 4, 3, 2, 3, 5, 4, 3, 4, 6, 5, 4, 5, 7, 6, 5, 6, 
	5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0, 4, 3, 2, 1, 5, 4, 3, 2, 6, 5, 4, 3, 7, 6, 5, 4, 8, 7, 6, 5, 
	3, 4, 5, 6, 2, 3, 4, 5, 1, 2, 3, 4, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 
	4, 3, 4, 5, 3, 2, 3, 4, 2, 1, 2, 3, 1, 0, 1, 2, 2, 1, 2, 3, 3, 2, 3, 4, 4, 3, 4, 5, 5, 4, 5, 6, 
	5, 4, 3, 4, 4, 3, 2, 3, 3, 2, 1, 2, 2, 1, 0, 1, 3, 2, 1, 2, 4, 3, 2, 3, 5, 4, 3, 4, 6, 5, 4, 5, 
	6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0, 4, 3, 2, 1, 5, 4, 3, 2, 6, 5, 4, 3, 7, 6, 5, 4, 
	4, 5, 6, 7, 3, 4, 5, 6, 2, 3, 4, 5, 1, 2, 3, 4, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 
	5, 4, 5, 6, 4, 3, 4, 5, 3, 2, 3, 4, 2, 1, 2, 3, 1, 0, 1, 2, 2, 1, 2, 3, 3, 2, 3, 4, 4, 3, 4, 5, 
	6, 5, 4, 5, 5, 4, 3, 4, 4, 3, 2, 3, 3, 2, 1, 2, 2, 1, 0, 1, 3, 2, 1, 2, 4, 3, 2, 3, 5, 4, 3, 4, 
	7, 6, 5, 4, 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0, 4, 3, 2, 1, 5, 4, 3, 2, 6, 5, 4, 3, 
	5, 6, 7, 8, 4, 5, 6, 7, 3, 4, 5, 6, 2, 3, 4, 5, 1, 2, 3, 4, 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 
	6, 5, 6, 7, 5, 4, 5, 6, 4, 3, 4, 5, 3, 2, 3, 4, 2, 1, 2, 3, 1, 0, 1, 2, 2, 1, 2, 3, 3, 2, 3, 4, 
	7, 6, 5, 6, 6, 5, 4, 5, 5, 4, 3, 4, 4, 3, 2, 3, 3, 2, 1, 2, 2, 1, 0, 1, 3, 2, 1, 2, 4, 3, 2, 3, 
	8, 7, 6, 5, 7, 6, 5, 4, 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0, 4, 3, 2, 1, 5, 4, 3, 2, 
	6, 7, 8, 9, 5, 6, 7, 8, 4, 5, 6, 7, 3, 4, 5, 6, 2, 3, 4, 5, 1, 2, 3, 4, 0, 1, 2, 3, 1, 2, 3, 4, 
	7, 6, 7, 8, 6, 5, 6, 7, 5, 4, 5, 6, 4, 3, 4, 5, 3, 2, 3, 4, 2, 1, 2, 3, 1, 0, 1, 2, 2, 1, 2, 3, 
	8, 7, 6, 7, 7, 6, 5, 6, 6, 5, 4, 5, 5, 4, 3, 4, 4, 3, 2, 3, 3, 2, 1, 2, 2, 1, 0, 1, 3, 2, 1, 2, 
	9, 8, 7, 6, 8, 7, 6, 5, 7, 6, 5, 4, 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0, 4, 3, 2, 1, 
	7, 8, 9, 10, 6, 7, 8, 9, 5, 6, 7, 8, 4, 5, 6, 7, 3, 4, 5, 6, 2, 3, 4, 5, 1, 2, 3, 4, 0, 1, 2, 3, 
	8, 7, 8, 9, 7, 6, 7, 8, 6, 5, 6, 7, 5, 4, 5, 6, 4, 3, 4, 5, 3, 2, 3, 4, 2, 1, 2, 3, 1, 0, 1, 2, 
	9, 8, 7, 8, 8, 7, 6, 7, 7, 6, 5, 6, 6, 5, 4, 5, 5, 4, 3, 4, 4, 3, 2, 3, 3, 2, 1, 2, 2, 1, 0, 1, 
	10, 9, 8, 7, 9, 8, 7, 6, 8, 7, 6, 5, 7, 6, 5, 4, 6, 5, 4, 3, 5, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 0
};

#endif


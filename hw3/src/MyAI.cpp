#include "float.h"
#ifdef WINDOWS
#include <time.h>
#else
#include <sys/time.h>
#endif
#include "MyAI.h"
#include <algorithm>
#include <cmath>
#include <cassert>

#define NOASSERT
#ifdef NOASSERT
#undef assert
#define assert(x) ((void)0)
#endif

#define WIN 1.0
#define DRAW 0.2
#define LOSE 0.0
#define BONUS 0.3
#define BONUS_MAX_PIECE 8

#define OFFSET (WIN + BONUS)

#define NOEATFLIP_LIMIT 60
#define POSITION_REPETITION_LIMIT 3

#define TOTAL_TIME 900000.
#define MAX_PLY_TIME 15000.

#define WALL_MULTIPLIER 4

// #ifdef FAST
#define EXPECT_PLYS 250 / 2
#define EXPECT_PLYS_LONG 320 / 2
// #else
// #define EXPECT_PLYS 180 / 2
// #define EXPECT_PLYS_LONG 320 / 2
// #endif

#define USE_STAR1
#define MAX_FLIPS_IN_SEARCH 3
#define MIN_FLIP_INTERVAL 1
#define MIN_R_DEPTH_TO_FLIP 3 

#define CLEAR_TRANS_FREQ 1
#define USE_TRANSPOSITION
#define USE_ASPIRATION
// #define USE_SEARCH_EXTENSION
#define USE_QUIESCENT
#define USE_KILLER

// #define DISTANCE

using namespace std;

mt19937_64 rng;

MyAI::MyAI(void){srand(time(NULL));}

MyAI::~MyAI(void){}

bool MyAI::protocol_version(const char* data[], char* response){
	strcpy(response, "1.1.0");
  return 0;
}

bool MyAI::name(const char* data[], char* response){
	strcpy(response, "MyAI");
	return 0;
}

bool MyAI::version(const char* data[], char* response){
	strcpy(response, "1.0.0");
	return 0;
}

bool MyAI::known_command(const char* data[], char* response){
  for(int i = 0; i < COMMAND_NUM; i++){
		if(!strcmp(data[0], commands_name[i])){
			strcpy(response, "true");
			return 0;
		}
	}
	strcpy(response, "false");
	return 0;
}

bool MyAI::list_commands(const char* data[], char* response){
  for(int i = 0; i < COMMAND_NUM; i++){
		strcat(response, commands_name[i]);
		if(i < COMMAND_NUM - 1){
			strcat(response, "\n");
		}
	}
	return 0;
}

bool MyAI::quit(const char* data[], char* response){
  fprintf(stderr, "Bye\n"); 
	return 0;
}

bool MyAI::boardsize(const char* data[], char* response){
  fprintf(stderr, "BoardSize: %s x %s\n", data[0], data[1]); 
	return 0;
}

bool MyAI::reset_board(const char* data[], char* response){
	this->Red_Time = -1; // unknown
	this->Black_Time = -1; // unknown
	this->initBoardState();
	return 0;
}

bool MyAI::num_repetition(const char* data[], char* response){
  return 0;
}

bool MyAI::num_moves_to_draw(const char* data[], char* response){
  return 0;
}

bool MyAI::move(const char* data[], char* response){
  char move[6];
	sprintf(move, "%s-%s", data[0], data[1]);
	this->MakeMove(&(this->main_chessboard), move);
	return 0;
}

bool MyAI::flip(const char* data[], char* response){
  char move[6];
	sprintf(move, "%s(%s)", data[0], data[1]);
	this->MakeMove(&(this->main_chessboard), move);
	return 0;
}

bool MyAI::genmove(const char* data[], char* response){
	// set color
	if(!strcmp(data[0], "red")){
		this->Color = RED;
	}else if(!strcmp(data[0], "black")){
		this->Color = BLACK;
	}else{
		this->Color = 2;
	}
	// genmove
  char move[6];
	this->generateMove(move);
	sprintf(response, "%c%c %c%c", move[0], move[1], move[3], move[4]);
	return 0;
}

bool MyAI::game_over(const char* data[], char* response){
  fprintf(stderr, "Game Result: %s\n", data[0]); 
	return 0;
}

bool MyAI::ready(const char* data[], char* response){
  return 0;
}

bool MyAI::time_settings(const char* data[], char* response){
  return 0;
}

bool MyAI::time_left(const char* data[], char* response){
  if(!strcmp(data[0], "red")){
		sscanf(data[1], "%d", &(this->Red_Time));
	}else{
		sscanf(data[1], "%d", &(this->Black_Time));
	}
	fprintf(stderr, "Time Left(%s): %s\n", data[0], data[1]); 
	return 0;
}

bool MyAI::showboard(const char* data[], char* response){
  Pirnf_Chessboard();
	return 0;
}

bool MyAI::init_board(const char* data[], char* response){
  initBoardState(data);
	return 0;
}


// *********************** MOVEINFO *********************** //

#define PRIORITY_FLIP 1
#define PRIORITY_MOVE 10
#define PRIORITY_EAT 100
#define PRIORITY_SEARCHED 10000
#define PRIORITY_KILLER 1000000

MoveInfo::MoveInfo(const array<int, 32>& board, int from, int to){
	from_location_no = from;
	to_location_no = to;
	from_chess_no = board[from];
	to_chess_no = board[to];
	num = from_location_no * 100 + to_location_no;
	
	// calculate priority
	int from_id = (from_chess_no % 7)+1;
	if (from_location_no == to_location_no){
		raw_priority = PRIORITY_FLIP;
	}
	else if (to_chess_no >= 0){
		int to_id = (to_chess_no % 7)+1;
		raw_priority = (to_id*10 + (8-from_id)) * PRIORITY_EAT;
	}
	else{		
		raw_priority = from_id * PRIORITY_MOVE;
	}
	raw_priority += rng() % PRIORITY_MOVE;
	priority = raw_priority;
}
void MyAI::moveOrdering(const key128_t& boardkey, vector<MoveInfo>& Moves, const int depth){
#ifdef USE_KILLER
	for(auto& m: Moves){
		if(this->killer.is_killer(m, depth)){
			m.priority = PRIORITY_KILLER + m.raw_priority;
		}
	}
#endif
	sort(Moves.begin(), Moves.end(), [](const MoveInfo& a, const MoveInfo& b) {return a.priority > b.priority;});
	if (depth == 0){
		// set priority to rank to be printed
		for(uint i = 0; i < Moves.size(); i++){
			Moves[i].rank = i; 
		}
	}
}

// *********************** AI FUNCTION *********************** //

int MyAI::GetFin(char c)
{
	static const char skind[]={'-','K','G','M','R','N','C','P','X','k','g','m','r','n','c','p'};
	for(int f=0;f<16;f++)if(c==skind[f])return f;
	return -1;
}

int MyAI::ConvertChessNo(int input)
{
	switch(input)
	{
	case 0:
		return CHESS_EMPTY;
		break;
	case 8:
		return CHESS_COVER;
		break;
	case 1:
		return 6;
		break;
	case 2:
		return 5;
		break;
	case 3:
		return 4;
		break;
	case 4:
		return 3;
		break;
	case 5:
		return 2;
		break;
	case 6:
		return 1;
		break;
	case 7:
		return 0;
		break;
	case 9:
		return 13;
		break;
	case 10:
		return 12;
		break;
	case 11:
		return 11;
		break;
	case 12:
		return 10;
		break;
	case 13:
		return 9;
		break;
	case 14:
		return 8;
		break;
	case 15:
		return 7;
		break;
	}
	return -1;
}


void MyAI::initBoardState()
{	
	const array<int, 14> iPieceCount{5, 2, 2, 2, 2, 2, 1, 5, 2, 2, 2, 2, 2, 1};
	main_chessboard.CoverChess = iPieceCount;
	main_chessboard.AliveChess = iPieceCount;
	main_chessboard.Chess_Nums = { 16, 16, 32 };
	main_chessboard.NoEatFlip = 0;
	main_chessboard.cantWin.fill(false);

	main_chessboard.Board.fill(CHESS_COVER);

	// initially, all chess are part of the COVER chain
	main_chessboard.Heads = { -1, -1, 0 };
	for(int i = 0; i < 32; i++){
		main_chessboard.Prev[i] = i - 1;
		main_chessboard.Next[i] = i == 31 ? -1 : (i + 1);
	}

	main_chessboard.History.reserve(1024);
	num_plys = 0;

	rng = ProperlySeededRandomEngine<mt19937_64>();
	transTable.init(rng);

	Pirnf_Chessboard();
}

void MyAI::initBoardState(const char* data[])
{	
	fprintf(stderr, "initBoardState(data) not implemented but called!\n");
	assert(false);
}

void MyAI::openingMove(char move[6])
{
/* generateMove Call by reference: change src,dst */
	unsigned long long flip = rng() % 32;

	// replace the move and score
	int StartPoint = flip;
	int EndPoint   = flip;
	sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));
	
	char chess_Start[4]="";
	char chess_End[4]="";
	Pirnf_Chess(main_chessboard.Board[StartPoint],chess_Start);
	Pirnf_Chess(main_chessboard.Board[EndPoint],chess_End);
	printf("My result: \n--------------------------\n");
	printf("Opening\n");
	printf("(%d) -> (%d)\n",StartPoint,EndPoint);
	printf("<%s> -> <%s>\n",chess_Start,chess_End);
	printf("move:%s\n",move);
	printf("--------------------------\n");
	this->Pirnf_Chessboard();
}

template<class T>
void humanReadableByteCountSI(T bytes, char msg[]) {
    if (bytes < 1000) {
        sprintf(msg, "%lu", bytes);
    }
    string ci = "kMGTPE";
	int i = 0;
    while (bytes >= 999950) {
        bytes /= 1000;
        i++;
    }
	sprintf(msg, "%.1lf%c", bytes / 1000.0, ci[i]);
}

void MyAI::generateMove(char move[6])
{
/* generateMove Call by reference: change src,dst */
	if (this->Color == 2){
		openingMove(move);
		return;
	}

	int StartPoint = 0;
	int EndPoint = 0;

	double t = -DBL_MAX;
#ifdef WINDOWS
	begin = clock();
	if (num_plys==0)
		origin = clock();
#else
	gettimeofday(&begin, 0);
	if (num_plys==0)
		gettimeofday(&origin, 0);
#endif
	num_plys++;

	estimatePlyTime();
	fprintf(stderr, "Estimate ply time: %+1.3lf s\n", this->ply_time / 1000);

	// iterative-deeping, start from 3, time limit = <TIME_LIMIT> sec
	double expected = 0;
	double prev_end = 0;

	key128_t boardkey = this->transTable.compute_hash(this->main_chessboard);

	this->killer.shift_up(2); // 2 plys has gone by

	char op_move[6];
	int op_start;
	int op_end;

	// depth-3 search
	this->node = 0;
	MoveInfo best_move_tmp;
	t = Nega_scout(this->main_chessboard, boardkey, best_move_tmp, 0, -1, this->Color, 0, 3, -DBL_MAX, DBL_MAX);
	StartPoint = best_move_tmp.from_location_no;
	EndPoint   = best_move_tmp.to_location_no;
	sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));

	op_start = this->prediction.from_location_no;
	op_end = this->prediction.to_location_no;
	sprintf(op_move, "%c%c-%c%c",'a'+(op_start%4),'1'+(7-op_start/4),'a'+(op_end%4),'1'+(7-op_end/4));

	double wall = this->ply_elapsed - prev_end;
	prev_end = this->ply_elapsed;
	expected = wall * WALL_MULTIPLIER;

	int killer_rate = (int)this->killer.success_rate();
	this->killer.clear_stats();
	fprintf(stderr, "[%c] Depth: %2d, Node: %10d, Score: %+1.5lf, Move: %s, Kills: %2d%%, Rank: %2d, Wall: %1.3lf, Next: %s\n", (this->timeIsUp ? 'U' : 'D'),
			3, node, t - OFFSET, move, killer_rate, best_move_tmp.rank, wall / 1000, op_move);
	fflush(stderr);

	// deeeper search
	for(int depth = 4; !isTimeUp() && depth <= MAX_DEPTH && abs(t - OFFSET) < WIN; depth+=2){
		if (this->ply_time - this->ply_elapsed < expected){
			// will early stop -> give up now
			fprintf(stderr, "Early stopped. Expected: %1.3lf, Remain: %1.3lf\n", expected / 1000, (this->ply_time - this->ply_elapsed) / 1000);
			fflush(stderr);
			break;
		}

		this->node = 0;
		double t_tmp;

		// run Nega-max
#ifdef USE_ASPIRATION
		const double threshold = 0.005;
#else
		const double threshold = DBL_MAX - OFFSET;
#endif
		double alpha = t - threshold;
		double beta = t + threshold;
		t_tmp = Nega_scout(this->main_chessboard, boardkey, best_move_tmp, 0, -1, this->Color, 0, depth, alpha, beta);

		int i = 0, j = 0;
		for (; t_tmp <= alpha; i++){ // failed low
			alpha = t_tmp - threshold * pow(10., i+1);
			t_tmp = Nega_scout(this->main_chessboard, boardkey, best_move_tmp, 0, -1, this->Color, 0, depth, alpha, t_tmp);
		}
		for (; t_tmp >= beta; j++){ // failed high
			beta = t_tmp + threshold * pow(10., j+1);
			t_tmp = Nega_scout(this->main_chessboard, boardkey, best_move_tmp, 0, -1, this->Color, 0, depth, t_tmp, beta);
		}

		// check score
		// if search all nodes
		// replace the move and score
		if(!this->timeIsUp){
			t = t_tmp;
			StartPoint = best_move_tmp.from_location_no;
			EndPoint   = best_move_tmp.to_location_no;
			sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));

			op_start = this->prediction.from_location_no;
			op_end = this->prediction.to_location_no;
			sprintf(op_move, "%c%c-%c%c",'a'+(op_start%4),'1'+(7-op_start/4),'a'+(op_end%4),'1'+(7-op_end/4));
		}else {best_move_tmp.rank = -1;}		

		double wall = this->ply_elapsed - prev_end;
		prev_end = this->ply_elapsed;
		expected = wall * WALL_MULTIPLIER;
		// U: Undone
		// D: Done
		killer_rate = (int)this->killer.success_rate();
		this->killer.clear_stats();
		fprintf(stderr, "[%c] Depth: %2d, Node: %10d, Score: %+1.5lf, Move: %s, Kills: %2d%%, Rank: %2d, Wall: %1.3lf, Next: %s\n", (this->timeIsUp ? 'U' : 'D'),
			depth, node, t - OFFSET, move, killer_rate, best_move_tmp.rank, wall / 1000, op_move);
		fflush(stderr);
	}
	if(this->main_chessboard.cantWin[this->Color]){
		fprintf(stderr, "Can't win.\n");
		fflush(stderr);
	}

#ifdef USE_TRANSPOSITION
	size_t n_query = this->transTable.num_query;
	size_t n_hit = this->transTable.num_hit;
	char msg[3][64];
	humanReadableByteCountSI(
		this->transTable.num_keys[RED] + this->transTable.num_keys[BLACK], msg[0]);
	humanReadableByteCountSI(n_hit, msg[1]);
	humanReadableByteCountSI(n_query, msg[2]);
	fprintf(stderr, "Table size: %s (entries), Hit rate: %s / %s (%.1lf%%)\n", msg[0], msg[1], msg[2], n_hit / (double)n_query * 100);

	if (num_plys % CLEAR_TRANS_FREQ == 0)
		this->transTable.clear_tables({RED,BLACK});
#endif
	
	char chess_Start[4]="";
	char chess_End[4]="";
	Pirnf_Chess(main_chessboard.Board[StartPoint],chess_Start);
	Pirnf_Chess(main_chessboard.Board[EndPoint],chess_End);
	printf("My result: \n--------------------------\n");
	printf("Nega scout: %lf (node: %d)\n", t, this->node);
	printf("(%d) -> (%d)\n",StartPoint,EndPoint);
	printf("<%s> -> <%s>\n",chess_Start,chess_End);
	printf("move:%s\n",move);
	printf("--------------------------\n");
	this->Pirnf_Chessboard();
}

void removeFromBoard(ChessBoard* chessboard, const int at){
	int left = chessboard->Prev[at];
	int right = chessboard->Next[at];
	if (left != -1)
		chessboard->Next[left] = right;
	if (right != -1)
		chessboard->Prev[right] = left;
	chessboard->Prev[at] = -1;
	chessboard->Next[at] = -1;
	for(auto& h: chessboard->Heads){
		if (h == at){
			h = right;
		}
	}
}

void insertInBoard(ChessBoard* chessboard, const int at, const bool flip){
	assert(chessboard->Board[at] != CHESS_EMPTY);
	assert(chessboard->Board[at] != CHESS_COVER);

	if (flip){
		// remove from COVER chain
		removeFromBoard(chessboard, at);
	}

	/*
	> 8 : --> use incremental
	<= 8: --> use list
	 */
	const int threshold = 8;
	int color = chessboard->Board[at] / 7;
	int old_head = chessboard->Heads[color];
	if (chessboard->Heads[color] == -1 || chessboard->Heads[color] > at)
		chessboard->Heads[color] = at;
	int cur_head = chessboard->Heads[color];
	int num_chess = chessboard->Chess_Nums[color];

	int left = -1, right = -1;
	assert(old_head != at);
	if (cur_head != old_head){
		right = old_head;
	}
	else if (num_chess > threshold){
		for(int i = at-1; i >= 0; i--){
			if (chessboard->Board[i] >= 0 && chessboard->Board[i] / 7 == color){
				left = i;
				right = chessboard->Next[i];
				break;
			}
		}
		// definitely have left
		assert(left != -1);
	}
	else{
		// definitely have cur_head
		for (int i = cur_head; i != -1; i = chessboard->Next[i]){
			if (i < at &&
				(chessboard->Next[i] == -1 || chessboard->Next[i] > at))
			{
				left = i;
				right = chessboard->Next[i];
				break;
			}
		}
		assert(left != -1);
	}

	chessboard->Prev[at] = left;
	chessboard->Next[at] = right;
	if (left != -1){
		chessboard->Next[left] = at;
	}
	if (right != -1){
		chessboard->Prev[right] = at;
	}
}

void moveInBoard(ChessBoard *chessboard, const int src, const int dst){
	/* Optimizes:
	removeFromBoard(chessboard, src);
	insertInBoard(chessboard, dst);
	*/
	int left = chessboard->Prev[src];
	int right = chessboard->Next[src];
	if (left < dst &&
		(right == -1 || right > dst))
	{
		if (left != -1)
			chessboard->Next[left] = dst;
		if (right != -1)
			chessboard->Prev[right] = dst;
		chessboard->Prev[dst] = left;
		chessboard->Next[dst] = right;
		chessboard->Prev[src] = -1;
		chessboard->Next[src] = -1;
		if (chessboard->Heads[RED] == src)
			chessboard->Heads[RED] = dst;
		if (chessboard->Heads[BLACK] == src)
			chessboard->Heads[BLACK] = dst;
	}
	else{
		removeFromBoard(chessboard, src);
		insertInBoard(chessboard, dst, false);
	}
}

bool MyAI::cantWinCheck(const ChessBoard *chessboard, const int color, const bool is_next){
	/*
	P C N R M G K
	0 1 2 3 4 5 6
	*/
	if (chessboard->cantWin[color])
		return true; // cant once, cant forever
	int my_num = chessboard->Chess_Nums[color];
	int op_color = color^1;
	// no chess
	if (my_num == 0)
		return true;
	// single cannon
	if (chessboard->AliveChess[color * 7 + 1] > 0 && my_num <= 1)
		return true;

	// single chess and not cover
	if (my_num == 1 && chessboard->Chess_Nums[2] == 0){
		int me = chessboard->Heads[color];
		if (chessboard->Chess_Nums[op_color] > 1) // single chess cannot eat more
			return true;
		for (int i = chessboard->Heads[op_color]; i != -1; i = chessboard->Next[i]){
			int delta = rel_distance[me*32+i];
			if(is_next != (delta % 2)){
				assert((is_next && (delta % 2 == 0)) || (!is_next && (delta % 2 == 1)));
				// next && delta even || !next && delta odd
				return true;
			}
		}
	}
	
	/* Domination:
		For each type, if number of pieces that can capture it
		is less than the number of pieces of that type, we loses
	*/
	std::vector<int> myGreater(8, 0);
	myGreater[7] = chessboard->AliveChess[color * 7 + 1]; // cannon
	for (int i = 6; i >= 0; i--)
	{
		myGreater[i] = myGreater[i+1] + chessboard->AliveChess[color * 7 + i];
	}
	myGreater[0] -= chessboard->AliveChess[color * 7 + 6]; // king can't eat pawn
	myGreater[6] += chessboard->AliveChess[color * 7 + 0]; // pawn can eat king

	for(int i = 0; i < 7; i++){
		if (myGreater[i] < chessboard->AliveChess[op_color * 7 + i]){
			return true;
		}
	}
	return false;
}

void MyAI::MakeMove(ChessBoard* chessboard, const int move, const int chess){
	int src = move/100, dst = move%100;
	if(src == dst){ // flip
		chessboard->Board[src] = chess;
		chessboard->CoverChess[chess]--;
		chessboard->Chess_Nums[2]--;
		chessboard->NoEatFlip = 0;
		insertInBoard(chessboard, src, true);
	}else { // move
		if(chessboard->Board[dst] != CHESS_EMPTY){
			if(chessboard->Board[dst] / 7 == 0){ // red
				(chessboard->Chess_Nums[0])--;
			}else{ // black
				(chessboard->Chess_Nums[1])--;
			}
			chessboard->AliveChess[chessboard->Board[dst]]--;
			removeFromBoard(chessboard, dst);
			chessboard->NoEatFlip = 0;
		}else{
			chessboard->NoEatFlip += 1;
		}
		chessboard->Board[dst] = chessboard->Board[src];
		chessboard->Board[src] = CHESS_EMPTY;
		moveInBoard(chessboard, src, dst);
	}
	chessboard->History.push_back(move);
	chessboard->cantWin[RED] = cantWinCheck(chessboard, RED, chessboard->Board[dst] / 7 == BLACK);
	chessboard->cantWin[BLACK] = cantWinCheck(chessboard, BLACK, chessboard->Board[dst] / 7 == RED);
}

void MyAI::MakeMove(ChessBoard* chessboard, const char move[6])
{ 
	int src, dst, m;
	src = ('8'-move[1])*4+(move[0]-'a');
	if(move[2]=='('){ // flip
		m = src*100 + src;
		printf("# call flip(): flip(%d,%d) = %d\n",src, src, GetFin(move[3])); 
	}else { // move
		dst = ('8'-move[4])*4+(move[3]-'a');
		m = src*100 + dst;
		printf("# call move(): move : %d-%d \n", src, dst);
	}
	MakeMove(chessboard, m, ConvertChessNo(GetFin(move[3])));
	Pirnf_Chessboard();
}

void MyAI::Expand(const ChessBoard *chessboard, const int color, vector<MoveInfo> &Result)
{
	if (color == 2) return;// initial board
#ifndef NOASSERT
	int n_chess = chessboard->Chess_Nums[color];
#endif
	const array<int, 32>& board = chessboard->Board;

	for (int i = chessboard->Heads[color]; i != -1; i = chessboard->Next[i]){
#ifndef NOASSERT
		n_chess--;
#endif
		int row = i/4;
		int col = i%4;
		// Cannon
		if(board[i] % 7 == 1)
		{
			for(int j = 0; j < 10; j++){
				int dst = cannon_shoot[i*10+j];
				if(Referee(board,i,dst,color))
				{
					Result.emplace_back(board, i, dst);
				}
			}
		}
		else
		{
			// up
			if(row > 0 && Referee(board,i,i-4,color))
			{
				Result.emplace_back(board, i, i-4);
			}
			// down
			if(row < 7 && Referee(board,i,i+4,color))
			{
				Result.emplace_back(board, i, i+4);
			}
			// left
			if(col > 0 && Referee(board,i,i-1,color))
			{
				Result.emplace_back(board, i, i-1);
			}
			// right
			if(col < 3 && Referee(board,i,i+1,color))
			{
				Result.emplace_back(board, i, i+1);
			}
		}
	}
#ifndef NOASSERT
	// n_chess should be num covered
	for(int i = 0; i < 7; i++){
		n_chess -= chessboard->CoverChess[color*7 + i];
	}
	assert(n_chess == 0);
#endif
}


// Referee
bool MyAI::Referee(const array<int, 32>& chess, const int from_location_no, const int to_location_no, const int UserId)
{
	// int MessageNo = 0;
	bool IsCurrent = true;
	int from_chess_no = chess[from_location_no];
	int to_chess_no = chess[to_location_no];

	static array<bool, 14*14> can_eat{
		0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
		1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
		1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 
		0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 
	};

	/* Gaurantees:
	1. from_chess_no >= 0 and from_chess_no / 7 == UserId
	2. from_loc != to_loc
	3. for cannon, from_loc and to_loc are on the same row or column, and in board
	4. for others, from_loc and to_loc are adjacent and in board
	*/
	assert(from_chess_no >= 0);
	assert(from_location_no != to_location_no);
	assert(
		((from_chess_no % 7 == 1) && (from_location_no / 4 == to_location_no / 4 || from_location_no % 4 == to_location_no % 4))||
		((from_chess_no % 7 != 1) && (abs(from_location_no / 4 - to_location_no / 4) + abs(from_location_no % 4-to_location_no % 4) == 1))
	);


	if(to_chess_no == CHESS_COVER){
		IsCurrent = false;
	}
	else if (to_chess_no >= 0 && (to_chess_no / 7 == UserId)){
		IsCurrent = false;
	}
	else if (from_chess_no % 7 != 1)// not cannon
	{
		IsCurrent = (to_chess_no == CHESS_EMPTY) || can_eat[from_chess_no * 14 + to_chess_no];
	}
	else if(to_chess_no == CHESS_EMPTY && rel_distance[from_location_no*32+to_location_no] == 1)//cannon walk
	{
		IsCurrent = true;
	}	
	else// cannon jump
	{
		int row_gap = from_location_no / 4 - to_location_no / 4;
		int col_gap = from_location_no % 4 - to_location_no % 4;
		int between_Count = 0;
		//row
		if(row_gap == 0) 
		{
			for(int i=1;i<abs(col_gap);i++)
			{
				int between_chess;
				if(col_gap>0)
					between_chess = chess[from_location_no-i];
				else
					between_chess = chess[from_location_no+i];
				if(between_chess != CHESS_EMPTY)
					between_Count++;
			}
		}
		//column
		else
		{
			for(int i=1;i<abs(row_gap);i++)
			{
				int between_chess;
				if(row_gap > 0)
					between_chess = chess[from_location_no-4*i];
				else
					between_chess = chess[from_location_no+4*i];
				if(between_chess != CHESS_EMPTY)
					between_Count++;
			}
			
		}

		if(between_Count != 1 )
		{
			// MessageNo = 4;
			//strcat(Message,"**gun can't eat opp without between one piece**");
			IsCurrent = false;
		}
		else if(to_chess_no == CHESS_EMPTY)
		{
			// MessageNo = 5;
			//strcat(Message,"**gun can't eat opp without between one piece**");
			IsCurrent = false;
		}
	}
	assert(Referee_debug(chess, from_location_no, to_location_no, UserId, nullptr) == IsCurrent);
	return IsCurrent;
}
bool MyAI::Referee_debug(const std::array<int, 32>& chess, const int from_location_no, const int to_location_no, const int UserId, int *fail_no)
{
	int MessageNo = 0;
	bool IsCurrent = true;
	int from_chess_no = chess[from_location_no];
	int to_chess_no = chess[to_location_no];
	int from_row = from_location_no / 4;
	int to_row = to_location_no / 4;
	int from_col = from_location_no % 4;
	int to_col = to_location_no % 4;

	if(from_chess_no < 0 || ( to_chess_no < 0 && to_chess_no != CHESS_EMPTY) )
	{  
		MessageNo = 1;
		//strcat(Message,"**no chess can move**");
		//strcat(Message,"**can't move darkchess**");
		IsCurrent = false;
	}
	else if (from_chess_no >= 0 && from_chess_no / 7 != UserId)
	{
		MessageNo = 2;
		//strcat(Message,"**not my chess**");
		IsCurrent = false;
	}
	else if((from_chess_no / 7 == to_chess_no / 7) && to_chess_no >= 0)
	{
		MessageNo = 3;
		//strcat(Message,"**can't eat my self**");
		IsCurrent = false;
	}
	//check attack
	else if(to_chess_no == CHESS_EMPTY && abs(from_row-to_row) + abs(from_col-to_col)  == 1)//legal move
	{
		IsCurrent = true;
	}	
	else if(from_chess_no % 7 == 1 ) //judge gun
	{
		int row_gap = from_row-to_row;
		int col_gap = from_col-to_col;
		int between_Count = 0;
		//slant
		if(from_row-to_row == 0 || from_col-to_col == 0)
		{
			//row
			if(row_gap == 0) 
			{
				for(int i=1;i<abs(col_gap);i++)
				{
					int between_chess;
					if(col_gap>0)
						between_chess = chess[from_location_no-i] ;
					else
						between_chess = chess[from_location_no+i] ;
					if(between_chess != CHESS_EMPTY)
						between_Count++;
				}
			}
			//column
			else
			{
				for(int i=1;i<abs(row_gap);i++)
				{
					int between_chess;
					if(row_gap > 0)
						between_chess = chess[from_location_no-4*i] ;
					else
						between_chess = chess[from_location_no+4*i] ;
					if(between_chess != CHESS_EMPTY)
						between_Count++;
				}
				
			}
			
			if(between_Count != 1 )
			{
				MessageNo = 4;
				//strcat(Message,"**gun can't eat opp without between one piece**");
				IsCurrent = false;
			}
			else if(to_chess_no == CHESS_EMPTY)
			{
				MessageNo = 5;
				//strcat(Message,"**gun can't eat opp without between one piece**");
				IsCurrent = false;
			}
		}
		//slide
		else
		{
			MessageNo = 6;
			//strcat(Message,"**cant slide**");
			IsCurrent = false;
		}
	}
	else // non gun
	{
		//judge pawn or king

		//distance
		if( abs(from_row-to_row) + abs(from_col-to_col)  > 1)
		{
			MessageNo = 7;
			//strcat(Message,"**cant eat**");
			IsCurrent = false;
		}
		//judge pawn
		else if(from_chess_no % 7 == 0)
		{
			if(to_chess_no % 7 != 0 && to_chess_no % 7 != 6)
			{
				MessageNo = 8;
				//strcat(Message,"**pawn only eat pawn and king**");
				IsCurrent = false;
			}
		}
		//judge king
		else if(from_chess_no % 7 == 6 && to_chess_no % 7 == 0)
		{
			MessageNo = 9;
			//strcat(Message,"**king can't eat pawn**");
			IsCurrent = false;
		}
		else if(from_chess_no % 7 < to_chess_no% 7)
		{
			MessageNo = 10;
			//strcat(Message,"**cant eat**");
			IsCurrent = false;
		}
	}
	if (fail_no != nullptr)
		*fail_no = MessageNo;
	return IsCurrent;
}


double MyAI::evalColor(const ChessBoard *chessboard, const vector<MoveInfo> &Moves, const int color)
{
	array<double, 14> values{
		1, 180, 6, 18, 90, 270, 810,
		1, 180, 6, 18, 90, 270, 810};
	// if eat a pawn, my king adds this much to its value
	// 5->4 : 0
	// 4->3 : 5
	// 3->2 : 17
	// 2->1 : 80
	// 1->0 : 260 
	static array<double, 6> king_add_n_pawn{  381, 111, 22, 5, 0, 0 };

	// adjust king value
	for(int c = 0; c < 2; c++){
		int op_king = (!c) * 7 + 6;
		int my_pawn = c * 7 + 0;
		int n = chessboard->AliveChess[my_pawn];
		values[op_king] += king_add_n_pawn[n];
	}

	double max_value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 + king_add_n_pawn[0];

	double value = 0;
	for (int i = chessboard->Heads[color]; i != -1; i = chessboard->Next[i])
	{
		value += values[chessboard->Board[i]];
	}
	// covered
	for(int i = 0; i < 7; i++){
		int p = color * 7 + i;
		value += chessboard->CoverChess[p] * values[p];
	}

	/*
	max mobility = 12 + 12 + 14 + 14 = 52 (can right + can left + up + down.)
	max increase:
		normal: +6
		value aware: (810 + king_add_n_pawn[0])*3 + 270*2 + 180
	*/
#ifdef UMOBI
	double mobilities[32] = {0};
	for(auto& m: Moves){
		mobilities[m.from_location_no]++;
	}
	const double w_mob = 1./6;
	const double max_mobi = 60;
#else
	const double w_mob = 1./((810 + king_add_n_pawn[0])*3 + 270*2 + 180);
	const double max_mobi = ((2*2+3*3) + 180*2*4 + 6*2*3 + 18*2*3 + 90*1*3 + (90*1 + 270*2 + 810*1 + king_add_n_pawn[0])*4)*w_mob;
#endif	
	double mobility = 0;
	for(auto& m: Moves){
		if(m.from_chess_no == CHESS_COVER)
			continue;
#ifdef UMOBI
		mobility += w_mob;
#else
		mobility += values[m.from_chess_no]*w_mob;
#endif
	}

	value += mobility;
	max_value += max_mobi;

#ifdef DISTANCE
	/* add bonus to increase distance from predators
	(unrealistic) max distance = 11 * 16 * 16
	max increase:
		+16
	*/
	double distance = 0;
	const double w_dist = 1./16;
	const double max_dist = 1344 * w_dist;
	static array<bool, 14*14> predator{
		/* 
		p  c  n  r  m  g  k  P  C  N  R  M  G  K  */
		0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, // p
		0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, // c
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, // n
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, // r
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // m
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, // g
		0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, // k
		0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,// P
		0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,// C
		0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,// N
		0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,// R
		0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,// N
		0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,// G
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,// K
	};
	if (chessboard->Chess_Nums[color] <= 8){
		// only do this at endgame
		for (int i = chessboard->Heads[color]; i != -1; i = chessboard->Next[i]){
			for (int j = chessboard->Heads[color^1]; j != -1; j = chessboard->Next[j]){
				int ci = chessboard->Board[i];
				int cj = chessboard->Board[j];
				if (predator[ci * 14 + cj]){
					// cj can eat ci
					distance += max(rel_distance[i*32+j], 2) * w_dist;
				}
			}
		}
	} else { distance = max_dist; }
	value += distance;
	max_value += max_dist;
#endif

	assert(value <= max_value);

	return value / max_value;
}


// always use my point of view, so use this->Color
double MyAI::Evaluate(const ChessBoard* chessboard, 
	const vector<MoveInfo>& Moves, const int color){
	// score = My Score - Opponent's Score
	// offset = <WIN + BONUS> to let score always not less than zero

	double score = OFFSET;
	bool finish;
	int legal_move_count = Moves.size();

	if(
		legal_move_count == 0 && 
		chessboard->Heads[2] != -1 &&
		chessboard->Chess_Nums[color] > 0){
		// at least one flip move is legal, unless out of chess
		legal_move_count = 1;
	}

	if(legal_move_count == 0){ // Win, Lose
		if(color == this->Color){ // Lose
			score += LOSE - WIN;
		}else{ // Win
			score += WIN - LOSE;
		}
		finish = true;
	}else if(isDraw(chessboard)){ // Draw
		// score += DRAW - DRAW;
		if (!this->main_chessboard.cantWin[this->Color]){
			score += LOSE - WIN;
		}
		finish = false;
	}else{ // no conclusion

		vector<MoveInfo> myMoves;
		vector<MoveInfo> oppMoves;
		myMoves.reserve(128);
		oppMoves.reserve(128);
		
		if (color == this->Color){
			// copy mine, then expand opponent
			myMoves = Moves;
			Expand(chessboard, this->Color^1, oppMoves);
		}
		else{
			// copy opponent, then expand mine
			oppMoves = Moves;
			Expand(chessboard, this->Color, myMoves);
		}

		// My Material
		double piece_value = (
			evalColor(chessboard, myMoves, this->Color) -
			evalColor(chessboard, oppMoves, this->Color^1)
		);

		// linear map to (-<WIN>, <WIN>)
		piece_value = piece_value / 1.0 * (WIN - 0.01);

		score += piece_value;
		finish = false;
	}

	// Bonus (Only Win / Draw)
	if(finish){
		if((this->Color == RED && chessboard->Chess_Nums[RED] > chessboard->Chess_Nums[BLACK]) ||
			(this->Color == BLACK && chessboard->Chess_Nums[RED] < chessboard->Chess_Nums[BLACK])){
			if(!(legal_move_count == 0 && color == this->Color)){ // except Lose
				double bonus = BONUS / BONUS_MAX_PIECE * 
					abs(chessboard->Chess_Nums[RED] - chessboard->Chess_Nums[BLACK]);
				if(bonus > BONUS){ bonus = BONUS; } // limit
				score += bonus;
			}
		}else if((this->Color == RED && chessboard->Chess_Nums[RED] < chessboard->Chess_Nums[BLACK]) ||
			(this->Color == BLACK && chessboard->Chess_Nums[RED] > chessboard->Chess_Nums[BLACK])){
			if(!(legal_move_count == 0 && color != this->Color)){ // except Lose
				double bonus = BONUS / BONUS_MAX_PIECE * 
					abs(chessboard->Chess_Nums[RED] - chessboard->Chess_Nums[BLACK]);
				if(bonus > BONUS){ bonus = BONUS; } // limit
				score -= bonus;
			}
		}
	}
	
	return score;
}

bool MyAI::searchExtension(const ChessBoard& chessboard, const vector<MoveInfo> &Moves, const int color){
	// Extremely low mobility.	
	if (Moves.size() < 3)
		return true;
	// Last move is capturing.
	if (!chessboard.History.empty()){
		int last = chessboard.History.back();
		if ((chessboard.NoEatFlip == 0) && ((last / 100) != (last % 100)))
			return true;
	}
	// In-check.
	if(chessboard.Chess_Nums[color] == 1 && chessboard.Heads[color] != -1){
		// this color has one chess and not dark
		const array<int, 32>& board = chessboard.Board;
		int op_color = color^1;
		int i = chessboard.Heads[color];

		for(int j = 0; j < 10; j++){
			int src = cannon_shoot[i*10+j];
			if ((board[src] >= 0) && 
				(board[src] / 7 == op_color) &&
				(board[src] % 7 == 1 ||
				 rel_distance[i*32+src] == 1) &&
				Referee(board,src,i,op_color)
			){
				return true;
			}
		}
	}
	// The current best score is much lower than the value of your last ply.
	return false;
}

/* 
Can only be called for a capturing move.
Arguments:
	chessboard:	the chessboard BEFORE the move
	position:	the destination of the move
	color:		the color of the source of the move
*/
double MyAI::SEE(const ChessBoard *chessboard, const int position, const int color){
	/*
	Assume it is red’s turn and there is a black piece bp at location.
		capture piece at location using the first element w in R;
		capture piece at location using the first element h in B; 
		the net gain of material values during the exchange 
	*/

	assert((position >= 0 && position < 32));
	assert(chessboard->Board[position] >= 0);
	assert(chessboard->Board[position] / 7 == (color^1));
	
	array<int, 32> board = chessboard->Board; // copy

	array<int, 16> MyCands;
	array<int, 16> OpCands;
	int my_num = 0;
	int op_num = 0;

	for(int j = 0; j < 10; j++){
		int src = cannon_shoot[position*10+j];
		int col = board[src] / 7;
		if(board[src] >= 0 && (
			board[src] % 7 == 1 ||
			rel_distance[position*32+src] == 1
		)){
			if (col == color){
				MyCands[my_num++] = src;
			}
			else{
				OpCands[op_num++] = src;
			}
		}
	}

	array<double, 14> values{
		1, 180, 6, 18, 90, 270, 810,
		1, 180, 6, 18, 90, 270, 810};

	static array<double, 6> king_add_n_pawn{  381, 111, 22, 5, 0, 0 };

	// adjust king value
	for(int c = 0; c < 2; c++){
		int op_king = (!c) * 7 + 6;
		int my_pawn = c * 7 + 0;
		int n = chessboard->AliveChess[my_pawn];
		values[op_king] += king_add_n_pawn[n];
	}

	// sort descending, will be accessed from the back
	auto comp = [board,values] (int const& a, int const& b) -> bool
    {
       return values[board[a]] > values[board[b]]; // compare chess values
    };
	sort(MyCands.begin(), MyCands.begin() + my_num, comp);
	sort(OpCands.begin(), OpCands.begin() + op_num, comp);
	
	for(int i = 0; i < my_num; i++){
		assert(board[MyCands[i]] >= 0 && board[MyCands[i]] / 7 == color);
		assert(i == 0 || values[board[MyCands[i]]] <= values[board[MyCands[i-1]]]);
	}
	for(int i = 0; i < op_num; i++){
		assert(board[OpCands[i]] >= 0 && board[OpCands[i]] / 7 == (color^1));
		assert(i == 0 || values[board[OpCands[i]]] <= values[board[OpCands[i-1]]]);
	}

	double gain = 0.0;
	int op_color = color^1;

	bool my_turn = true; // i go first
	while((my_turn && my_num > 0) || (!my_turn && op_num > 0)){		
		if (my_turn){
			int from = MyCands[my_num-1];
			if (Referee(board, from, position, color)){
				gain += values[board[position]]; // capture piece
				board[position] = board[from]; // place at position
				board[from] = CHESS_EMPTY;
				my_turn = false; // switch
			}
			my_num--; // remove from cands			
		}
		else{
			// Opponent
			int from = OpCands[op_num-1];
			if (Referee(board, from, position, op_color)){
				gain -= values[board[position]]; // capture piece
				board[position] = board[from]; // place at position
				board[from] = CHESS_EMPTY;
				my_turn = true; // switch
			}
			op_num--; // remove from cands
		}
	}
	return gain;
}

double MyAI::Nega_scout(const ChessBoard chessboard, const key128_t& boardkey, MoveInfo& move, const int n_flips, const int prev_flip, const int color, const int depth, const int remain_depth, double alpha, double beta){
	assert(color == RED || color == BLACK);

	vector<MoveInfo> Moves;
	vector<int> Choice;
	Moves.reserve(128);
	Choice.reserve(16);

	// check table
	TransPosition& table = this->transTable;
	TableEntry entry;
	bool cached_moves = false;
#ifdef USE_TRANSPOSITION
	if(table.query(boardkey, color, &entry)){
		assert(table.compute_hash(chessboard) == boardkey);

		// hash hit
		if (entry.rdepth >= remain_depth){
			if (entry.vtype == ENTRY_EXACT){
				MoveInfo& mv = entry.child_move;
				assert((mv.from_location_no == mv.to_location_no) ||
				Referee_debug(chessboard.Board, mv.from_location_no, mv.to_location_no, color, nullptr));
				move = mv;

				// cancel if this leads to draw
				ChessBoard new_chessboard = chessboard;
				MakeMove(&new_chessboard, mv.num, 0); // 0: dummy
				if(mv.from_location_no == mv.to_location_no ||
					!isDraw(&new_chessboard)){
					// table.num_short++;
					return entry.value;
				}
			}
			else if (entry.vtype == ENTRY_LOWER){
				alpha = max(alpha, entry.value);
			}
			else{
				beta = min(beta, entry.value);
			}

			// check whether alpha < beta! else it is cutoff!!
			if (alpha >= beta){
				move = entry.child_move;
				// table.num_short++;
				return entry.value;
			}
		}
		Moves = entry.all_moves;
		Choice = entry.flip_choices;
		cached_moves = true;
	}
	else{
		Expand(&chessboard, color, Moves);
	}
#else
	Expand(&chessboard, color, Moves);
#endif

	// flip
	// first check whether should do flip
	// TODO: BUGFIX: when we ignore flip moves,
	// but store the move and choices in table,
	// it would be incorrect
	bool has_flips = cached_moves;
	if ((remain_depth >= MIN_R_DEPTH_TO_FLIP) &&
		(!cached_moves) &&
		(n_flips < MAX_FLIPS_IN_SEARCH) &&
		((depth - prev_flip >= MIN_FLIP_INTERVAL) || (n_flips == 0))
	){
		has_flips = true;
		for(int j = 0; chessboard.Chess_Nums[2] > 0 && j < 14; j++){ // find remain chess
			if(chessboard.CoverChess[j] > 0){
				Choice.push_back(j);
			}
		}
		for (int i = chessboard.Heads[2]; i != -1; i = chessboard.Next[i]){
			assert(chessboard.Board[i] == CHESS_COVER);
			Moves.emplace_back(chessboard.Board, i, i);
		}
	}
	moveOrdering(boardkey, Moves, depth);

	bool isQuiescent = true;
#ifdef USE_QUIESCENT
	int see_pos = chessboard.History.empty() ? -1 : (chessboard.History.back() % 100); 
	isQuiescent = (see_pos == -1 || (remain_depth <= 0 && SEE(&chessboard, see_pos, color) <= 0));
#endif

	if(isTimeUp() || // time is up
		depth >= MAX_DEPTH || // reach absolute depth
		(remain_depth <= 0 && isQuiescent) || // reach limit of depth 
		chessboard.Chess_Nums[RED] == 0 || // terminal node (no chess type)
		chessboard.Chess_Nums[BLACK] == 0 || // terminal node (no chess type)
		Moves.empty() || // terminal node (no move type)
		isDraw(&chessboard)  // draw
		){
		this->node++;
		// odd: *-1, even: *1
		return Evaluate(&chessboard, Moves, color) * (depth&1 ? -1 : 1);
	}else{
		double m = -DBL_MAX;
		double n = beta;
		MoveInfo new_move;
		int final_rdepth = remain_depth;
		// search deeper
		for(uint i = 0; i < Moves.size(); i++){ // move
			double t = -DBL_MAX;			
			if (Moves[i].from_location_no == Moves[i].to_location_no){
				// Chance node
#ifdef USE_STAR1
				t = Star1_EQU(chessboard, boardkey, Moves[i], n_flips, Choice, color, depth, remain_depth, max(alpha, m), beta);
#else
				t = Star0_EQU(chessboard, boardkey, Moves[i], n_flips, Choice, color, depth, remain_depth);
#endif

				if(t > m){ 
					// TODO: don't always use best
					m = t;
					move = Moves[i];
					final_rdepth = remain_depth;
				}
			}
			else{
				ChessBoard new_chessboard = chessboard;
				MakeMove(&new_chessboard, Moves[i].num, 0); // 0: dummy
				key128_t new_boardkey = table.MakeMove(boardkey, Moves[i], 0);
				// remain_depth: deepening is 3 -> 4 -> 6 ...
				// before: 2->me 1->op 
				// after : 4->me 3->op 2->me 1->op
				int new_remain_depth = remain_depth;
				int new_n_flips = n_flips;

#ifdef USE_SEARCH_EXTENSION
				if (
					n_flips==0 &&	// dont extend chance search
					depth > 3 &&	// dont extend first search
					remain_depth == 2 &&	// only extend horizon
					color == this->Color && // only do this for me
					searchExtension(new_chessboard, Moves, color)
				){
					new_remain_depth = remain_depth + 2;
					new_n_flips = 1; // Dirty hack to only extend once. (first condition above will fail)
				}
#endif
				t = -Nega_scout(new_chessboard, new_boardkey, new_move, new_n_flips, prev_flip, color^1, depth+1, new_remain_depth-1, -n, -max(alpha, m));

				if(t > m){ 
					if (n == beta || new_remain_depth < 3 || t >= beta){
						m = t;
						move = Moves[i];
						if (depth == 0) this->prediction = new_move;
						final_rdepth = new_remain_depth;
					}
					else{
						t = -Nega_scout(new_chessboard, new_boardkey, new_move, new_n_flips, prev_flip, color^1, depth+1, new_remain_depth-1, -beta, -t);
						m = t;
						move = Moves[i];
						if (depth == 0) this->prediction = new_move;
						final_rdepth = new_remain_depth;
					}
				}
			}

			Moves[i].priority = (int)((t*(depth&1 ? -1 : 1) - OFFSET) * PRIORITY_SEARCHED) + Moves[i].raw_priority;

			if (m >= beta){
				// record the hash entry as a lower bound
#ifdef USE_TRANSPOSITION
				if (has_flips){
					entry.value = m;
					entry.rdepth = final_rdepth;
					entry.vtype = ENTRY_LOWER;
					entry.child_move = move;
					entry.all_moves = Moves;
					entry.flip_choices = Choice;
					table.insert(boardkey, color, entry);
				}
#endif
#ifdef USE_KILLER
				this->killer.insert(move, depth);
#endif
				return m;
			}
#ifdef DISTNACE
			n = max(alpha, m) + 0.00041491510090903424;
#else
			n = max(alpha, m) + 0.0004298982894494268;
#endif
		}

#ifdef USE_TRANSPOSITION
		if (has_flips){
			if (m > alpha){
				// record the hash entry as an exact value m
				entry.vtype = ENTRY_EXACT;
			}
			else{
				// record the hash entry as an upper bound m;
				entry.vtype = ENTRY_UPPER;
			}
			entry.value = m;
			entry.rdepth = final_rdepth;
			entry.child_move = move;
			entry.all_moves = Moves;
			entry.flip_choices = Choice;
			table.insert(boardkey, color, entry);
		}
#endif
		return m;
	}
}

double MyAI::Star0_EQU(const ChessBoard& chessboard, const key128_t& boardkey, const MoveInfo& move, const int n_flips, const vector<int>& Choice, const int color, const int depth, const int remain_depth){
	TransPosition& table = this->transTable;
	MoveInfo new_move;
	double total = 0;
	int trim = max( min(
			(int)log2(Choice.size()+1) * 2 - 1,
			 7), 1);
	// int trim = 3;
	for(auto& k: Choice){
		ChessBoard new_chessboard = chessboard;
		key128_t new_boardkey = table.MakeMove(boardkey, move, k);
		MakeMove(&new_chessboard, move.num, k);

		// int next_col = (color == 2) ? ((k / 7)^1) : (color^1);
		int prev_flip = depth;
		double t = -Nega_scout(new_chessboard, new_boardkey, new_move, n_flips+1, prev_flip, color^1, depth+1, remain_depth-trim, -DBL_MAX, DBL_MAX);
		total += chessboard.CoverChess[k] * t;
	}
	assert(chessboard.Chess_Nums[2] > 0);
	return total / chessboard.Chess_Nums[2];
}

double MyAI::Star1_EQU(const ChessBoard& chessboard, const key128_t& boardkey, const MoveInfo& move, const int n_flips, const vector<int>& Choice, const int color, const int depth, const int remain_depth, const double alpha, const double beta){
	// me: [0, 2*(WIN+BONUS)], opp: [-2*(WIN+BONUS), 0]
	double v_min = (depth&1) ? -2*OFFSET : 0;
	double v_max = (depth&1) ? 0 : 2*OFFSET;

	double c = chessboard.Chess_Nums[2];

	double A = c * (alpha - v_max); // the ( ) / w + v_max is done in loop
	double B = c * (beta - v_min); // the ( ) / w + v_max is done in loop

	double m = v_min;
	double M = v_max;

	// if (depth == 0)
	// 	fprintf(stderr, "[debug] pos: %d\n", move.from_location_no);
	TransPosition& table = this->transTable;
	MoveInfo new_move;
	double total = 0;
	int trim = max( min(
			(int)log2(Choice.size()+1) * 2 - 1,
			 7), 1);
	for(auto& k: Choice){
		ChessBoard new_chessboard = chessboard;
		key128_t new_boardkey = table.MakeMove(boardkey, move, k);
		MakeMove(&new_chessboard, move.num, k);

		assert(-min(B, v_max) <= -max(A, v_min));
		double w = chessboard.CoverChess[k];
		A = A / w + v_max;
		B = B / w + v_min;
		int prev_flip = depth;
		double t = -Nega_scout(new_chessboard, new_boardkey, new_move, n_flips+1, prev_flip, color^1, depth+1, remain_depth-trim, -min(B, v_max), -max(A, v_min));
		
		m += w / c * (t - v_min);
		M += w / c * (t - v_max);
		if (t >= B) return m;
		if (t <= A) return M;
		total += w * t;
		A = w * (A - t); // the ( ) / w + v_max is done above
		B = w * (B - t); // the ( ) / w + v_max is done above
	}
	return total / c;
}

bool MyAI::isDraw(const ChessBoard* chessboard){
	// At least 12 to cause draw
	if(chessboard->NoEatFlip < 12){
		return false;
	}
	// No Eat Flip
	if(chessboard->NoEatFlip >= NOEATFLIP_LIMIT){
		return true;
	}

	// Position Repetition
	int start = chessboard->History.size() - 1;
	// All ply must be move type
	int end = max(start - chessboard->NoEatFlip + 1, 0);
	
	/* Use KMP to check if History[ end ... start ] is a repetition
	   [a b a b a c]
	              ^
				  start
	*/
	const vector<int>& H = chessboard->History;

	// lps array
	array<int, NOEATFLIP_LIMIT> dp = {0};
	int i = 0; // len of current matched suffix
	int j = 2; // current head (length of matched string)

	while(start-j >= end){
		if (H[start-j] == H[start-i]){
			// increase matched len by 1
			i++;
			dp[j] = i;
			j++;

			// is a repeat if dp[n-1] > 0 && n % (n - dp[n-1]) == 0;
			if (j / (j - dp[j-1]) == POSITION_REPETITION_LIMIT){
				return true;
			}
		}
		else if(i > 0){
			// roll back to prev match
			i = dp[i-1];                
		}
		else{
			dp[j] = 0;
			j++;
		}
	}
	return false;
}

bool MyAI::isTimeUp(){
	double elapsed; // ms
	
	// design for different os
#ifdef WINDOWS
	clock_t end = clock();
	elapsed = (end - begin);
#else
	struct timeval end;
	gettimeofday(&end, 0);
	long seconds = end.tv_sec - begin.tv_sec;
	long microseconds = end.tv_usec - begin.tv_usec;
	elapsed = (seconds*1000 + microseconds*1e-3);
#endif
	this->ply_elapsed = elapsed;
	this->timeIsUp = (elapsed >= this->ply_time);
	return this->timeIsUp;
}

double MyAI::estimatePlyTime(){
	int noeatflip = this->main_chessboard.NoEatFlip;
	int my_num = this->main_chessboard.Chess_Nums[this->Color];
	int opp_num = this->main_chessboard.Chess_Nums[this->Color^1];
	double real_exp_ply = this->num_plys + noeatflip + my_num + opp_num;
	if (real_exp_ply < EXPECT_PLYS){
		real_exp_ply = EXPECT_PLYS;
	}
	else if (real_exp_ply < EXPECT_PLYS_LONG){
		real_exp_ply = EXPECT_PLYS_LONG;
	}	

	double timeLeft = this->Color == RED ? this->Red_Time : this->Black_Time;
	if (timeLeft < 0) timeLeft = TOTAL_TIME;

	this->ply_time = min(MAX_PLY_TIME, timeLeft / (real_exp_ply - this->num_plys + 1));
	assert(this->ply_time <= MAX_PLY_TIME);
	assert(this->ply_time >= 0);
	return this->ply_time;
}

//Display chess board
void MyAI::Pirnf_Chessboard()
{	
	char Mes[1024] = "";
	char temp[1024];
	char myColor[10] = "";
	if(Color == -99)
		strcpy(myColor, "Unknown");
	else if(this->Color == RED)
		strcpy(myColor, "Red");
	else
		strcpy(myColor, "Black");
	sprintf(temp, "------------%s-------------\n", myColor);
	strcat(Mes, temp);
	strcat(Mes, "<8> ");
	for(int i = 0; i < 32; i++){
		if(i != 0 && i % 4 == 0){
			sprintf(temp, "\n<%d> ", 8-(i/4));
			strcat(Mes, temp);
		}
		char chess_name[4] = "";
		Pirnf_Chess(this->main_chessboard.Board[i], chess_name);
		sprintf(temp,"%5s", chess_name);
		strcat(Mes, temp);
	}
	strcat(Mes, "\n\n     ");
	for(int i = 0; i < 4; i++){
		sprintf(temp, " <%c> ", 'a'+i);
		strcat(Mes, temp);
	}
	strcat(Mes, "\n\n");
	printf("%s", Mes);
}


// Print chess
void MyAI::Pirnf_Chess(int chess_no,char *Result){
	// XX -> Empty
	if(chess_no == CHESS_EMPTY){	
		strcat(Result, " - ");
		return;
	}
	// OO -> DarkChess
	else if(chess_no == CHESS_COVER){
		strcat(Result, " X ");
		return;
	}

	switch(chess_no){
		case 0:
			strcat(Result, " P ");
			break;
		case 1:
			strcat(Result, " C ");
			break;
		case 2:
			strcat(Result, " N ");
			break;
		case 3:
			strcat(Result, " R ");
			break;
		case 4:
			strcat(Result, " M ");
			break;
		case 5:
			strcat(Result, " G ");
			break;
		case 6:
			strcat(Result, " K ");
			break;
		case 7:
			strcat(Result, " p ");
			break;
		case 8:
			strcat(Result, " c ");
			break;
		case 9:
			strcat(Result, " n ");
			break;
		case 10:
			strcat(Result, " r ");
			break;
		case 11:
			strcat(Result, " m ");
			break;
		case 12:
			strcat(Result, " g ");
			break;
		case 13:
			strcat(Result, " k ");
			break;
	}
}



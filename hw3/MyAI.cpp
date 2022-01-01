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

#define MAX_DEPTH 31

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
#define EXPECT_PLYS 180 / 2

#define MAX_FLIPS_IN_SEARCH 2
#define USE_TRANSPOSITION

using namespace std;

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

#define PRIORITY_FLIP 0
#define PRIORITY_MOVE 1
#define PRIORITY_EAT 100
#define PRIORITY_DONE 10000

MoveInfo::MoveInfo(const array<int, 32>& board, int from, int to){
	from_location_no = from;
	to_location_no = to;
	from_chess_no = board[from];
	to_chess_no = board[to];
	num = from_location_no * 100 + to_location_no;
	
	// calculate priority
	if (from == to){
		priority = PRIORITY_FLIP;
	}
	else if (to_chess_no >= 0){
		int from_id = (from_chess_no % 7)+1; // both 1-7
		int to_id = (to_chess_no % 7)+1;
		priority = (to_id*10 + (8-from_id)) * PRIORITY_EAT;
	}
	else{
		int from_id = (from_chess_no % 7)+1;
		int noise = rand()%10;
		priority = (from_id*10 + noise) * PRIORITY_MOVE;
	}
}
void MyAI::moveOrdering(const key128_t& boardkey, vector<MoveInfo>& Moves, const int depth){
#ifdef USE_TRANSPOSITION
	TransPosition& table = this->transTable;
	TableEntry entry;
	for(auto& m: Moves){
		key128_t new_key = table.MakeMove(boardkey, m);
		int color = m.from_chess_no / 7;
		if(table.query(new_key, color^1, entry)){
			m.priority += (-entry.value + OFFSET) * PRIORITY_DONE;
		}
	}
#endif
	sort(Moves.begin(), Moves.end(), [](const MoveInfo& a, const MoveInfo& b) {return a.priority > b.priority;});
	if (depth == 0){
		// set priority to rank to be printed
		for(uint i = 0; i < Moves.size(); i++){
			Moves[i].priority = i; 
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

	Pirnf_Chessboard();
}

void MyAI::initBoardState(const char* data[])
{	
	fprintf(stderr, "initBoardState(data) not implemented but called!\n");
	assert(false);
}

void MyAI::generateMove(char move[6])
{
/* generateMove Call by reference: change src,dst */
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
	this->transTable.clear_tables();

	for(int depth = 3; !isTimeUp() && depth <= MAX_DEPTH; depth+=2){
		// this->transTable.clear_tables();
		if (this->ply_time - this->ply_elapsed < expected){
			// will early stop -> give up now
			fprintf(stderr, "Early stopped. Expected: %1.3lf, Remain: %1.3lf\n", expected / 1000, (this->ply_time - this->ply_elapsed) / 1000);
			fflush(stderr);
			break;
		}

		this->node = 0;
		MoveInfo best_move_tmp;
		double t_tmp;

		// run Nega-max
		t_tmp = Nega_scout(this->main_chessboard, boardkey, best_move_tmp, this->Color, 0, depth, -DBL_MAX, DBL_MAX);
		t_tmp -= OFFSET; // rescale

		// check score
		// if search all nodes
		// replace the move and score
		if(!this->timeIsUp || depth == 3){
			t = t_tmp;
			StartPoint = best_move_tmp.from_location_no;
			EndPoint   = best_move_tmp.to_location_no;
			sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));
		}else {best_move_tmp.priority = -1;}
		// U: Undone
		// D: Done
		double wall = this->ply_elapsed - prev_end;
		prev_end = this->ply_elapsed;
		expected = wall * 2;

		// depth_wall[depth].add(wall);
		// if (depth > 3){
		// 	double prev_time = depth_wall[depth - 2].mean();
		// 	double cur_time = depth_wall[depth].mean();

		// 	double prev_std = depth_wall[depth - 2].std(prev_time);
		// 	double cur_std = depth_wall[depth].std(cur_time);
		// 	const double error = 1000; // ms
		// 	if(prev_time > 0 && prev_std < error && cur_time > 0 && cur_std < error){
		// 		// cur_time -= cur_std;
		// 		expected = cur_time / prev_time * cur_time;
		// 	}
		// }

		fprintf(stderr, "[%c] Depth: %2d, Node: %10d, Score: %+1.5lf, Move: %s, Rank: %d, Wall: %1.3lf\n", (this->timeIsUp ? 'U' : 'D'),
			depth, node, t, move, best_move_tmp.priority, wall / 1000);
		fflush(stderr);

		// game finish !!!
		if(t >= WIN){
			break;
		}
	}
	
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
	// if (chessboard->Heads[RED] == at)
	// 	chessboard->Heads[RED] = right;
	// if (chessboard->Heads[BLACK] == at)
	// 	chessboard->Heads[BLACK] = right;
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

bool cantWinCheck(const ChessBoard *chessboard, const int color){
	/*
	P C N R M G K
	0 1 2 3 4 5 6
	*/
	if (chessboard->cantWin[color])
		return true; // cant once, cant forever
	int my_num = chessboard->Chess_Nums[color];
	// int op_num = color == RED ? chessboard->Black_Chess_Num : chessboard->Red_Chess_Num;
	int op_color = color^1;
	// single cannon
	if (chessboard->AliveChess[color * 7 + 1] > 0 && my_num < 2)
		return true;
	
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
	for (int c = 0; c < 2; c++){
		chessboard->cantWin[c] = cantWinCheck(chessboard, c);
	}
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
	int n_chess = chessboard->Chess_Nums[color];
	const array<int, 32>& board = chessboard->Board;
	const array<int, 32*10> cannon_shoot{
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

	for (int i = chessboard->Heads[color]; i != -1; i = chessboard->Next[i]){
		n_chess--;
		int row = i/4;
		int col = i%4;
		// Cannon
		if(board[i] % 7 == 1)
		{
			for(int j = 0; j < 10; j++){
				int dst = cannon_shoot.at(i*10+j);
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
	// n_chess should be num covered
	for(int i = 0; i < 7; i++){
		n_chess -= chessboard->CoverChess[color*7 + i];
	}
	assert(n_chess == 0);
}


// Referee
bool MyAI::Referee(const array<int, 32>& chess, const int from_location_no, const int to_location_no, const int UserId)
{
	// int MessageNo = 0;
	bool IsCurrent = true;
	int from_chess_no = chess[from_location_no];
	int to_chess_no = chess[to_location_no];
	int from_row = from_location_no / 4;
	int to_row = to_location_no / 4;
	int from_col = from_location_no % 4;
	int to_col = to_location_no % 4;

	static array<bool, 7*7> can_eat{
		/* 
		p  c  n  r  m  g  k  */
		1, 0, 0, 0, 0, 0, 1, // p -> pawn, king
		0, 0, 0, 0, 0, 0, 0, // c -> need to jump first
		1, 1, 1, 0, 0, 0, 0, // n
		1, 1, 1, 1, 0, 0, 0, // r
		1, 1, 1, 1, 1, 0, 0, // m
		1, 1, 1, 1, 1, 1, 0, // g
		0, 1, 1, 1, 1, 1, 1  // k -> !pawn
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
		((from_chess_no % 7 == 1) && (from_row == to_row || from_col == to_col))||
		((from_chess_no % 7 != 1) && (abs(from_row-to_row) + abs(from_col-to_col) == 1))
	);


	if(to_chess_no == CHESS_COVER){
		IsCurrent = false;
	}
	else if (to_chess_no >= 0 && (to_chess_no / 7 == UserId)){
		IsCurrent = false;
	}
	else if (from_chess_no % 7 != 1)// not cannon
	{
		IsCurrent = (to_chess_no == CHESS_EMPTY) || can_eat.at((from_chess_no % 7)*7 + (to_chess_no % 7));
	}
	else if(to_chess_no == CHESS_EMPTY && abs(from_row-to_row) + abs(from_col-to_col)  == 1)//cannon walk
	{
		IsCurrent = true;
	}	
	else// cannon jump
	{		
		int row_gap = from_row-to_row;
		int col_gap = from_col-to_col;
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
	int fail_no;
	assert(Referee_debug(chess, from_location_no, to_location_no, UserId, &fail_no) == IsCurrent);
	return IsCurrent;
}
bool MyAI::Referee_debug(const std::array<int, 32>& chess, const int from_location_no, const int to_location_no, const int UserId, int *fail_no)
{
	// int MessageNo = 0;
	bool IsCurrent = true;
	int from_chess_no = chess[from_location_no];
	int to_chess_no = chess[to_location_no];
	int from_row = from_location_no / 4;
	int to_row = to_location_no / 4;
	int from_col = from_location_no % 4;
	int to_col = to_location_no % 4;

	if(from_chess_no < 0 || ( to_chess_no < 0 && to_chess_no != CHESS_EMPTY) )
	{  
		// MessageNo = 1;
		//strcat(Message,"**no chess can move**");
		//strcat(Message,"**can't move darkchess**");
		IsCurrent = false;
		*fail_no = 0;
	}
	else if (from_chess_no >= 0 && from_chess_no / 7 != UserId)
	{
		// MessageNo = 2;
		//strcat(Message,"**not my chess**");
		IsCurrent = false;
		*fail_no = 1;
	}
	else if((from_chess_no / 7 == to_chess_no / 7) && to_chess_no >= 0)
	{
		// MessageNo = 3;
		//strcat(Message,"**can't eat my self**");
		IsCurrent = false;
		*fail_no = 2;
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
				// MessageNo = 4;
				//strcat(Message,"**gun can't eat opp without between one piece**");
				IsCurrent = false;
				*fail_no = 3;
			}
			else if(to_chess_no == CHESS_EMPTY)
			{
				// MessageNo = 5;
				//strcat(Message,"**gun can't eat opp without between one piece**");
				IsCurrent = false;
				*fail_no = 4;
			}
		}
		//slide
		else
		{
			// MessageNo = 6;
			//strcat(Message,"**cant slide**");
			IsCurrent = false;
			*fail_no = 5;
		}
	}
	else // non gun
	{
		//judge pawn or king

		//distance
		if( abs(from_row-to_row) + abs(from_col-to_col)  > 1)
		{
			// MessageNo = 7;
			//strcat(Message,"**cant eat**");
			IsCurrent = false;
			*fail_no = 6;
		}
		//judge pawn
		else if(from_chess_no % 7 == 0)
		{
			if(to_chess_no % 7 != 0 && to_chess_no % 7 != 6)
			{
				// MessageNo = 8;
				//strcat(Message,"**pawn only eat pawn and king**");
				IsCurrent = false;
				*fail_no = 7;
			}
		}
		//judge king
		else if(from_chess_no % 7 == 6 && to_chess_no % 7 == 0)
		{
			// MessageNo = 9;
			//strcat(Message,"**king can't eat pawn**");
			IsCurrent = false;
			*fail_no = 8;
		}
		else if(from_chess_no % 7 < to_chess_no% 7)
		{
			// MessageNo = 10;
			//strcat(Message,"**cant eat**");
			IsCurrent = false;
			*fail_no = 9;
		}
	}
	return IsCurrent;
}


double evalColor(const ChessBoard *chessboard, const vector<MoveInfo> &Moves, const int color)
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
	const double w_mob = 1/6;
	const double max_mobi = 60;
#else
	const double w_mob = 1/((810 + king_add_n_pawn[0])*3 + 270*2 + 180);
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

	if(legal_move_count == 0){ // Win, Lose
		if(color == this->Color){ // Lose
			score += LOSE - WIN;
		}else{ // Win
			score += WIN - LOSE;
		}
		finish = true;
	}else if(isDraw(chessboard)){ // Draw
		// score += DRAW - DRAW;
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

double MyAI::Nega_scout(const ChessBoard chessboard, const key128_t& boardkey, MoveInfo& move, const int color, const int depth, const int remain_depth, double alpha, double beta){

	// check table
	TransPosition& table = this->transTable;
	TableEntry entry;
#ifdef USE_TRANSPOSITION
	if(color < 2 && table.query(boardkey, color, entry)){
		// hash hit
		// if (entry.depth >= depth){
		if (entry.rdepth >= remain_depth){
			if (entry.vtype == ENTRY_EXACT){
				MoveInfo& mv = entry.child_move;
				int fail_no;
				if((mv.from_location_no != mv.to_location_no) && 
				!Referee_debug(chessboard.Board, mv.from_location_no, mv.to_location_no, color, &fail_no)){
					Pirnf_Chessboard();
					printf("move: %d->%d, fail: %d\n", mv.from_location_no, mv.to_location_no, fail_no);
					fflush(stdout);
					exit(1);
				}
				move = mv;
				return entry.value;
			}
			else if (entry.vtype == ENTRY_LOWER){
				alpha = max(alpha, entry.value);
			}
			else{
				beta = min(beta, entry.value);
			}
		}
	}
#endif

	int move_count = 0, flip_count = 0;

	vector<MoveInfo> Moves;	
	Moves.reserve(128);

	// move
	Expand(&chessboard, color, Moves);
	move_count = Moves.size();
	moveOrdering(boardkey, Moves, depth);
	
	// flip
	// first check whether should do flip
	vector<int> Choice;
	Choice.reserve(16);

	if (remain_depth >= 3){
		int n_flips_before = 0;
		int hist_size = chessboard.History.size();
		for(int i = hist_size - 1; i >= max(hist_size - depth, 0); i--){
			int mv = chessboard.History.at(i);
			if (mv / 100 == mv % 100){
				n_flips_before++;
				if (hist_size - i < 4){
					n_flips_before = MAX_FLIPS_IN_SEARCH;
					break;
				}
			}
		}
		if (n_flips_before < MAX_FLIPS_IN_SEARCH){
			for(int j = 0; j < 14; j++){ // find remain chess
				if(chessboard.CoverChess[j] > 0){
					Choice.push_back(j);
				}
			}
			for (int i = chessboard.Heads[2]; i != -1; i = chessboard.Next[i]){
				assert(chessboard.Board[i] == CHESS_COVER);
				Moves.emplace_back(chessboard.Board, i, i);
			}
			flip_count = Moves.size() - move_count;
		}
	}

	if(isTimeUp() || // time is up
		remain_depth <= 0 || // reach limit of depth 
		chessboard.Chess_Nums[RED] == 0 || // terminal node (no chess type)
		chessboard.Chess_Nums[BLACK] == 0 || // terminal node (no chess type)
		move_count+flip_count == 0 || // terminal node (no move type)
		isDraw(&chessboard) // draw
		){
		this->node++;
		// odd: *-1, even: *1
		return Evaluate(&chessboard, Moves, color) * (depth&1 ? -1 : 1);
	}else{
		double m = -DBL_MAX;
		double n = beta;
		MoveInfo new_move;
		// search deeper
		for(int i = 0; i < move_count; i++){ // move
			ChessBoard new_chessboard = chessboard;
			MakeMove(&new_chessboard, Moves[i].num, 0); // 0: dummy
			key128_t new_boardkey = table.MakeMove(boardkey, Moves[i], 0);
			double t = -Nega_scout(new_chessboard, new_boardkey, new_move, color^1, depth+1, remain_depth-1, -n, -max(alpha, m));
			bool draw_penalize = (
				(depth == 0) &&
				(!new_chessboard.cantWin[this->Color]) &&
				isDraw(&new_chessboard));
			if (draw_penalize){
				t -= WIN;
			}
			if(t > m){ 
				if (n == beta || remain_depth < 3 || t >= beta){
					m = t;
					move = Moves[i];
				}
				else{
					m = -Nega_scout(new_chessboard, new_boardkey, new_move, color^1, depth+1, remain_depth-1, -beta, -t);
					if (draw_penalize)
						m -= WIN;
					move = Moves[i];
				}
			}
			if (m >= beta){
				// record the hash entry as a lower bound
#ifdef USE_TRANSPOSITION
				if (color < 2){
					entry.value = m;
					// entry.depth = depth;
					entry.rdepth = remain_depth;
					entry.vtype = ENTRY_LOWER;
					entry.child_move = Moves[i];
					table.insert(boardkey, color, entry);
				}
#endif
				return m;
			}
			n = max(alpha, m) + 1;
		}

		// chance nodes
		for(int i = move_count; i < move_count + flip_count; i++){ // flip
			// calculate the expect value of flip
			double t = Star0_EQU(chessboard, boardkey, Moves[i], Choice, color, depth, remain_depth);

			if(t > m){ 
				m = t;
				move = Moves[i];
			}
			if (m >= beta){
				// record the hash entry as a lower bound
#ifdef USE_TRANSPOSITION
				if (color < 2){
					entry.value = m;
					// entry.depth = depth;
					entry.rdepth = remain_depth;
					entry.vtype = ENTRY_LOWER;
					entry.child_move = Moves[i];
					table.insert(boardkey, color, entry);
				}
#endif
				return m;
			}
		}

#ifdef USE_TRANSPOSITION
		if (color < 2){
			if (m > alpha){
				// record the hash entry as an exact value m
				entry.vtype = ENTRY_EXACT;
			}
			else{
				// record the hash entry as an upper bound m;
				entry.vtype = ENTRY_UPPER;
			}
			entry.value = m;
			// entry.depth = depth;
			entry.rdepth = remain_depth;
			entry.child_move = move;
			table.insert(boardkey, color, entry);
		}
#endif
		return m;
	}
}

double MyAI::Star0_EQU(const ChessBoard& chessboard, const key128_t& boardkey, const MoveInfo& move, const vector<int>& Choice, const int color, const int depth, const int remain_depth){
	TransPosition& table = this->transTable;
	MoveInfo new_move;
	double total = 0;
	for(auto& k: Choice){
		ChessBoard new_chessboard = chessboard;
		key128_t new_boardkey = table.MakeMove(boardkey, move, k);
		MakeMove(&new_chessboard, move.num, k);

		int next_col = (color == 2) ? ((k / 7)^1) : (color^1);

		double t = -Nega_scout(new_chessboard, new_boardkey, new_move, next_col, depth+1, remain_depth-3, -DBL_MAX, DBL_MAX);
		total += chessboard.CoverChess[k] * t;
	}
	assert(chessboard.Chess_Nums[2] > 0);
	return total / chessboard.Chess_Nums[2];
}


bool MyAI::isDraw(const ChessBoard* chessboard){
	// No Eat Flip
	if(chessboard->NoEatFlip >= NOEATFLIP_LIMIT){
		return true;
	}

	// Position Repetition
	int last_idx = chessboard->History.size() - 1;
	// -2: my previous ply
	int idx = last_idx - 2;
	// All ply must be move type
	int smallest_repetition_idx = last_idx - (chessboard->NoEatFlip / POSITION_REPETITION_LIMIT);
	// check loop
	while(idx >= 0 && idx >= smallest_repetition_idx){
		if(chessboard->History[idx] == chessboard->History[last_idx]){
			// how much ply compose one repetition
			int repetition_size = last_idx - idx;
			bool isEqual = true;
			for(int i = 1; i < POSITION_REPETITION_LIMIT && isEqual; ++i){
				for(int j = 0; j < repetition_size; ++j){
					int src_idx = last_idx - j;
					int checked_idx = last_idx - i*repetition_size - j;
					if(chessboard->History[src_idx] != chessboard->History[checked_idx]){
						isEqual = false;
						break;
					}
				}
			}
			if(isEqual){
				return true;
			}
		}
		idx -= 2;
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
	double elapsed; // ms
	
	// design for different os
#ifdef WINDOWS
	clock_t end = clock();
	elapsed = (end - origin);
#else
	struct timeval end;
	gettimeofday(&end, 0);
	long seconds = end.tv_sec - origin.tv_sec;
	long microseconds = end.tv_usec - origin.tv_usec;
	elapsed = (seconds*1000 + microseconds*1e-3);
#endif

	double real_exp_ply = max(EXPECT_PLYS, num_plys + 45 + this->main_chessboard.Chess_Nums[this->Color^1]);

	this->ply_time = min(MAX_PLY_TIME, (TOTAL_TIME - elapsed) / (real_exp_ply - num_plys + 1));
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



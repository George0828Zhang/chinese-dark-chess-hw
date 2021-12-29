#include "float.h"

#include "MyAI.h"
#include <algorithm>
#include <cassert>

// #define NOASSERT
#ifdef NOASSERT
#undef assert
#define assert(x) ((void)0)
#endif

#define MAX_DEPTH 3

#define WIN 1.0
#define DRAW 0.2
#define LOSE 0.0
#define BONUS 0.3
#define BONUS_MAX_PIECE 8

#define OFFSET (WIN + BONUS)

#define NOEATFLIP_LIMIT 60
#define POSITION_REPETITION_LIMIT 3

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
	main_chessboard.Chess_Nums.fill(16);
	main_chessboard.Heads.fill(-1);
	main_chessboard.NoEatFlip = 0;

	main_chessboard.Board.fill(CHESS_COVER);
	main_chessboard.Prev.fill(-1);
	main_chessboard.Next.fill(-1);

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

	// iterative-deeping, start from 3, time limit = <TIME_LIMIT> sec
	this->node = 0;
	int best_move;

	// run Nega-max
	t = Nega_scout(this->main_chessboard, &best_move, this->Color, 0, MAX_DEPTH, -DBL_MAX, DBL_MAX);
	t -= OFFSET; // rescale

	// replace the move and score
	StartPoint = best_move/100;
	EndPoint   = best_move%100;
	sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));
	fprintf(stderr, "Depth: %2d, Node: %10d, Score: %+1.5lf, Move: %s\n",  
		MAX_DEPTH, node, t, move);
	fflush(stderr);
	
	char chess_Start[4]="";
	char chess_End[4]="";
	Pirnf_Chess(main_chessboard.Board[StartPoint],chess_Start);
	Pirnf_Chess(main_chessboard.Board[EndPoint],chess_End);
	printf("My result: \n--------------------------\n");
	printf("Nega max: %lf (node: %d)\n", t, this->node);
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
	if (chessboard->Heads[0] == at)
		chessboard->Heads[0] = right;
	if (chessboard->Heads[1] == at)
		chessboard->Heads[1] = right;
}

void insertInBoard(ChessBoard* chessboard, const int at){
	assert(chessboard->Board[at] != CHESS_EMPTY);
	assert(chessboard->Board[at] != CHESS_COVER);
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
		if (chessboard->Heads[0] == src)
			chessboard->Heads[0] = dst;
		if (chessboard->Heads[1] == src)
			chessboard->Heads[1] = dst;
	}
	else{
		removeFromBoard(chessboard, src);
		insertInBoard(chessboard, dst);
	}
}

void MyAI::MakeMove(ChessBoard* chessboard, const int move, const int chess){
	int src = move/100, dst = move%100;
	if(src == dst){ // flip
		chessboard->Board[src] = chess;
		chessboard->CoverChess[chess]--;
		chessboard->NoEatFlip = 0;
		insertInBoard(chessboard, src);
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

int MyAI::Expand(const ChessBoard *chessboard, const int color,int *Result)
{
	if (color == 2) return 0;// initial board
	int n_chess = chessboard->Chess_Nums[color];
	const array<int, 32>& board = chessboard->Board;

	int ResultCount = 0;
	for (int i = chessboard->Heads[color]; i != -1; i = chessboard->Next[i]){
		n_chess--;
		// Cannon
		if(board[i] % 7 == 1)
		{
			int row = i/4;
			int col = i%4;
			for(int rowCount=row*4;rowCount<(row+1)*4;rowCount++)
			{
				if(Referee(board,i,rowCount,color))
				{
					Result[ResultCount] = i*100+rowCount;
					ResultCount++;
				}
			}
			for(int colCount=col; colCount<32;colCount += 4)
			{
			
				if(Referee(board,i,colCount,color))
				{
					Result[ResultCount] = i*100+colCount;
					ResultCount++;
				}
			}
		}
		else
		{
			int Move[4] = {i-4,i+1,i+4,i-1};
			for(int k=0; k<4;k++)
			{
				
				if(Move[k] >= 0 && Move[k] < 32 && Referee(board,i,Move[k],color))
				{
					Result[ResultCount] = i*100+Move[k];
					ResultCount++;
				}
			}
		}
	}
	// n_chess should be num covered
	for(int i = 0; i < 7; i++){
		n_chess -= chessboard->CoverChess[color*7 + i];
	}
	assert(n_chess == 0);
	return ResultCount;
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

	if(from_chess_no < 0 || ( to_chess_no < 0 && to_chess_no != CHESS_EMPTY) )
	{  
		// MessageNo = 1;
		//strcat(Message,"**no chess can move**");
		//strcat(Message,"**can't move darkchess**");
		IsCurrent = false;
	}
	else if (from_chess_no >= 0 && from_chess_no / 7 != UserId)
	{
		// MessageNo = 2;
		//strcat(Message,"**not my chess**");
		IsCurrent = false;
	}
	else if((from_chess_no / 7 == to_chess_no / 7) && to_chess_no >= 0)
	{
		// MessageNo = 3;
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
		//slide
		else
		{
			// MessageNo = 6;
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
			// MessageNo = 7;
			//strcat(Message,"**cant eat**");
			IsCurrent = false;
		}
		//judge pawn
		else if(from_chess_no % 7 == 0)
		{
			if(to_chess_no % 7 != 0 && to_chess_no % 7 != 6)
			{
				// MessageNo = 8;
				//strcat(Message,"**pawn only eat pawn and king**");
				IsCurrent = false;
			}
		}
		//judge king
		else if(from_chess_no % 7 == 6 && to_chess_no % 7 == 0)
		{
			// MessageNo = 9;
			//strcat(Message,"**king can't eat pawn**");
			IsCurrent = false;
		}
		else if(from_chess_no % 7 < to_chess_no% 7)
		{
			// MessageNo = 10;
			//strcat(Message,"**cant eat**");
			IsCurrent = false;
		}
	}
	return IsCurrent;
}

// always use my point of view, so use this->Color
double MyAI::Evaluate(const ChessBoard* chessboard, 
	const int legal_move_count, const int color){
	// score = My Score - Opponent's Score
	// offset = <WIN + BONUS> to let score always not less than zero

	double score = OFFSET;
	bool finish;

	if(legal_move_count == 0){ // Win, Lose
		if(color == this->Color){ // Lose
			score += LOSE - WIN;
		}else{ // Win
			score += WIN - LOSE;
		}
		finish = true;
	}else if(isDraw(chessboard)){ // Draw
		// score += DRAW - DRAW;
		finish = true;
	}else{ // no conclusion
		// static material values
		// cover and empty are both zero
		static const double values[14] = {
			  1,180,  6, 18, 90,270,810,  
			  1,180,  6, 18, 90,270,810
		};
		
		double piece_value = 0;
		for(int i = 0; i < 32; i++){
			if(chessboard->Board[i] != CHESS_EMPTY && 
				chessboard->Board[i] != CHESS_COVER){
				if(chessboard->Board[i] / 7 == this->Color){
					piece_value += values[chessboard->Board[i]];
				}else{
					piece_value -= values[chessboard->Board[i]];
				}
			}
		}
		// linear map to (-<WIN>, <WIN>)
		// score max value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 = 1943
		// <ORIGINAL_SCORE> / <ORIGINAL_SCORE_MAX_VALUE> * (<WIN> - 0.01)
		piece_value = piece_value / 1943 * (WIN - 0.01);
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

double MyAI::Nega_scout(const ChessBoard chessboard, int* move, const int color, const int depth, const int remain_depth, const double alpha, const double beta){

	int Moves[2048], Chess[2048];
	int move_count = 0, flip_count = 0, remain_count = 0, remain_total = 0;

	// move
	move_count = Expand(&chessboard, color, Moves);
	// flip
	for(int j = 0; j < 14; j++){ // find remain chess
		if(chessboard.CoverChess[j] > 0){
			Chess[remain_count] = j;
			remain_count++;
			remain_total += chessboard.CoverChess[j];
		}
	}
	for(int i = 0; i < 32; i++){ // find cover
		if(chessboard.Board[i] == CHESS_COVER){
			Moves[move_count + flip_count] = i*100+i;
			flip_count++;
		}
	}

	if(remain_depth <= 0 || // reach limit of depth
		chessboard.Chess_Nums[RED] == 0 || // terminal node (no chess type)
		chessboard.Chess_Nums[BLACK] == 0 || // terminal node (no chess type)
		move_count+flip_count == 0 // terminal node (no move type)
		){
		this->node++;
		// odd: *-1, even: *1
		return Evaluate(&chessboard, move_count+flip_count, color) * (depth&1 ? -1 : 1);
	}else{
		double m = -DBL_MAX;
		double n = beta;
		int new_move;
		// search deeper
		for(int i = 0; i < move_count; i++){ // move
			ChessBoard new_chessboard = chessboard;
			
			MakeMove(&new_chessboard, Moves[i], 0); // 0: dummy
			double t = -Nega_scout(new_chessboard, &new_move, color^1, depth+1, remain_depth-1, -n, -max(alpha, m));
			if(t > m){ 
				if (n == beta || remain_depth < 3 || t >= beta){
					m = t;
					*move = Moves[i];
				}
				else{
					m = -Nega_scout(new_chessboard, &new_move, color^1, depth+1, remain_depth-1, -beta, -t);
					*move = Moves[i];
				}
			}
			if (m >= beta) return m; 
			n = max(alpha, m) + 1;
		}

		// chance nodes
		for(int i = move_count; i < move_count + flip_count; i++){ // flip
			// calculate the expect value of flip
			double t = Star0_EQU(chessboard, Moves[i], Chess, remain_count, remain_total, color, depth, remain_depth); // , -beta, -max(alpha, m)

			if(t > m){ 
				m = t;
				*move = Moves[i];
			}
			if (m >= beta) return m;
		}
		return m;
	}
}

double MyAI::Star0_EQU(const ChessBoard& chessboard, int move, const int* Chess, const int remain_count, const int remain_total, const int color, const int depth, const int remain_depth){
	int new_move;
	double total = 0;
	for(int k = 0; k < remain_count; k++){
		ChessBoard new_chessboard = chessboard;
		
		MakeMove(&new_chessboard, move, Chess[k]);

		int next_col = (color == 2) ? ((Chess[k] / 7)^1) : (color^1);

		double t = -Nega_scout(new_chessboard, &new_move, next_col, depth+1, remain_depth-1, -DBL_MAX, DBL_MAX);
		total += chessboard.CoverChess[Chess[k]] * t;
	}
	return total / remain_total;
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


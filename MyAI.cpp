#include "float.h"

#ifdef WINDOWS
#include <time.h>
#else
#include <sys/time.h>
#endif
#include "MyAI.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cassert>

#define TIME_LIMIT 9.5

#define WIN 1.0
#define DRAW 0.2
#define LOSE 0.0
#define BONUS 0.3
// #define WIN 100000
// #define BONUS 30000
#define BONUS_MAX_PIECE 8

#define OFFSET (WIN + BONUS)

#define NOEATFLIP_LIMIT 60
#define POSITION_REPETITION_LIMIT 3

int rand_seed;

MoveInfo::MoveInfo(){}
MoveInfo::MoveInfo(const int *board, int from, int to)
{
	from_location_no = from;
	to_location_no = to;
	from_chess_no = board[from];
	to_chess_no = board[to];
}
inline int MoveInfo::num()
{
	return from_location_no * 100 + to_location_no;
}
inline bool cantWinCheck(const ChessBoard *chessboard, const int color);

MyAI::MyAI(void){}

MyAI::~MyAI(void){}

bool MyAI::protocol_version(const char* data[], char* response){
	strcpy(response, "1.0.0");
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
	int iPieceCount[14] = {5, 2, 2, 2, 2, 2, 1, 5, 2, 2, 2, 2, 2, 1};
	memcpy(main_chessboard.CoverChess, iPieceCount, sizeof(int) * 14);
	memcpy(main_chessboard.AliveChess, iPieceCount, sizeof(int) * 14);
	main_chessboard.Red_Chess_Num = 16;
	main_chessboard.Black_Chess_Num = 16;
	main_chessboard.NoEatFlip = 0;
	// main_chessboard.HistoryCount = 0;
	// main_chessboard.red_chess_loc = std::vector<int>();
	// main_chessboard.black_chess_loc = std::vector<int>();
	main_chessboard.RedHead = -1;
	main_chessboard.BlackHead = -1;
	// memset(main_chessboard.Footprint, 0, sizeof(int) * FOOTPRINTSZ);
	// memset(main_chessboard.Timestamp, -1, sizeof(int) * FOOTPRINTSZ);
	main_chessboard.cantWin[0] = false;
	main_chessboard.cantWin[1] = false;

	//convert to my format
	int Index = 0;
	for(int i=0;i<8;i++)
	{
		for(int j=0;j<4;j++)
		{
			main_chessboard.Board[Index] = CHESS_COVER;
			main_chessboard.Prev[Index] = -1;
			main_chessboard.Next[Index] = -1;			
			Index++;
		}
	}
	Pirnf_Chessboard();
}

void MyAI::generateMove(char move[6])
{
	/* generateMove Call by reference: change src,dst */
	int StartPoint = 0;
	int EndPoint = 0;

	double t = -DBL_MAX;
#ifdef WINDOWS
	begin = clock();
#else
	gettimeofday(&begin, 0);
#endif

	// iterative-deeping, start from 3, time limit = <TIME_LIMIT> sec
	for(int depth = 3; !isTimeUp(); depth+=2){
		this->node = 0;
		int best_move_tmp; double t_tmp;

		// run Nega-max
		t_tmp = Nega_max(this->main_chessboard, &best_move_tmp, this->Color, 0, depth, -DBL_MAX, DBL_MAX);
		t_tmp -= OFFSET; // rescale

		// check score
		// if search all nodes
		// replace the move and score
		if(!this->timeIsUp){
			t = t_tmp;
			StartPoint = best_move_tmp/100;
			EndPoint   = best_move_tmp%100;
			sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));
		}
		// U: Undone
		// D: Done
		fprintf(stderr, "[%c] Depth: %2d, Node: %10d, Score: %+1.5lf, Move: %s\n", (this->timeIsUp ? 'U' : 'D'), 
			depth, node, t, move);
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
	printf("Nega max: %lf (node: %d)\n", t, this->node);
	printf("(%d) -> (%d)\n",StartPoint,EndPoint);
	printf("<%s> -> <%s>\n",chess_Start,chess_End);
	printf("move:%s\n",move);
	printf("--------------------------\n");
	this->Pirnf_Chessboard();
}

void printVec(const std::vector<int> &vec){
	fprintf(stderr, ">");
	for (auto& i: vec)
		fprintf(stderr, "%2d ", i);
	fprintf(stderr, "\n");
}

void removeFromBoard(ChessBoard* chessboard, int at){
	int left = chessboard->Prev[at];
	int right = chessboard->Next[at];
	if (left != -1)
		chessboard->Next[left] = right;
	if (right != -1)
		chessboard->Prev[right] = left;
	chessboard->Prev[at] = -1;
	chessboard->Next[at] = -1;
	if (chessboard->RedHead == at)
		chessboard->RedHead = right;
	if (chessboard->BlackHead == at)
		chessboard->BlackHead = right;
}

void insertInBoard(ChessBoard* chessboard, int at){
	int color = chessboard->Board[at] / 7;
	if (color == RED){
		if (chessboard->RedHead == -1 || chessboard->RedHead > at)
			chessboard->RedHead = at;
	}
	else{
		if (chessboard->BlackHead == -1 || chessboard->BlackHead > at)
			chessboard->BlackHead = at;
	}

	int left = -1, right = -1;
	for(int i = at+1; i < 32; i++){
		if (chessboard->Board[i] >= 0 && chessboard->Board[i] / 7 == color){
			right = i;
			break;
		}
	}
	for(int i = at-1; i >= 0; i--){
		if (chessboard->Board[i] >= 0 && chessboard->Board[i] / 7 == color){
			left = i;
			break;
		}
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
				(chessboard->Red_Chess_Num)--;
			}else{ // black
				(chessboard->Black_Chess_Num)--;
			}
			chessboard->AliveChess[chessboard->Board[dst]]--;
			removeFromBoard(chessboard, dst);
			chessboard->NoEatFlip = 0;
		}else{
			chessboard->NoEatFlip += 1;
		}
		chessboard->Board[dst] = chessboard->Board[src];
		chessboard->Board[src] = CHESS_EMPTY;
		removeFromBoard(chessboard, src);
		insertInBoard(chessboard, dst);
	}
	chessboard->History.push_back(move);
	// chessboard->RepeatAt.push_back(chessboard->Footprint[move]);
	// chessboard->Timestamp[move] = chessboard->History.size() - 1;
	// chessboard->Footprint[move]++;
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

void MyAI::Expand(const ChessBoard *chessboard, const int color, std::vector<MoveInfo> &Result)
{
	int i = (color == RED) ? chessboard->RedHead : chessboard->BlackHead;
	const int *board = chessboard->Board;

	for (; i != -1; i = chessboard->Next[i]){
		// Cannon
		if(board[i] % 7 == 1)
		{
			int row = i/4;
			int col = i%4;
			for(int rowCount=row*4;rowCount<(row+1)*4;rowCount++)
			{
				if(Referee(board,i,rowCount,color))
				{
					Result.push_back(MoveInfo(board, i, rowCount));
				}
			}
			for(int colCount=col; colCount<32;colCount += 4)
			{

				if(Referee(board,i,colCount,color))
				{
					Result.push_back(MoveInfo(board, i, colCount));
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
					Result.push_back(MoveInfo(board, i, Move[k]));
				}
			}
		}
	}
}


// Referee
bool MyAI::Referee(const int* chess, const int from_location_no, const int to_location_no, const int UserId)
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

void debug_Chess(int chess_no, char *Result)
{
	// XX -> Empty
	if (chess_no == CHESS_EMPTY)
	{
		strcat(Result, " - ");
		return;
	}
	// OO -> DarkChess
	else if (chess_no == CHESS_COVER)
	{
		strcat(Result, " X ");
		return;
	}

	switch (chess_no)
	{
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

//Display chess board
void debug_Chessboard(const ChessBoard *chessboard, const int color)
{
	char Mes[1024] = "";
	char temp[1024];
	char myColor[10] = "";
	if (color == -99)
		strcpy(myColor, "Unknown");
	else if (color == RED)
		strcpy(myColor, "Red");
	else
		strcpy(myColor, "Black");
	sprintf(temp, "------------%s-------------\n", myColor);
	strcat(Mes, temp);
	strcat(Mes, "<8> ");
	for (int i = 0; i < 32; i++)
	{
		if (i != 0 && i % 4 == 0)
		{
			sprintf(temp, "\n<%d> ", 8 - (i / 4));
			strcat(Mes, temp);
		}
		char chess_name[4] = "";
		debug_Chess(chessboard->Board[i], chess_name);
		sprintf(temp, "%5s", chess_name);
		strcat(Mes, temp);
	}
	strcat(Mes, "\n\n     ");
	for (int i = 0; i < 4; i++)
	{
		sprintf(temp, " <%c> ", 'a' + i);
		strcat(Mes, temp);
	}
	strcat(Mes, "\n\n");
	fprintf(stderr, "%s", Mes);
}
inline bool cantWinCheck(const ChessBoard *chessboard, const int color){
	if (chessboard->cantWin[color])
		return true; // cant once, cant forever
	if (chessboard->AliveChess[color * 7 + 1] > 0)
		return false; // i have cannon might win
	int op_color = color^1;
	if ((op_color == RED && chessboard->Red_Chess_Num == 0) &&
		(op_color == BLACK && chessboard->Black_Chess_Num == 0))
		return false; // opponent loses i win
	int offset = color * 7;
	int myCumsumAlive[7], cumsum = 0;
	for (int i = 6; i >= 0; i--)
	{
		cumsum += chessboard->AliveChess[offset + i];
		myCumsumAlive[i] = cumsum;
	}
	myCumsumAlive[0] -= chessboard->AliveChess[offset + 6];
	myCumsumAlive[6] += chessboard->AliveChess[offset + 0];

	offset = op_color * 7;
	for(int i = 0; i < 7; i++){
		if (chessboard->AliveChess[offset + i] > 0 && myCumsumAlive[i] < 1){
			return true;
		}
	}
	return false;
}

double evalColor(const ChessBoard *chessboard, const std::vector<MoveInfo> &Moves, const int color)
{
	/* 
	Need to return value between 0 ~ +1 
	static material values
	cover and empty are both zero
	max value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 = 1943

	max mobility = 12 + 12 + 14 + 14 = 52 (can right + can left + up + down, ignore cannon.)
	if cannon: +(-2+8)*2 (cannon is best at corner (?), =8)
		= 64
	*/
	double values[14] = {
		1, 180, 6, 18, 90, 270, 810,
		1, 180, 6, 18, 90, 270, 810};
	double king_add_n_pawn[] = {  381, 111, 22, 5, 0, 0 }; // if eat a pawn, my king adds this much to its value
	// 5->4 : 0
	// 4->3 : 5
	// 3->2 : 17
	// 2->1 : 80
	// 1->0 : 260 
	double mobilities[32] = {0};
	for(auto& m: Moves){
		mobilities[m.from_location_no]++;
	}

	// for(int c = 0; c < 2; c++){
	// 	int op_king = (!c) * 7 + 6;
	// 	int my_pawn = c * 7 + 0;
	// 	if (chessboard->AliveChess[op_king] > 0){
	// 		if (chessboard->AliveChess[my_pawn] == 1)
	// 			values[my_pawn] = 810;
	// 		else if (chessboard->AliveChess[my_pawn] == 2)
	// 			values[my_pawn] = 410;
	// 	}
	// }

	for(int c = 0; c < 2; c++){
		int op_king = (!c) * 7 + 6;
		int my_pawn = c * 7 + 0;
		int n = chessboard->AliveChess[my_pawn];
		values[op_king] += king_add_n_pawn[n];
	}

	double max_value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 + king_add_n_pawn[0];
	// static const double w_mob = 0.5; //4.0/(max_value+4.0);
	static const double w_mob = 0.34;


	double value = 0;
	int head = (color == RED) ? chessboard->RedHead : chessboard->BlackHead;
	for (int i = head; i != -1; i = chessboard->Next[i])
	{
		value += values[chessboard->Board[i]] + mobilities[i] * w_mob;// max delta mobi = 4 - 1 = 3
		// value += values[chessboard->Board[i]] * (1.0-w_mob) / max_value + mobilities[i] * w_mob / 64;
	}
	return value / (max_value + 64*w_mob);
}

// always use my point of view, so use this->Color
double MyAI::Evaluate(const ChessBoard* chessboard, 
	const std::vector<MoveInfo>& Moves, const int color){
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
		// score = DRAW - DRAW;
		if (!cantWinCheck(chessboard, color))
			score -= WIN; // if win is possible, dont draw
		finish = true;
	}else{ // no conclusion
		// static material values
		// cover and empty are both zero
		// static const double values[14] = {
		// 	  1,180,  6, 18, 90,270,810,
		// 	  1,180,  6, 18, 90,270,810
		// };
		
		// Material
		std::vector<MoveInfo> otherMoves;

		Expand(chessboard, (color == RED) ? BLACK : RED, otherMoves);
		
		double piece_value;
		double red_value = evalColor(chessboard, (color == RED) ? Moves : otherMoves, RED);
		double black_value = evalColor(chessboard, (color == BLACK) ? Moves : otherMoves, BLACK);
		piece_value = (red_value - black_value) * (this->Color == RED ? 1 : -1);

		// for(int i = 0; i < 32; i++){
		// 	if(chessboard->Board[i] != CHESS_EMPTY && 
		// 		chessboard->Board[i] != CHESS_COVER){
		// 		if(chessboard->Board[i] / 7 == this->Color){
		// 			piece_value += values[chessboard->Board[i]];
		// 		}else{
		// 			piece_value -= values[chessboard->Board[i]];
		// 		}
		// 	}
		// }

				// linear map to (-<WIN>, <WIN>)
		// score max value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 = 1943
		// <ORIGINAL_SCORE> / <ORIGINAL_SCORE_MAX_VALUE> * (<WIN> - 0.01)
		piece_value = piece_value / 1.0 * (WIN - 0.01);

		score += piece_value; 
		finish = false;
	}

	// Bonus (Only Win / Draw)
	if(finish){
		if((this->Color == RED && chessboard->Red_Chess_Num > chessboard->Black_Chess_Num) ||
			(this->Color == BLACK && chessboard->Red_Chess_Num < chessboard->Black_Chess_Num)){
			if(!(legal_move_count == 0 && color == this->Color)){ // except Lose
				double bonus = BONUS / BONUS_MAX_PIECE * 
				abs(chessboard->Red_Chess_Num - chessboard->Black_Chess_Num);
				if(bonus > BONUS){ bonus = BONUS; } // limit
				score += bonus;
			}
		}else if((this->Color == RED && chessboard->Red_Chess_Num < chessboard->Black_Chess_Num) ||
			(this->Color == BLACK && chessboard->Red_Chess_Num > chessboard->Black_Chess_Num)){
			if(!(legal_move_count == 0 && color != this->Color)){ // except Lose
				double bonus = BONUS / BONUS_MAX_PIECE * 
				abs(chessboard->Red_Chess_Num - chessboard->Black_Chess_Num);
				if(bonus > BONUS){ bonus = BONUS; } // limit
				score -= bonus;
			}
		}
	}
	
	return score;
}

static inline bool movecomp(const MoveInfo &a, const MoveInfo &b){
	/* 
	returns ​true if the first argument is ordered before the second.
	1. CHESS_EMPTY => -2 mod 7 is still -2
	2. 'to' bigger the better, more important
	3. 'from' smaller the better, less important
	When both no eat, random permute
	BUG when flip?
	*/
	int a_from = a.from_chess_no % 7;
	int b_from = b.from_chess_no % 7;
	int a_to = a.to_chess_no % 7;
	int b_to = b.to_chess_no % 7;
	int a_mask = a.to_chess_no >= 0;
	int b_mask = b.to_chess_no >= 0;
	int noise = ((rand_seed*(1+abs(a_from * a_to + b_from * b_to))) % 100) > 49;
	return (a_to * 10 + (6 - a_from) * a_mask) * 2 + noise > (b_to * 10 + (6 - b_from) * b_mask) * 2;
}
static inline void insertion_sort_shuffle(std::vector<MoveInfo>& Moves){
	/* small array: insertion sort is ok
	*/
	rand_seed = rand();
	for(unsigned int i = 0; i < Moves.size(); i++){
		for (unsigned int j = i; j > 0; j--){
			if (movecomp(Moves[j], Moves[j - 1])){
				std::swap(Moves[j], Moves[j - 1]);
			} else break;
		}
	}
}
double MyAI::SEE(const ChessBoard *chessboard, const int position, const int color){
	if(!(position >= 0 && position < 32) ||
	((chessboard->Board[position] / 7) == color)) // last move is flip
		return 0;
	
	// assert(chessboard->Board[position] >= 0);

	std::vector<int> RedCands, BlackCands;
	int board[32];
	memcpy(board, chessboard->Board, sizeof(int)*32);
	for(int i = chessboard->RedHead; i != -1; i = chessboard->Next[i]){
		if (board[i] % 7 == 1 || // Cannon
			(i == position - 4) || // neighbors
			(i == position - 1) ||
			(i == position + 4) ||
			(i == position + 1)){
				// assert(board[i] >= 0 && board[i] < 14);
				RedCands.push_back(i);
		}
	}
	for(int i = chessboard->BlackHead; i != -1; i = chessboard->Next[i]){
		if (board[i] % 7 == 1 || // Cannon
			(i == position - 4) || // neighbors
			(i == position - 1) ||
			(i == position + 4) ||
			(i == position + 1)){
				// assert(board[i] >= 0 && board[i] < 14);
				BlackCands.push_back(i);
		}
	}

	static const double values[14] = {
		1,180,  6, 18, 90,270,810,
		1,180,  6, 18, 90,270,810
	};
	auto comp = [board] (int const& a, int const& b) -> bool
    {
       return values[board[a]] > values[board[b]]; // compare chess values
    };
	if (RedCands.size() > 0)
		std::sort(RedCands.begin(), RedCands.end(), comp);
	if (BlackCands.size() > 0)
		std::sort(BlackCands.begin(), BlackCands.end(), comp);
	// sort descending, will be accessed from the back

	// for(int i = 1; i < RedCands.size(); i++){
	// 	assert(values[board[RedCands[i]]] <= values[board[RedCands[i]]]);
	// }
	// for(int i = 1; i < BlackCands.size(); i++){
	// 	assert(values[board[BlackCands[i]]] <= values[board[BlackCands[i]]]);
	// }

	double gain = 0.0;
	// int piece = board[position]; // my piece
	int opcolor = color^1;
	std::vector<int> &mycand = color==RED ? RedCands : BlackCands;
	std::vector<int> &opcand = color==RED ? BlackCands : RedCands;

	int myturn = 1;
	while(opcand.size()>0 && mycand.size()>0){
		// Opponent
		if (myturn==0){
			int from = opcand.back();
			if (Referee(board, from, position, opcolor)){
				gain -= values[board[position]]; // capture piece
				board[position] = board[from]; // place at position
				board[from] = CHESS_EMPTY;
				myturn = 1;
			}
			opcand.pop_back(); // remove from cands
		}
		else{
			// my turn
			int from = mycand.back();
			if (Referee(board, from, position, color)){
				gain += values[board[position]]; // capture piece
				board[position] = board[from]; // place at position
				board[from] = CHESS_EMPTY;
				myturn = 0;
			}
			mycand.pop_back(); // remove from cands
		}
	}

	return gain;
}
double MyAI::Nega_max(const ChessBoard chessboard, int* move, const int color, const int depth, const int remain_depth, double alpha, double beta){
	std::vector<MoveInfo> Moves;
	// move
	Expand(&chessboard, color, Moves);
	int move_count = Moves.size();
	insertion_sort_shuffle(Moves);

	// src = move/100, dst = move%100
	int see_pos = chessboard.History.empty() ? -1 : (chessboard.History.back() % 100); 
	if(isTimeUp() || // time is up
		(remain_depth <= 0 && SEE(&chessboard, see_pos, color) <= 0) || // reach limit of depth and is quiescent (net gain < 0)
		chessboard.Red_Chess_Num == 0 || // terminal node (no chess type)
		chessboard.Black_Chess_Num == 0 || // terminal node (no chess type)
		move_count == 0 // terminal node (no move type)
		){
		this->node++;
		// odd: *-1, even: *1
		return Evaluate(&chessboard, Moves, color) * (depth & 1 ? -1 : 1);
	}else{
		double m = alpha; // initialize with alpha instead
		int new_move;
		// search deeper
		for(int i = 0; i < move_count; i++){ // move
			if (isTimeUp()) break;
			ChessBoard new_chessboard = chessboard;

			MakeMove(&new_chessboard, Moves[i].num(), 0); // 0: dummy
			double t = -Nega_max(new_chessboard, &new_move, color ^ 1, depth + 1, remain_depth - 1, -beta, -m); // nega max: flip sign of alpha and beta

			// check 'current' chess board for repetition
			// int prev_mv_id = chessboard.Timestamp[Moves[i].num()];
			// int prev_new_mv_id = chessboard.Timestamp[new_move];
			// int repeats = chessboard.Footprint[Moves[i].num()];
			if (color == this->Color &&
				// prev_mv_id != -1 &&
				// prev_new_mv_id != -1 &&
				// prev_new_mv_id - prev_mv_id == 1 &&
				// chessboard.History.size() - prev_mv_id <= 10 &&
				!cantWinCheck(&new_chessboard, color) && // if win is possible, dont draw
				isDraw(&new_chessboard)) // repetition in 10 steps
			{
				// heavy penalty for repetition on my side
				// t -= 0.5*pow(2.0, repeats-1); 
				t -= WIN;
			}
			if (t > m){
				m = t;
				*move = Moves[i].num();
			}
			if (m >= beta){
				return m;
			}
		}
		return m;
	}
}

bool MyAI::isDraw(const ChessBoard* chessboard){
	// No Eat Flip
	if(chessboard->NoEatFlip >= NOEATFLIP_LIMIT){
		return true;
	}

	// Position Repetition
	int last_idx = chessboard->History.size() - 1; //chessboard->HistoryCount - 1;
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

	this->timeIsUp = (elapsed >= TIME_LIMIT*1000);
	return this->timeIsUp;
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

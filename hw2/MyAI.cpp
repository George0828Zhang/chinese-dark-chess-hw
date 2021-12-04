#include "float.h"

#ifdef WINDOWS
#include <time.h>
#else
#include <sys/time.h>
#endif

#include "MyAI.h"
#include "MCTree.hpp"
#include <algorithm>
#include <array>
#include <deque>
#include <vector>
#include <cmath>
#include <cassert>

#define NOASSERT
#ifdef NOASSERT
#undef assert
#define assert(x) ((void)0)
#endif

// #define TIME_LIMIT 9.5
#define TIME_LIMIT 5.0

#define WIN 1.0
#define DRAW 0.2*WIN
#define LOSE 0.0*WIN
#define BONUS 0.3*WIN
#define MIDDLE 0.98

#define MAX_SCORE (WIN + BONUS)

#define NOEATFLIP_LIMIT 60
#define POSITION_REPETITION_LIMIT 3

#define SIMULATE_COUNT_PER_CHILD 10

MyAI::MyAI(void){
	pcg32_srandom_r(&this->rng, time(NULL) ^ (intptr_t)&printf, 
		(intptr_t)&this->Color);

	// // dirty (same schematic as above)
	// rng = pcg32(time(NULL) ^ (intptr_t)&printf, (intptr_t)&this->Color);
	
	// // recommended
	// pcg_extras::seed_seq_from<std::random_device> seed_source;
    // pcg32 rng(seed_source);
}

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
	const std::array<int, 14> iPieceCount{5, 2, 2, 2, 2, 2, 1, 5, 2, 2, 2, 2, 2, 1};
	main_chessboard.CoverChess = iPieceCount;
	main_chessboard.AliveChess = iPieceCount;
	main_chessboard.cantWin = {false , false};
	main_chessboard.Red_Chess_Num = 16;
	main_chessboard.Black_Chess_Num = 16;
	main_chessboard.RedHead = -1;
	main_chessboard.BlackHead = -1;
	main_chessboard.NoEatFlip = 0;

	main_chessboard.Board.fill(CHESS_COVER);
	main_chessboard.Prev.fill(-1);
	main_chessboard.Next.fill(-1);

	Pirnf_Chessboard();
}


void MyAI::fastForward(ChessBoard* chessboard, std::vector<int>& history){
	for(auto& m: history){
		MakeMove(chessboard, m, 0);
	}
}
void MyAI::generateMove(char move[6])
{
#ifdef WINDOWS
	begin = clock();
#else
	gettimeofday(&begin, 0);
#endif

	// int num_chess = this->main_chessboard.Red_Chess_Num + this->main_chessboard.Black_Chess_Num;
	// MCTree tree(num_chess < 16 ? 0.88 : 1.18, 1.0);
	MCTree tree(1.18, 1.0); // 0.23, 0.18
	int par = tree.expand(-1, -1); // depth = 0, current board.
	assert(par == 0);
	assert(tree.parent_id[0] == -1);
	assert(tree.depth[0] == 0);

	int prev_par = -1;
	while(!isTimeUp() && par != -1){
		int tcolor = this->Color ^ (tree.depth[par] % 2);		// depth even: my color
		ChessBoard root_chessboard = this->main_chessboard;		// copy
		fastForward(&root_chessboard, tree.histories[par]);
		// Expand
		std::deque<MoveInfo> Moves;
		Expand(&root_chessboard, tcolor, Moves, nullptr);
		if (Moves.size() == 0){
			// tcolor loses! go back 2 layers
			prev_par = par;
			par = tree.parent_id[par];
			if (par != -1) par = tree.parent_id[par];
			continue;
		}
		else if (tree.children_ids[par].size() == 0){
			// Expand
			for(auto& m: Moves){
				tree.expand(par, m.num());
			}
		}

		// clear propogation buffer
		tree.clear_stats_buff();
		for(auto& id: tree.children_ids[par]){
			assert(!tree.histories[id].empty());
			int move = tree.histories[id].back();
			ChessBoard new_chessboard = root_chessboard;		// copy
			MakeMove(&new_chessboard, move, 0);				// already made move, so next in simulate must be opponent.
			
			// run simulation
			double total_score = 0;
			double total_square = 0;
			for(int k = 0; k < SIMULATE_COUNT_PER_CHILD; ++k){
				double sc = Simulate(new_chessboard, tcolor, 60);
				total_score += sc;
				total_square += sc*sc;
			}
			tree.update_leaf(id, total_score, total_square, SIMULATE_COUNT_PER_CHILD);
		}
		tree.propogate(par);

		prev_par = par;
		par = tree.pv_leaf(0);
		if (par == prev_par && tree.n_trials[0] > 100000){
			fprintf(stderr, "[DEBUG] early stop depth: %d, score: %5lf, var: %5lf, trials: %d (%s)\n",
				tree.depth[par], tree.Average[par], tree.Variance[par], tree.n_trials[par],
				tcolor==this->Color ? "Lose" : "Win"
			);
			break;
		}
	}

	std::deque<MoveInfo> Moves;
	std::vector<double> Children_Scores;
	std::vector<double> Children_Vars;
	std::vector<int> Children_Trials;
	int best_child_id = 0;
	int max_depth = tree.max_depth;
	int sim_count = tree.n_trials[0];
	bool early_stop = par == -1 || prev_par == par;

	// children of first node is what we are after
	for(auto& i: tree.children_ids[0]){
		assert(tree.histories[i].size() == 1);
		assert(tree.depth[i] == 1);
		int move = tree.histories[i].front();
		double score = tree.Average[i];
		int trials = tree.n_trials[i];
		Moves.push_back(MoveInfo(this->main_chessboard.Board, move/100, move%100));
		Children_Scores.push_back(score);
		Children_Vars.push_back(tree.Variance[i]);
		Children_Trials.push_back(trials);
		if (score > Children_Scores[best_child_id] ||
			(score == Children_Scores[best_child_id] && trials > Children_Trials[best_child_id])
		){
			best_child_id = Children_Scores.size() - 1;
		}
	}

	// Log
	for(int i = 0; i < (int)Moves.size(); ++i){
		char tmp[6];
		int tmp_start = Moves[i].from_location_no;
		int tmp_end   = Moves[i].to_location_no;
		// change int to char
		sprintf(tmp, "%c%c-%c%c",'a'+(tmp_start%4),'1'+(7-tmp_start/4),'a'+(tmp_end%4),'1'+(7-tmp_end/4));

		fprintf(stderr, "%2d. Move: %s, Score: %+5lf, Var: %+5lf, Sim_Count: %7d\n",
			i+1, tmp, Children_Scores[i], Children_Vars[i], Children_Trials[i]);
		fflush(stderr);
	}
	char msg[256];
	sprintf(msg, "MCTS: %lf (total simulations: %d, tree depth: %d%s)\n", Children_Scores[best_child_id], sim_count, max_depth, early_stop ? ", early stopped" : "");
	fprintf(stderr, "%s", msg);

	// set return value
	int StartPoint = Moves[best_child_id].from_location_no;
	int EndPoint = Moves[best_child_id].to_location_no;
	sprintf(move, "%c%c-%c%c",'a'+(StartPoint%4),'1'+(7-StartPoint/4),'a'+(EndPoint%4),'1'+(7-EndPoint/4));
	
	
	char chess_Start[4]="";
	char chess_End[4]="";
	Pirnf_Chess(main_chessboard.Board[StartPoint], chess_Start);
	Pirnf_Chess(main_chessboard.Board[EndPoint], chess_End);
	printf("My result: \n--------------------------\n");
	printf("%s", msg);
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
	if (chessboard->RedHead == at)
		chessboard->RedHead = right;
	if (chessboard->BlackHead == at)
		chessboard->BlackHead = right;
}

void insertInBoard(ChessBoard* chessboard, const int at){
	/*
	> 8 : --> use incremental
	<= 8: --> use list
	 */
	const int threshold = 8;
	int color = chessboard->Board[at] / 7;
	int num_chess;
	int old_head;
	int cur_head;
	if (color == RED){
		old_head = chessboard->RedHead;
		if (chessboard->RedHead == -1 || chessboard->RedHead > at)
			chessboard->RedHead = at;
		cur_head = chessboard->RedHead;
		num_chess = chessboard->Red_Chess_Num;
	}
	else{
		old_head = chessboard->BlackHead;
		if (chessboard->BlackHead == -1 || chessboard->BlackHead > at)
			chessboard->BlackHead = at;
		cur_head = chessboard->BlackHead;
		num_chess = chessboard->Black_Chess_Num;
	}
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
		if (chessboard->RedHead == src)
			chessboard->RedHead = dst;
		if (chessboard->BlackHead == src)
			chessboard->BlackHead = dst;
	}
	else{
		removeFromBoard(chessboard, src);
		insertInBoard(chessboard, dst);
	}
}

bool cantWinCheck(const ChessBoard *chessboard, const int color){
	/*
	P C N R M G K
	0 1 2 3 4 5 6
	*/
	if (chessboard->cantWin[color])
		return true; // cant once, cant forever
	int my_num = color == RED ? chessboard->Red_Chess_Num : chessboard->Black_Chess_Num;
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

inline void addToMove(std::deque<MoveInfo> &moves, const MoveInfo& elem, int* num_eats){
	if(elem.from_chess_no == elem.to_chess_no ||
	elem.to_chess_no == CHESS_EMPTY){ // flip or move
		moves.push_back(elem);
	}
	else{
		moves.push_front(elem);
		(*num_eats)++;
	}
}

void MyAI::Expand(const ChessBoard *chessboard, const int color, std::deque<MoveInfo> &Result, int* num_eats)
{
	assert(Result.empty());
	int i = (color == RED) ? chessboard->RedHead : chessboard->BlackHead;
	int n_chess = (color == RED) ? chessboard->Red_Chess_Num : chessboard->Black_Chess_Num;
	const std::array<int, 32>& board = chessboard->Board;
	int numEats = 0;
	for (; i != -1; i = chessboard->Next[i]){
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
					// Result.push_back(MoveInfo(board, i, rowCount));
					addToMove(Result, MoveInfo(board, i, rowCount), &numEats);
				}
			}
			for(int colCount=col; colCount<32;colCount += 4)
			{

				if(Referee(board,i,colCount,color))
				{
					// Result.push_back(MoveInfo(board, i, colCount));
					addToMove(Result, MoveInfo(board, i, colCount), &numEats);
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
					// Result.push_back(MoveInfo(board, i, Move[k]));
					addToMove(Result, MoveInfo(board, i, Move[k]), &numEats);
				}
			}
		}
	}
	assert(n_chess == 0);
	if(num_eats != nullptr){
		*num_eats = numEats;
	}
}


// Referee
bool MyAI::Referee(const std::array<int, 32>& chess, const int from_location_no, const int to_location_no, const int UserId)
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

double evalColor(const ChessBoard *chessboard, const std::deque<MoveInfo> &Moves, const int color, const bool static_material)
{
	std::array<double, 14> values{
		1, 180, 6, 18, 90, 270, 810,
		1, 180, 6, 18, 90, 270, 810};
	// if eat a pawn, my king adds this much to its value
	// 5->4 : 0
	// 4->3 : 5
	// 3->2 : 17
	// 2->1 : 80
	// 1->0 : 260 
	static std::array<double, 6> king_add_n_pawn{  381, 111, 22, 5, 0, 0 };

	// adjust king value
	for(int c = 0; c < 2; c++){
		int op_king = (!c) * 7 + 6;
		int my_pawn = c * 7 + 0;
		int n = chessboard->AliveChess[my_pawn];
		values[op_king] += king_add_n_pawn[n] * (!static_material);
	}

	double max_value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 + king_add_n_pawn[0] * (!static_material);

	double value = 0;
	int head = (color == RED) ? chessboard->RedHead : chessboard->BlackHead;
	for (int i = head; i != -1; i = chessboard->Next[i])
	{
		assert(chessboard->Board[i] / 7 == color);
		value += values[chessboard->Board[i]];
	}
	// covered
	for(int i = 0; i < 7; i++){
		int p = color * 7 + i;
		assert(chessboard->CoverChess[p] >= 0);
		value += chessboard->CoverChess[p] * values[p];
	}

	/*
	max mobility = 12 + 12 + 14 + 14 = 52 (can right + can left + up + down.)
	cannon's mobility is also at most 4 (only one legal jump)

	max increase:
		normal: +6
		value aware: (810 + king_add_n_pawn[0])*3 + 270*2 + 180
	*/
	const double w_mob = 1/((810 + king_add_n_pawn[0])*3 + 270*2 + 180);
	double mobility = 0;
	const double max_mobi = ((2*2+3*3) + 180*2*4 + 6*2*3 + 18*2*3 + 90*1*3 + (90*1 + 270*2 + 810*1 + king_add_n_pawn[0])*4)*w_mob;
	for(auto& m: Moves){
		if(m.from_chess_no == CHESS_COVER)
			continue;
		mobility += values[m.from_chess_no]*w_mob;
	}

	if(!static_material){
		value += mobility;
		max_value += max_mobi;
	}
	assert(value <= max_value);

	return value / max_value;
}

// always use my point of view, so use this->Color
double MyAI::Evaluate(const ChessBoard *chessboard,
					  const std::deque<MoveInfo> &Moves, const int color, const int depth)
{
	// score = My Score - Opponent's Score
	// offset = <WIN + BONUS> to let score always not less than zero

	double score = 0;
	bool finish;
	int legal_move_count = Moves.size();

	if (legal_move_count == 0)
	{ // Win, Lose
		if (color == this->Color)
		{ // Lose
			score += LOSE - WIN;
		}
		else
		{ // Win
			score += WIN - LOSE;
		}
		finish = true;
	}
	else if (isDraw(chessboard))
	{ // Draw
		// score = DRAW - DRAW;
		finish = false;
		if (!cantWinCheck(chessboard, this->Color))
			score += LOSE - WIN;
		else
			finish = true;
	}
	else
	{ // no conclusion
		
		std::deque<MoveInfo> myMoves;
		std::deque<MoveInfo> oppMoves;
		
		if (color == this->Color){
			// copy mine, then expand opponent
			myMoves = Moves;
			Expand(chessboard, this->Color^1, oppMoves, nullptr);
		}
		else{
			// copy opponent, then expand mine
			oppMoves = Moves;
			Expand(chessboard, this->Color, myMoves, nullptr);
		}

		// My Material
		double piece_value = (
			evalColor(chessboard, myMoves, this->Color, false) -
			evalColor(chessboard, oppMoves, this->Color^1, false)
		);

		// linear map to (-<WIN>, <WIN>)
		piece_value = piece_value / 1.0 * MIDDLE;

		score += piece_value;
		finish = false;
	}

	// Bonus (Only Win / Draw)
	if (finish)
	{
		std::deque<MoveInfo> plh; // place holder
		// My Material
		double piece_value = (
			evalColor(chessboard, plh, this->Color, true) -
			evalColor(chessboard, plh, this->Color^1, true)
		);

		if(legal_move_count == 0 && color == this->Color){ // I lose
			if(piece_value > 0){ // but net value > 0
				piece_value = 0;
			}
		}else if(legal_move_count == 0 && color != this->Color){ // Opponent lose
			if(piece_value < 0){ // but net value < 0
				piece_value = 0;
			}
		}

		// linear map to [-<BONUS>, <BONUS>]
		// score max value = 1*5 + 180*2 + 6*2 + 18*2 + 90*2 + 270*2 + 810*1 = 1943
		// <ORIGINAL_SCORE> / <ORIGINAL_SCORE_MAX_VALUE> * <BONUS>
		double max_value = 1.0; 
		const double max_depth_bonus = 0.3; // 0.3 works
		double depth_bonus = std::pow(0.99, std::max(0, depth/2))*max_depth_bonus;
		piece_value += depth_bonus;
		max_value += max_depth_bonus;

		piece_value = piece_value / max_value * BONUS;
		assert(piece_value <= BONUS && piece_value >= -BONUS);
		score += piece_value; 
	}

	return score;
}

double MyAI::Simulate(ChessBoard chessboard, const int color, const int max_depth){
	// color is the player previous moved
	// int turn_color = this->Color ^ 1;
	int turn_color = color ^ 1;
	int remain_depth = max_depth;

	while(true){
		// Expand
		std::deque<MoveInfo> Moves;
		int eatNum;
		Expand(&chessboard, turn_color, Moves, &eatNum);
		int moveNum = Moves.size();

		// Check if is finish
		if(isFinish(&chessboard, Moves, eatNum, turn_color, remain_depth)){
			return Evaluate(&chessboard, Moves, turn_color, max_depth - remain_depth);
		}

		// distinguish eat-move and pure-move
		for(int i = 0; i < eatNum; i++){
			assert(Moves[i].to_chess_no != CHESS_EMPTY);
		}

		// std::vector<double> Probs(moveNum);
		// std::discrete_distribution<int> categorical(Probs.begin(), Probs.end());
		// int move_id = categorical(this->rng);

		// Random Move
		int move_id = (eatNum != 0 && randIndex(2)) ? randIndex(eatNum) : randIndex(moveNum);
		MakeMove(&chessboard, Moves[move_id].num(), 0); // 0: dummy

		// Change color
		turn_color ^= 1;
		remain_depth--;
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

double MyAI::SEE(const ChessBoard *chessboard, const int position, const int color){
	assert((position >= 0 && position < 32));
	assert(chessboard->Board[position] >= 0);
	assert(chessboard->Board[position] / 7 != color);
	// if(chessboard->Board[position] / 7 == color) // last move is flip
	// 	return 0;
	

	std::vector<int> RedCands, BlackCands;
	std::array<int, 32> board = chessboard->Board; // copy
	for(int i = chessboard->RedHead; i != -1; i = chessboard->Next[i]){
		if (board[i] % 7 == 1 || // Cannon
			(i == position - 4) || // neighbors
			(i == position - 1) ||
			(i == position + 4) ||
			(i == position + 1)){
				assert(board[i] >= 0 && board[i] < 14);
				RedCands.push_back(i);
		}
	}
	for(int i = chessboard->BlackHead; i != -1; i = chessboard->Next[i]){
		if (board[i] % 7 == 1 || // Cannon
			(i == position - 4) || // neighbors
			(i == position - 1) ||
			(i == position + 4) ||
			(i == position + 1)){
				assert(board[i] >= 0 && board[i] < 14);
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

	for(size_t i = 1; i < RedCands.size(); i++){
		assert(values[board[RedCands[i]]] <= values[board[RedCands[i]]]);
	}
	for(size_t i = 1; i < BlackCands.size(); i++){
		assert(values[board[BlackCands[i]]] <= values[board[BlackCands[i]]]);
	}

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
bool MyAI::isFinish(const ChessBoard* chessboard, const std::deque<MoveInfo> &Moves, const int eatNum, const int color, const int remain_depth){
	int see_pos = chessboard->History.empty() ? -1 : (chessboard->History.back() % 100);
	assert(chessboard->Board[see_pos] / 7 == (color^1));
	return (isTimeUp() || // time is up
		(remain_depth <= 0 && SEE(chessboard, see_pos, color) <= 0) ||	// reach limit of depth and is quiescent (net gain < 0)
		chessboard->Red_Chess_Num == 0 ||								// terminal node (no chess type)
		chessboard->Black_Chess_Num == 0 ||								// terminal node (no chess type)
		Moves.empty() ||												// terminal node (no move type)
		isDraw(chessboard)						 						// draw
	);
	// if (
	// 	chessboard->Red_Chess_Num == 0 ||		 // terminal node (no chess type)
	// 	chessboard->Black_Chess_Num == 0 ||		 // terminal node (no chess type)
	// 	Moves.empty() ||						 // terminal node (no move type)
	// 	isDraw(chessboard)						 // draw
	// ) return true;
	// for (int i = 0; i < eatNum; i++){
	// 	// gaurantee is eat
	// 	// not quiescent (net gain > 0) --> not finish
	// 	int see_pos = Moves[i].to_location_no;
	// 	assert(Moves[i].to_chess_no == CHESS_COVER || Moves[i].to_chess_no / 7 != color);
	// 	if (SEE(chessboard, see_pos, color) > 0)
	// 		return false;
	// }
	// return true;
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

	return elapsed >= TIME_LIMIT*1000;
}

// return range: [0, max)
uint32_t MyAI::randIndex(uint32_t max){
	return pcg32_boundedrand_r(&this->rng, max);
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

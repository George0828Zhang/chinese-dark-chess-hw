#ifndef MCTREE_INCLUDED
#define MCTREE_INCLUDED 

#include <vector>
#include <cfloat>
#include <cmath>
#include <cassert>
#include "MyAI.h"
#define RAVE

class MCTree{
	double _c;
	double _c2;
public:
	int max_depth;
	std::vector<std::vector<MoveInfo>> histories;
	std::vector<double> sum_scores;
	std::vector<double> sum_squares;
	std::vector<int> n_trials;

	std::vector<double> sum_scores_amaf;
	std::vector<double> sum_squares_amaf;
	std::vector<int> n_trials_amaf;

	std::vector<int> parent_id;
	std::vector<int> depth;
	std::vector<bool> dead;
	std::vector<std::vector<int>> children_ids;

    // speedup
	std::vector<double> CsqrtlogN;
	std::vector<double> sqrtN;
	std::vector<double> Average;
	std::vector<double> Variance;

	// propogation
	double buffer_scores;
	double buffer_squares;
	double buffer_trials;

	// hashtable
	std::array<double, 15*32*32> tab_scores;
	std::array<double, 15*32*32> tab_squares;
	std::array<int, 15*32*32> tab_trials;

	static inline int mvhash(const MoveInfo& mv){
		return (mv.from_chess_no + 1)*1024 + mv.from_location_no*32 + mv.to_location_no;
	}

	MCTree(double exploration, double max_var):
    _c(exploration),_c2(max_var),max_depth(0),histories()
	,sum_scores(),sum_squares(),n_trials()
	,sum_scores_amaf(),sum_squares_amaf(),n_trials_amaf()
	,parent_id(),depth(),dead(),children_ids()
	,CsqrtlogN(),sqrtN(),Average(),Variance()
	,buffer_scores(0),buffer_squares(0),buffer_trials(0) {};

	int expand(int parent, const MoveInfo& new_move){
		int id = histories.size();
		std::vector<MoveInfo> new_hist;
		if (parent != -1){
			new_hist = histories[parent]; // copy
			new_hist.push_back(new_move);
		}
		histories.push_back(new_hist);
		sum_scores.push_back(0);
		sum_squares.push_back(0);
		n_trials.push_back(0);
		sum_scores_amaf.push_back(0);
		sum_squares_amaf.push_back(0);
		n_trials_amaf.push_back(0);
		parent_id.push_back(parent);
		depth.push_back(parent == -1 ? 0 : (depth[parent]+1));
		dead.push_back(false);
		children_ids.push_back(std::vector<int>());
		if (parent != -1)
			children_ids[parent].push_back(id);
		max_depth = std::max(depth.back(), max_depth);

        // speedup
        CsqrtlogN.push_back(0);
        sqrtN.push_back(0);
        Average.push_back(0);
        Variance.push_back(0);
		return id;
	}
	void update_stats(int i, double sum1, double sum2, int trials){
		sum_scores[i] += sum1;
		sum_squares[i] += sum2;
		n_trials[i] += trials;
	}
	void update_amaf_stats(int i, double sum1, double sum2, int trials){
		sum_scores_amaf[i] += sum1;
		sum_squares_amaf[i] += sum2;
		n_trials_amaf[i] += trials;
	}
	void update_computation(int i){
		assert(n_trials[i] > 0);
		double trials_amaf = n_trials[i] + n_trials_amaf[i];
		double alpha = std::min(1.0, (double)n_trials[i] / 10000.0);
		// Silver
		// const double _b2 = 0.03*0.03;
		// double beta = 1.0 / ((n_trials[i] / trials_amaf) + 1 + 4*_b2*n_trials[i]);
		// double alpha = 1 - beta;

		// speedup
		CsqrtlogN[i] = _c*std::sqrt(std::log(trials_amaf));
		sqrtN[i] = std::sqrt(trials_amaf);

		double avg_real = sum_scores[i] / n_trials[i];
		double var_real = sum_squares[i] / n_trials[i] - avg_real * avg_real + 1e-7;
		assert(var_real >= 0);
		double avg_amaf = (sum_scores[i] + sum_scores_amaf[i]) / trials_amaf;
		double var_amaf = (sum_squares[i] + sum_squares_amaf[i]) / trials_amaf - avg_amaf * avg_amaf + 1e-7;
		assert(var_amaf >= 0);
		Average[i] = alpha*avg_real + (1.0-alpha)*avg_amaf;
		Variance[i] = alpha*alpha*var_real + (1.0-alpha)*(1.0-alpha)*var_amaf + 2*alpha*(1.0-alpha)*std::sqrt(var_real*var_amaf);
	}
	void update_stats_buff(double sum1, double sum2, int trials){
		buffer_scores += sum1;
		buffer_squares += sum2;
		buffer_trials += trials;
	}
	void clear_stats_buff_and_hash(){
		buffer_scores = 0;
		buffer_squares = 0;
		buffer_trials = 0;
		tab_scores.fill(0.0);
		tab_squares.fill(0.0);
		tab_trials.fill(0);
	}
	void update_leaf(int i, double sum1, double sum2, int trials){
		update_stats(i, sum1, sum2, trials);
		update_computation(i);
		update_stats_buff(sum1, sum2, trials); // aggregate for parent & ancestors
		// save to hash table for ancestors
		int i_hash = mvhash(histories[i].back()); // this is the move from parent to i
		tab_scores[i_hash] = sum1; // add this many simulations to ancestor siblings
		tab_squares[i_hash] = sum2;
		tab_trials[i_hash] = trials;
	}
	void propagate(int i){
		// pop buffer
		double sum1 = buffer_scores;
		double sum2 = buffer_squares;
		int trials = buffer_trials;

		// cumsum used to propogate amaf to ancestors
		double cumm_cum1 = 0;
		double cumm_cum2 = 0;
		double cumm_trials = 0;

		if (trials == 0) return;
		while(i != -1){
			update_stats(i, sum1, sum2, trials);
#ifdef RAVE
			// AMAF
			for(auto& ch: children_ids[i]){
				if(dead[ch]) continue;
				int ch_hash = mvhash(histories[ch].back());
				if(tab_trials[ch_hash] > 0){
					update_amaf_stats(ch, tab_scores[ch_hash], tab_squares[ch_hash], tab_trials[ch_hash]);
					cumm_cum1 += tab_scores[ch_hash];
					cumm_cum2 += tab_squares[ch_hash];
					cumm_trials += tab_trials[ch_hash];
				}
			}
			// update this node's amaf
			update_amaf_stats(i, cumm_cum1, cumm_cum2, cumm_trials);
			// save to hash table
			if (!histories[i].empty()){
				int i_hash = mvhash(histories[i].back()); // this is the move from parent to i
				tab_scores[i_hash] = sum1; // add this many simulations to ancestor siblings
				tab_squares[i_hash] = sum2;
				tab_trials[i_hash] = trials;
				/* for all nodes above the leaves' parent, they will contribute <buffer_trials> simulations.
				this differs from the values for leaves, which are distinct.
				actually, <buffer_trials> is just the sum of all leaves.
				*/
			}
#endif
            // speedup
            update_computation(i);

			i = parent_id[i];
		}
	}
	void mark_dead(int i){
		dead[i] = true;
	}
	int pv_leaf(int par){
		while(!children_ids[par].empty()){	
			double co = (depth[par]&1 ? -1 : 1); // Minimax
			int best_ch = 0;
			double best_score = -DBL_MAX;
			for(auto& ch: children_ids[par]){
				if (dead[ch]) continue;
				double explore = CsqrtlogN[par] / sqrtN[ch];
				double stdv = std::sqrt(std::min(Variance[ch] + explore, _c2));
				double cur_score = co*Average[ch] + explore * stdv;
				if(cur_score > best_score){
					best_ch = ch;
					best_score = cur_score;
				}
			}
			par = best_ch;
		}
		return par;
	}
};

#endif
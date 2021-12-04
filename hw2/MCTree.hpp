#ifndef MCTREE_INCLUDED
#define MCTREE_INCLUDED 

#include <vector>
#include <cfloat>
#include <cmath>

class MCTree{
	double _c;
	double _c2;
public:
	int max_depth;
	std::vector<std::vector<int>> histories;
	std::vector<double> sum_scores;
	std::vector<double> sum_squares;
	std::vector<int> n_trials;
	std::vector<int> parent_id;
	std::vector<int> depth;
	std::vector<bool> dead;
	std::vector<std::vector<int>> children_ids;

    // speedup
	std::vector<double> CsqrtlogN;
	std::vector<double> sqrtN;
	std::vector<double> Average;
	std::vector<double> Variance;

	MCTree(double exploration, double max_var):
    _c(exploration),_c2(max_var),max_depth(0),histories(),sum_scores(),sum_squares(),n_trials(),parent_id(),depth(),dead(),children_ids(),CsqrtlogN(),sqrtN(),Average(),Variance() {};

	int expand(int parent, int new_move){
		int id = histories.size();
		std::vector<int> new_hist;
		if (parent != -1){
			new_hist = histories[parent]; // copy
			new_hist.push_back(new_move);
		}
		histories.push_back(new_hist);
		sum_scores.push_back(0);
		sum_squares.push_back(0);
		n_trials.push_back(0);
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
	void update_computation(int i){
		// speedup
		CsqrtlogN[i] = _c*std::sqrt(std::log((double)n_trials[i]));
		sqrtN[i] = std::sqrt((double)n_trials[i]);
		Average[i] = sum_scores[i] / n_trials[i];
		Variance[i] = sum_squares[i] / n_trials[i] - Average[i] * Average[i];
	}
	void update_leaf(int i, double sum1, double sum2, int trials){
		update_stats(i, sum1, sum2, trials);
		update_computation(i);
	}
	void propogate(int i){
		double sum1 = 0;
		double sum2 = 0;
		int trials = 0;
		for(auto& ch: children_ids[i]){
			sum1 += sum_scores[ch];
			sum2 += sum_squares[ch];
			trials += n_trials[ch];
		}
		while(i != -1){
			update_stats(i, sum1, sum2, trials);

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
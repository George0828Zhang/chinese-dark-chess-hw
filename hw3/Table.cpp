// #include <tsl/robin_map.h>
#include <random>
#include "MyAI.h"

// class TransPosition{
// 	static const int POSITIONS = 32; 
// 	static const int TYPES = 15;
// 	array<key128_t, POSITIONS*TYPES> salt; // 32 pos, 14+1 types
// 	array<robin_map<key128_t, TableEntry>, 2> tables;
// public:

/* courtesy https://codereview.stackexchange.com/questions/109260/seed-stdmt19937-from-stdrandom-device */
template<class T = std::mt19937, std::size_t N = T::state_size * sizeof(typename T::result_type)>
auto ProperlySeededRandomEngine () -> typename std::enable_if<N, T>::type {
    std::random_device source;
    std::random_device::result_type random_data[(N - 1) / sizeof(source()) + 1];
    std::generate(std::begin(random_data), std::end(random_data), std::ref(source));
    std::seed_seq seeds(std::begin(random_data), std::end(random_data));
    return T(seeds);
}

TransPosition::TransPosition(){

    auto rng = ProperlySeededRandomEngine<mt19937_64>();

    for (uint i = 0; i < salt.size(); i++){
        uint64_t lo = rng();
        uint64_t hi = rng();
        salt[i] = lo + ((key128_t)hi << 64);
    }
}
inline int TransPosition::Convert(int chess) {
    // COVER -1 -> 15
    return (chess + TYPES) % TYPES;
}
key128_t TransPosition::compute_hash(const ChessBoard& chessboard) const {
    key128_t h = 0;
    for(int i = 0; i < POSITIONS; i++){
        if (chessboard.Board[i] == CHESS_EMPTY) continue;
        int piece = Convert(chessboard.Board[i]);
        h ^= salt[i * TYPES + piece];
    }
    return h;
}
key128_t TransPosition::MakeMove(const key128_t& other, const MoveInfo& move, const int chess) const {
    key128_t out = other;

    int src = move.from_location_no;
    int src_chess = move.from_chess_no;

    int dst = move.to_location_no;
    int dst_chess = move.to_chess_no;

    if (src == dst){//flip
        static const int cover = Convert(CHESS_COVER);
        out ^= salt[src * TYPES + cover]; // remove cover
        out ^= salt[src * TYPES + chess]; // add chess
    }
    else if (dst_chess == CHESS_EMPTY){// move
        out ^= salt[src * TYPES + src_chess]; // remove from src
        out ^= salt[dst * TYPES + src_chess]; // add to dst
    }
    else{//capture
        out ^= salt[src * TYPES + src_chess]; // remove src from src
        out ^= salt[dst * TYPES + dst_chess]; // remove dst from dst
        out ^= salt[dst * TYPES + src_chess]; // add src to dst
    }
    return out;
}
bool TransPosition::query(const key128_t& key, const int color, TableEntry& result) {
    if(tables[color].count(key) == 0)
        return false;
    result = tables[color][key];
    return true;
}
bool TransPosition::insert(const key128_t& key, const int color, const TableEntry& update){
    if(tables[color].count(key) == 0){
        tables[color][key] = update;
        return true;
    }		
    TableEntry& entry = tables[color][key];
    // if (update.vtype == ENTRY_EXACT && update.depth >= entry.depth){
    if (update.vtype == ENTRY_EXACT && update.rdepth >= entry.rdepth){
        entry = update;
        return true;
    }
    return false;
}
void TransPosition::clear_tables(){
    tables[RED].clear();
    tables[BLACK].clear();
}
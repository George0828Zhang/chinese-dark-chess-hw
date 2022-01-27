# Theory of Computer Games - Chinese Dark Chess
The repo contains implementations for search algorithms introduced in the course [Theory of Computer Games Fall 2021](https://homepage.iis.sinica.edu.tw/~tshsu/tcg/2021/).

## Usage
For all three versions, go into the folder and run
```bash
make
```
The AI will be at `exec.X`, where `X=hw1,hw2,final`. You would need the client, and possibly another baseline program to test the AI. See course website for details.

## Nega-scout
The implementation is in folder `hw3/`. Changes from baseline are listed below.
- Nega-scout algorithm
- Time control
- Transposition table ([robin-map](https://github.com/Tessil/robin-map))
- Dynamic search extension
- Aspiration search
- Star0 & Star1 chance search
- Killer heuristic
- Other optimizations
    - Pre-compute distance and capture rules
    - Optimize move generation
    - Optimize draw detection

For details, see [report](hw3/negascout-report.pdf). 
> :information_source: This version achieved #2 in course competition.


## Monte-Carlo Tree Search
The implementation is in folder `hw2/`. Changes from baseline are listed below.
- UCT with variance
- RAVE (AMAF heuristic)
- Progressive Pruning
- Depth-$i$ enhancement

## Nega-max
The implementation is in folder `hw1/`. Changes from baseline are listed below.

> :warning: **all** of the features are obsoleted by the version in [Nega-scout](#nega-scout).


- Improved evaluation function
- Move ordering
- Quiescent search
- Doubly-link-list-like array implementation




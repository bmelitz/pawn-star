#include "search.hpp"
#include "eval.hpp"
#include "tt.hpp"
#include "bitboard_helpers.hpp"
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace Search {

    Move best_move;
    int nodes_searched = 0;

    constexpr int PIECE_VALUES[] = {100, 300, 320, 500, 900, 10000};
    // after enough half moves, even in the case of a transposition table collision we opt to evaluate
    // the position ourselves to avoid the situation where an important line would invoke the 50 move rule
    constexpr int NEAR_FIFTY_GUARD = 90;

    std::vector<Move> extract_pv(Board& board, int max_length) {
        std::vector<Move> pv;
        std::unordered_set<uint64_t> seen_keys;
        std::vector<StateInfo> states(max_length);  // keep all states alive (otherwise dangling pointers)

        for (int i = 0; i < max_length; i++) {
            uint64_t key = board.get_hash();

            if (seen_keys.count(key)) {
                break;
            }
            seen_keys.insert(key);

            int tt_score;
            Move tt_move;
            if (!TT::probe(key, 0, 0, -INFINITY_SCORE, INFINITY_SCORE, tt_score, tt_move)) {
                break;
            }
            if (tt_move.data == 0) {
                break;
            }

            pv.push_back(tt_move);
            board.make_move(tt_move, states[i]);  // states[i] lives until function returns
        }

        for (int i = pv.size() - 1; i >= 0; i--) {
            board.unmake_move(pv[i]);
        }

        return pv;
    }

    // scores a move for ordering purposes
    // captures: MVV-LVA (most valuable victim, least valuable attacker)
    // promotions: scored as if the pawn is "capturing" the promoted piece — i.e.
    //   the gain is +promoted_piece_value rather than a captured enemy piece.
    //   This puts promotions in the same scoring band as captures, ordered by
    //   how valuable the piece being promoted to is.
    static inline int score_move(const Board& board, Move m, Move tt_move = Move()) {
        if (m.data == tt_move.data && m.data != 0) {
            return 2000000;
        }

        if (m.is_promo()) {
            // treat promotion as "capturing" the promoted piece with the pawn:
            // victim_val = value of promoted piece, attacker_val = pawn (100)
            int promo_piece = m.flag() & 3; // 0=N, 1=B, 2=R, 3=Q
            int promoted_val = PIECE_VALUES[promo_piece + 1]; // offset by 1: N=1,B=2,R=3,Q=4
            constexpr int pawn_val = 100;
            return 1000000 + (promoted_val * 10) - pawn_val;
        }

        if (m.is_capture()) {
            int attacker = board.get_full_piece(m.from_sq()) % 6;
            int victim_val = 100;

            if (m.flag() != Move::EP_CAPTURE) {
                int victim_piece = board.get_full_piece(m.to_sq());
                if (victim_piece != -1) {
                    victim_val = PIECE_VALUES[victim_piece % 6];
                }
            }

            return 1000000 + (victim_val * 10) - PIECE_VALUES[attacker];
        }

        return 0; // quiet move
    }

    int quiescence_search(Board& board, int alpha, int beta) {
        nodes_searched++;

        int stand_pat = Eval::evaluate(board);
        if (stand_pat >= beta) {
            return beta;
        }

        int best_score = stand_pat;
        if (best_score > alpha) {
            alpha = best_score;
        }

        MoveList list;
        MoveGen::generate_captures(board, list);
        Color us = board.get_side_to_move();

        int move_scores[256];
        for (int i = 0; i < list.count; ++i) {
            move_scores[i] = score_move(board, list.moves[i]);
        }

        for (int i = 0; i < list.count; ++i) {
            // selection sort
            int best_idx = i;
            for (int j = i + 1; j < list.count; ++j) {
                if (move_scores[j] > move_scores[best_idx]) {
                    best_idx = j;
                }
            }
            std::swap(list.moves[i], list.moves[best_idx]);
            std::swap(move_scores[i], move_scores[best_idx]);

            Move m = list.moves[i];

            StateInfo next_state;
            board.make_move(m, next_state);

            Square king_sq = board.get_king_square(us);
            if (MoveGen::is_square_attacked(board, king_sq, board.get_side_to_move())) {
                board.unmake_move(m);
                continue;
            }

            int search_score = -quiescence_search(board, -beta, -alpha);

            board.unmake_move(m);

            if (search_score > best_score) {
                best_score = search_score;
            }
            if (best_score > alpha) {
                alpha = best_score;
            }
            if (alpha >= beta) {
                break;
            }
        }

        return best_score;
    }

    int negamax(Board& board, int depth, int alpha, int beta, int ply, bool can_extend) {
        nodes_searched++;

        if (ply >= MAX_PLY) {
            return Eval::evaluate(board);
        }

        int mate_value = MATE_SCORE - ply;
        if (alpha >= mate_value) {
            return alpha;
        }
        if (beta <= -mate_value) {
            return beta;
        }

        // Skip at ply 0 — the root position is handled by the game manager,
        // and returning 0 here would prevent searching entirely
        if (ply > 0 && (board.is_repetition() || board.is_insufficient_material() || board.get_half_move_clock() >= 100)) {
            return 0;
        }

        uint64_t hash_key = board.get_hash();
        int tt_score;
        Move tt_move = Move();

        // This branch is very rare - hopefully optimized away by the branch predictor
        bool near_fifty = board.get_half_move_clock() >= NEAR_FIFTY_GUARD;
        // TT::probe still runs even if near_fifty, so move ordering is preserved
        // We're only skipping the score cutoff
        // Opting to evaluate the move ourselves in case the position being close to the 50 move rule changes something
        if (TT::probe(hash_key, depth, ply, alpha, beta, tt_score, tt_move) && !near_fifty) {
            if (ply == 0) {
                best_move = tt_move;
            }
            return tt_score;
        }

        int original_alpha = alpha;
        Color us = board.get_side_to_move();
        Square king_sq = board.get_king_square(us);
        bool in_check = MoveGen::is_square_attacked(board, king_sq, ~us);
        bool is_extending = false;

        // check extension
        if (in_check && can_extend) {
            depth++;
            is_extending = true;
        }

        if (depth == 0) {
            return quiescence_search(board, alpha, beta);
        }

        MoveList list;
        MoveGen::generate_pseudo_moves(board, list);

        int move_scores[256];
        for (int i = 0; i < list.count; ++i) {
            move_scores[i] = score_move(board, list.moves[i], tt_move);
        }

        int legal_moves = 0;
        int best_score = -INFINITY_SCORE;
        Move best_move_this_node = Move();

        for (int i = 0; i < list.count; ++i) {
            // selection sort
            int best_idx = i;
            for (int j = i + 1; j < list.count; ++j) {
                if (move_scores[j] > move_scores[best_idx]) {
                    best_idx = j;
                }
            }
            std::swap(list.moves[i], list.moves[best_idx]);
            std::swap(move_scores[i], move_scores[best_idx]);

            Move m = list.moves[i];
            StateInfo next_state;

            board.make_move(m, next_state);

            Square current_king_sq = board.get_king_square(us);
            if (MoveGen::is_square_attacked(board, current_king_sq, board.get_side_to_move())) {
                board.unmake_move(m);
                continue;
            }

            legal_moves++;

            int search_score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, !is_extending);
            board.unmake_move(m);

            if (search_score > best_score) {
                best_score = search_score;
                best_move_this_node = m;

                if (ply == 0) {
                    best_move = m;
                }
            }

            if (best_score > alpha) {
                alpha = best_score;
            }

            if (alpha >= beta) {
                break;
            }
        }

        if (legal_moves == 0) {
            if (in_check) {
                return -MATE_SCORE + ply;
            } else {
                return 0;
            }
        }

        int tt_flag = HASH_EXACT;
        if (best_score <= original_alpha) {
            tt_flag = HASH_ALPHA;
        }
        else if (best_score >= beta) {
            tt_flag = HASH_BETA;
        }
        TT::store(hash_key, depth, ply, best_score, tt_flag, best_move_this_node);

        return best_score;
    }

    int get_best_move(Board& board, int max_depth) {
        nodes_searched = 0;
        int latest_score = 0;

        for (int current_depth = 1; current_depth <= max_depth; current_depth++) {
            latest_score = negamax(board, current_depth, -INFINITY_SCORE, INFINITY_SCORE, 0, true);

            std::vector<Move> pv = extract_pv(board, current_depth);

            std::cout << "info depth " << current_depth
                      << " score cp " << latest_score
                      << " nodes " << nodes_searched
                      << " pv ";
            for (auto& m : pv) {
                BitboardHelpers::print_move(m);
                std::cout << " ";
            }
            std::cout << std::endl;
        }
        return latest_score;
    }
}

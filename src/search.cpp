#include "search.hpp"
#include "eval.hpp"
#include "tt.hpp"
#include "bitboard_helpers.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace Search {

    Move best_move;
    int best_score = 0;
    uint64_t nodes_searched = 0;
    std::atomic<bool> stop{false};

    // state of the search in progress
    static Limits limits;
    static std::chrono::steady_clock::time_point start_time;
    static int root_depth = 0;     // depth of the current iteration
    static Move iteration_best;    // best root move found so far in the current iteration
    static int seldepth = 0;       // deepest ply reached this iteration, quiescence included

    // Triangular PV table. pv_table[ply] holds the best line found from the node at
    // that ply, pv_length[ply] long. When a move raises alpha, the line becomes that
    // move followed by the child's line (pv_table[ply + 1]). pv_table[0] ends up as
    // the principal variation from the root. Sized MAX_PLY + 1 since ply reaches MAX_PLY.
    static Move pv_table[MAX_PLY + 1][MAX_PLY + 1];
    static int pv_length[MAX_PLY + 1];

    static void update_pv(int ply, Move m) {
        pv_table[ply][0] = m;
        for (int i = 0; i < pv_length[ply + 1]; i++) {
            pv_table[ply][i + 1] = pv_table[ply + 1][i];
        }
        pv_length[ply] = pv_length[ply + 1] + 1;
    }

    static int64_t elapsed_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
    }

    // Reading the clock is slow relative to a node, so only check every 2048 nodes.
    static inline void check_time() {
        if ((nodes_searched & 2047) == 0 && limits.hard_ms >= 0 && elapsed_ms() >= limits.hard_ms) {
            stop = true;
        }
    }

    // Once this is true every node returns immediately, unwinding the search.
    // Ignored during depth 1 so there is always a complete iteration to fall back on.
    static inline bool stopped() {
        return root_depth > 1 && stop.load(std::memory_order_relaxed);
    }

    constexpr int PIECE_VALUES[] = {100, 300, 320, 500, 900, 10000};
    // after enough half moves, even in the case of a transposition table collision we opt to evaluate
    // the position ourselves to avoid the situation where an important line would invoke the 50 move rule
    constexpr int NEAR_FIFTY_GUARD = 90;

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

    int quiescence_search(Board& board, int alpha, int beta, int ply) {
        nodes_searched++;
        seldepth = std::max(seldepth, ply);
        check_time();
        if (stopped()) {
            return 0;
        }

        // "Stand pat": the side to move doesn't have to capture, so the static eval is
        // a lower bound. Fail-soft: return the eval itself rather than clamping to beta,
        // matching negamax (a tighter bound is more useful to the TT and the parent).
        int stand_pat = Eval::evaluate(board);
        if (stand_pat >= beta) {
            return stand_pat;
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

            int search_score = -quiescence_search(board, -beta, -alpha, ply + 1);

            board.unmake_move(m);
            if (stopped()) {
                return 0;
            }

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

    int negamax(Board& board, int depth, int alpha, int beta, int ply, bool can_extend, bool allow_null) {
        pv_length[ply] = 0; // first, so every early return leaves an empty line
        nodes_searched++;
        seldepth = std::max(seldepth, ply);
        check_time();
        if (stopped()) {
            return 0;
        }

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
        if (ply > 0 && (board.is_repetition(ply) || board.is_insufficient_material() || board.get_half_move_clock() >= 100)) {
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
        // Never cut off at the root: we need a real search there to pick the move
        // (and the root position's repetition history isn't part of the TT entry).
        // The TT move is still used for ordering.
        bool tt_hit = TT::probe(hash_key, depth, ply, alpha, beta, tt_score, tt_move);
        if (tt_hit && ply > 0 && !near_fifty) {
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
            return quiescence_search(board, alpha, beta, ply);
        }

        // Null-move pruning: let the opponent move twice in a row. If our position is
        // still >= beta after a reduced-depth search, a real move would almost surely do
        // even better (doing something beats doing nothing), so cut off without searching
        // any moves. The exception is zugzwang, where every move makes things worse,
        // so skip it when we only have pawns (where zugzwang is common), in check
        // (passing would be illegal), right after another null move, and at the root.
        if (allow_null && ply > 0 && !in_check && depth >= 3 &&
            board.has_non_pawn_material(us) && Eval::evaluate(board) >= beta) {
            int reduction = 3 + depth / 4;

            StateInfo null_state;
            board.make_null_move(null_state);
            int null_score = -negamax(board, std::max(depth - reduction, 0), -beta, -beta + 1,
                                      ply + 1, true, false);
            board.unmake_null_move();

            if (stopped()) {
                return 0;
            }
            if (null_score >= beta) {
                // a "mate" found after passing isn't a real mate, so don't report one
                return null_score >= MATE_SCORE - MAX_PLY ? beta : null_score;
            }
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

            int search_score = -negamax(board, depth - 1, -beta, -alpha, ply + 1, !is_extending, true);
            board.unmake_move(m);

            // aborted: the score is garbage, so don't use it or store it in the TT
            if (stopped()) {
                return 0;
            }

            if (search_score > best_score) {
                best_score = search_score;
                best_move_this_node = m;

                if (ply == 0) {
                    iteration_best = m;
                }
            }

            // only true right after this move became the new best above alpha
            if (best_score > alpha) {
                alpha = best_score;
                update_pv(ply, m);
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

    static void print_info(int depth, int score) {
        int64_t ms = elapsed_ms();
        std::ostringstream out;
        out << "info depth " << depth << " seldepth " << seldepth;
        if (std::abs(score) >= MATE_SCORE - MAX_PLY) {
            // UCI wants mate in moves (not plies), negative if we are getting mated
            int plies = MATE_SCORE - std::abs(score);
            int moves = (plies + 1) / 2;
            out << " score mate " << (score > 0 ? moves : -moves);
        } else {
            out << " score cp " << score;
        }
        out << " nodes " << nodes_searched
            << " nps " << nodes_searched * 1000 / std::max<int64_t>(ms, 1)
            << " time " << ms
            << " pv";
        for (int i = 0; i < pv_length[0]; i++) {
            out << " " << BitboardHelpers::move_str(pv_table[0][i]);
        }
        std::cout << out.str() << std::endl;
    }

    Move think(Board& board, const Limits& search_limits) {
        limits = search_limits;
        start_time = limits.start;
        nodes_searched = 0;
        best_move = Move();
        best_score = 0;

        for (int depth = 1; depth <= limits.depth; depth++) {
            root_depth = depth;
            iteration_best = Move();
            seldepth = 0;

            int score = negamax(board, depth, -INFINITY_SCORE, INFINITY_SCORE, 0, true, true);

            // cut off mid-iteration: keep the previous iteration's move
            if (stopped()) {
                break;
            }

            best_move = iteration_best;
            best_score = score;

            // no legal moves at the root (checkmate or stalemate)
            if (best_move.data == 0) {
                break;
            }
            if (limits.print_info) {
                print_info(depth, score);
            }
            // another iteration takes several times longer than this one did,
            // so past the soft limit it would almost surely be cut off anyway
            if (limits.soft_ms >= 0 && elapsed_ms() >= limits.soft_ms) {
                break;
            }
        }

        root_depth = 0;
        return best_move;
    }

    int get_best_move(Board& board, int max_depth) {
        stop = false;
        Limits fixed_depth;
        fixed_depth.depth = max_depth;
        think(board, fixed_depth);
        return best_score;
    }
}

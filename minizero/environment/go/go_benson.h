#pragma once

#include "go.h"
#include <iostream>
#include <map>
#include <set>
#include <vector>
namespace minizero::env::go {

class GoBenson {
public:
    static GoPair<GoBitboard> getBensonBitboard(const GoPair<GoBitboard> benson_bitboard, const GoPair<GoBitboard>& stone_bitboard_, int board_size_, const GoBitboard& board_left_boundary_bitboard_,
                                                const GoBitboard& board_right_boundary_bitboard_, const GoBitboard& board_mask_bitboard_)
    {
        GoPair<GoBitboard> result;
        // GoPair<GoBitboard> benson_stone_bitboard_;
        // benson_stone_bitboard_.set(Player::kPlayer1, (~benson_bitboard.get(Player::kPlayer2)) & stone_bitboard_.get(Player::kPlayer1));
        // benson_stone_bitboard_.set(Player::kPlayer2, stone_bitboard_.get(Player::kPlayer2));
        result.set(Player::kPlayer1, compute1ColorBensonBitboard(stone_bitboard_, board_size_, board_left_boundary_bitboard_,
                                                                 board_right_boundary_bitboard_, board_mask_bitboard_, Player::kPlayer1));
        // benson_stone_bitboard_.set(Player::kPlayer2, (~result.get(Player::kPlayer1)) & stone_bitboard_.get(Player::kPlayer2));
        // benson_stone_bitboard_.set(Player::kPlayer1, stone_bitboard_.get(Player::kPlayer1));
        result.set(Player::kPlayer2, compute1ColorBensonBitboard(stone_bitboard_, board_size_, board_left_boundary_bitboard_,
                                                                 board_right_boundary_bitboard_, board_mask_bitboard_, Player::kPlayer2));
        // printboard(stone_bitboard_.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer2), board_size_, Player::kPlayerNone);
        // printboard(result.get(Player::kPlayer1), board_size_, Player::kPlayer1);
        // printboard(result.get(Player::kPlayer2), board_size_, Player::kPlayer2);
        return result;
    }
    static GoBitboard dilateBitboard(const GoBitboard& bitboard, int board_size_, const GoBitboard& board_left_boundary_bitboard_,
                                     const GoBitboard& board_right_boundary_bitboard_, const GoBitboard& board_mask_bitboard_)
    {
        return ((bitboard << board_size_) |                           // move up
                (bitboard >> board_size_) |                           // move down
                ((bitboard & ~board_left_boundary_bitboard_) >> 1) |  // move left
                ((bitboard & ~board_right_boundary_bitboard_) << 1) | // move right
                bitboard) &
               board_mask_bitboard_;
    }
    static void getChain(const GoBitboard& color_stone_bitboard, int board_size_, const GoBitboard& board_left_boundary_bitboard_,
                         const GoBitboard& board_right_boundary_bitboard_, const GoBitboard& board_mask_bitboard_, std::vector<GoBitboard>& vcolor_reg_bitboard, std::vector<GoBitboard>& vcolor_surr_bitboard)
    {
        GoBitboard stone_bitboard = color_stone_bitboard;
        while (!stone_bitboard.none()) {
            int pos = stone_bitboard._Find_first();
            // flood fill
            GoBitboard flood_fill_bitboard;
            flood_fill_bitboard.set(pos);
            bool need_dilate = true;
            while (need_dilate) {
                GoBitboard dilate_bitboard = dilateBitboard(flood_fill_bitboard, board_size_, board_left_boundary_bitboard_,
                                                            board_right_boundary_bitboard_, board_mask_bitboard_) &
                                             stone_bitboard;
                need_dilate = (flood_fill_bitboard != dilate_bitboard);
                flood_fill_bitboard = dilate_bitboard;
            }
            GoBitboard surrounding_bitboard = dilateBitboard(flood_fill_bitboard, board_size_, board_left_boundary_bitboard_,
                                                             board_right_boundary_bitboard_, board_mask_bitboard_) &
                                              ~flood_fill_bitboard;
            stone_bitboard &= ~flood_fill_bitboard;
            vcolor_reg_bitboard.push_back(flood_fill_bitboard);
            vcolor_surr_bitboard.push_back(surrounding_bitboard);
        }
        return;
    }
    static GoBitboard compute1ColorBensonBitboard(const GoPair<GoBitboard>& stone_bitboard_, int board_size_, const GoBitboard& board_left_boundary_bitboard_,
                                                  const GoBitboard& board_right_boundary_bitboard_, const GoBitboard& board_mask_bitboard_, Player player)
    {
        GoBitboard ret, empty_stone_bitboard = ~(stone_bitboard_.get(Player::kPlayer1) | stone_bitboard_.get(Player::kPlayer2)) & board_mask_bitboard_;
        std::vector<GoBitboard> vcolor_reg_bitboard, vcolor_surr_bitboard, vother_reg_bitboard, vother_surr_bitboard;
        // no stone of this player
        if (stone_bitboard_.get(player).none())
            return ret;
        // this player's stone chain
        getChain(stone_bitboard_.get(player), board_size_, board_left_boundary_bitboard_,
                 board_right_boundary_bitboard_, board_mask_bitboard_, vcolor_reg_bitboard, vcolor_surr_bitboard);
        // not this player's stone chain
        getChain(~stone_bitboard_.get(player) & board_mask_bitboard_, board_size_, board_left_boundary_bitboard_,
                 board_right_boundary_bitboard_, board_mask_bitboard_, vother_reg_bitboard, vother_surr_bitboard);
        // map of R -> surrounding Xs, X -> surrounding Rs, healthy R -> surrounding Xs, X -> surrounding healthy Rs
        std::map<int, std::set<int>> m_R_surrounding_Xs, m_X_surrounding_Rs, m_healthy_R_surrounding_Xs, m_X_surrounding_healthy_Rs;
        // initialize healthy region count = 0
        for (int j = int(vcolor_surr_bitboard.size()) - 1; j >= 0; --j)
            m_X_surrounding_healthy_Rs[j] = std::set<int>();
        // get small regions for each chain
        for (int i = vother_reg_bitboard.size() - 1; i >= 0; --i) {
            for (int j = int(vcolor_surr_bitboard.size()) - 1; j >= 0; --j) {
                if ((vother_reg_bitboard[i] & vcolor_surr_bitboard[j]).none())
                    continue;
                // check enclosed
                m_R_surrounding_Xs[i].insert(j);
                m_X_surrounding_Rs[j].insert(i);
                // check healthy
                if ((vother_reg_bitboard[i] & empty_stone_bitboard & vcolor_surr_bitboard[j]) != (vother_reg_bitboard[i] & empty_stone_bitboard))
                    continue;
                m_healthy_R_surrounding_Xs[i].insert(j);
                m_X_surrounding_healthy_Rs[j].insert(i);
            }
        }
        while (true) {
            // Benson's Algorithm step 1
            // Xs and Rs removed in each step
            std::set<int> s_rm_X, s_rm_R;
            // find Xs with healthy region < 2, store them into s_rm_x
            for (auto& XRs : m_X_surrounding_healthy_Rs) {
                if (XRs.second.size() >= 2)
                    continue;
                s_rm_X.insert(XRs.first);
            }
            // no change
            if (s_rm_X.empty())
                break;
            // remove Xs and find Rs surrounding those X's, store them into s_rm_R
            for (auto& rm_X : s_rm_X) {
                for (auto& R : m_X_surrounding_healthy_Rs[rm_X]) {
                    s_rm_R.insert(R);
                    m_healthy_R_surrounding_Xs[R].erase(rm_X);
                    m_R_surrounding_Xs[R].erase(rm_X);
                }
                for (auto& R : m_X_surrounding_Rs[rm_X]) {
                    s_rm_R.insert(R);
                    m_R_surrounding_Xs[R].erase(rm_X);
                }
                m_X_surrounding_healthy_Rs.erase(rm_X);
                m_X_surrounding_Rs.erase(rm_X);
            }
            // Benson's Algorithm step 2
            if (s_rm_R.empty())
                break;
            for (auto& rm_R : s_rm_R) {
                for (auto& X : m_healthy_R_surrounding_Xs[rm_R]) {
                    m_X_surrounding_healthy_Rs[X].erase(rm_R);
                }
                for (auto& X : m_R_surrounding_Xs[rm_R]) {
                    m_X_surrounding_Rs[X].erase(rm_R);
                }
                m_healthy_R_surrounding_Xs.erase(rm_R);
                m_R_surrounding_Xs.erase(rm_R);
            }
        }
        for (auto& RX : m_healthy_R_surrounding_Xs) {
            ret |= vother_reg_bitboard[RX.first];
        }
        for (auto& XR : m_X_surrounding_Rs) {
            ret |= vcolor_reg_bitboard[XR.first];
        }
        return ret;
    }
    static void printboard(GoBitboard b, const int b_sz, Player player)
    {
        std::string board_str = "";
        std::string piece = "O";
        if (player == Player::kPlayer2) {
            std::cout << "===========================\nwhite:\n\n";
            piece = "X";
        } else if (player == Player::kPlayer1) {
            std::cout << "===========================\nblack:\n\n";
        } else {
            std::cout << "===========================\ntotal:\n\n";
        }
        for (int i = b_sz - 1; i >= 0; --i) {
            for (int j = 0; j < b_sz; ++j) {
                if (b[i * b_sz + j] == 1)
                    std::cout << piece << " ";
                else
                    std::cout << ". ";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
        return;
    }
};

} // namespace minizero::env::go
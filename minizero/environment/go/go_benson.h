#pragma once

#include "go.h"
#include <iostream>
#include <vector>
namespace minizero::env::go
{

    class GoBenson
    {
    public:
        static GoPair<GoBitboard> GetBensonBitboard(const GoEnv &go_env)
        {
            GoPair<GoBitboard> result;
            result.set(Player::kPlayer1, Compute1ColorBensonBitboard(go_env, Player::kPlayer1));
            result.set(Player::kPlayer2, Compute1ColorBensonBitboard(go_env, Player::kPlayer2));
            return result;
        }
        static GoBitboard Compute1ColorBensonBitboard(const GoEnv &go_env, Player player)
        {
            GoBitboard empty_stone_bitboard = ~(go_env.stone_bitboard_.Get(Player::kPlayer1) | go_env.stone_bitboard_.Get(Player::kPlayer2)) & go_env.board_mask_bitboard_;
            std::vector<GoBitboard> vcolor_reg_bitboard, vcolor_surr_bitboard, vother_reg_bitboard, vother_surr_bitboard;
            std::pair<GoBitboard, GoBitboard> color_all_reg_surr_bitboard, other_all_reg_surr_bitboard;
            color_all_reg_surr_bitboard = GetChain(go_env, go_env.stone_bitboard_.Get(player), vcolor_reg_bitboard, vcolor_surr_bitboard);
            other_all_reg_surr_bitboard = GetChain(go_env, ~go_env.stone_bitboard_.Get(player) & go_env.board_mask_bitboard_, vother_reg_bitboard, vother_surr_bitboard);
            bool change = true, first = true;
            while (change)
            {
                change = first;
                if (first)
                    first = false;
                // get small regions at the first time, then Benson's Algorithm step 2
                for (int i = vother_reg_bitboard.size() - 1; i >= 0; --i)
                {
                    if ((vother_reg_bitboard[i] & empty_stone_bitboard & color_all_reg_surr_bitboard.second) != (vother_reg_bitboard[i] & empty_stone_bitboard))
                    {
                        change = true;
                        other_all_reg_surr_bitboard.first &= (~vother_reg_bitboard[i] & go_env.board_mask_bitboard_);
                        other_all_reg_surr_bitboard.second &= (~vother_surr_bitboard[i] & go_env.board_mask_bitboard_);
                        vother_reg_bitboard.erase(vother_reg_bitboard.begin() + i);
                        vother_surr_bitboard.erase(vother_surr_bitboard.begin() + i);
                    }
                }
                if (!change)
                    break;
                change = false;
                // Benson's Algorithm step 1
                for (int i = int(vcolor_surr_bitboard.size()) - 1; i >= 0; --i)
                {
                    int healthy_cnt = 0;
                    for (int j = int(vother_reg_bitboard.size()) - 1; j >= 0; --j)
                    {
                        if ((vcolor_surr_bitboard[i] & vother_reg_bitboard[j]).any())
                        {
                            ++healthy_cnt;
                            if (healthy_cnt >= 2)
                                break;
                        }
                    }
                    if (healthy_cnt < 2)
                    {
                        color_all_reg_surr_bitboard.first &= (~vcolor_reg_bitboard[i] & go_env.board_mask_bitboard_);
                        color_all_reg_surr_bitboard.second &= (~vcolor_surr_bitboard[i] & go_env.board_mask_bitboard_);
                        vcolor_surr_bitboard.erase(vcolor_surr_bitboard.begin() + i);
                        vcolor_reg_bitboard.erase(vcolor_reg_bitboard.begin() + i);
                        change = true;
                    }
                }
            }
            GoBitboard ret = color_all_reg_surr_bitboard.first | other_all_reg_surr_bitboard.first;
            Printboard(ret, go_env.board_size_, player);
            return ret;
        }
        static void Printboard(GoBitboard b, const int b_sz, Player player)
        {
            std::string board_str = "";
            std::string piece = "O";
            if (player == Player::kPlayer2)
            {
                std::cout << "===========================\nwhite:\n\n";
                piece = "X";
            }
            else
            {
                std::cout << "===========================\nblack:\n\n";
            }
            for (int i = b_sz - 1; i >= 0; --i)
            {
                for (int j = 0; j < b_sz; ++j)
                {
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
        static std::pair<GoBitboard, GoBitboard> GetChain(const GoEnv &go_env, const GoBitboard &color_stone_bitboard, std::vector<GoBitboard> &vcolor_reg_bitboard, std::vector<GoBitboard> &vcolor_surr_bitboard)
        {
            GoBitboard stone_bitboard = color_stone_bitboard;
            GoBitboard all_surr_bitboard, all_reg_bitboard;
            while (!stone_bitboard.none())
            {
                int pos = stone_bitboard._Find_first();
                // flood fill
                GoBitboard flood_fill_bitboard;
                flood_fill_bitboard.set(pos);
                bool need_dilate = true;
                while (need_dilate)
                {
                    GoBitboard dilate_bitboard = go_env.DilateBitboard(flood_fill_bitboard) & stone_bitboard;
                    need_dilate = (flood_fill_bitboard != dilate_bitboard);
                    flood_fill_bitboard = dilate_bitboard;
                }
                GoBitboard surrounding_bitboard = go_env.DilateBitboard(flood_fill_bitboard) & ~flood_fill_bitboard;
                stone_bitboard &= ~flood_fill_bitboard;

                vcolor_reg_bitboard.push_back(flood_fill_bitboard);
                all_reg_bitboard |= flood_fill_bitboard;

                vcolor_surr_bitboard.push_back(surrounding_bitboard);
                all_surr_bitboard |= surrounding_bitboard;
            }
            return std::pair<GoBitboard, GoBitboard>(all_reg_bitboard, all_surr_bitboard);
        }
    };

} // namespace minizero::env::go
#include "configuration.h"
#include <string>

namespace minizero::config {

// program parameters
int program_seed = 0;
bool program_auto_seed = false;
bool program_quiet = false;

// actor parameters
int actor_num_simulation = 50;
float actor_mcts_puct_base = 19652;
float actor_mcts_puct_init = 1.25;
float actor_mcts_reward_discount = 1.0f;
int actor_mcts_think_batch_size = 1;
float actor_mcts_think_time_limit = 0;
bool actor_mcts_value_rescale = false;
bool actor_select_action_by_count = false;
bool actor_select_action_by_softmax_count = true;
float actor_select_action_softmax_temperature = 1.0f;
bool actor_select_action_softmax_temperature_decay = false;
bool actor_use_random_rotation_features = true;
bool actor_use_dirichlet_noise = true;
float actor_dirichlet_noise_alpha = 0.03f;
float actor_dirichlet_noise_epsilon = 0.25f;
bool actor_use_gumbel = false;
bool actor_use_gumbel_noise = false;
int actor_gumbel_sample_size = 16;
float actor_gumbel_sigma_visit_c = 50;
float actor_gumbel_sigma_scale_c = 1;
float actor_resign_threshold = -0.9f;

// zero parameters
int zero_num_threads = 4;
int zero_num_parallel_games = 32;
int zero_server_port = 9999;
std::string zero_training_directory = "";
int zero_num_games_per_iteration = 5000;
int zero_start_iteration = 0;
int zero_end_iteration = 100;
int zero_replay_buffer = 20;
float zero_disable_resign_ratio = 0.1;
int zero_actor_intermediate_sequence_length = 0;
std::string zero_actor_ignored_command = "reset_actors";
bool zero_actor_stop_after_enough_games = false;
bool zero_server_accept_different_model_games = true;

// learner parameters
bool learner_use_per = false;
float learner_per_alpha = 1.0f;
float learner_per_init_beta = 1.0f;
bool learner_per_beta_anneal = true;
int learner_training_step = 500;
int learner_training_display_step = 100;
int learner_batch_size = 1024;
int learner_muzero_unrolling_step = 5;
int learner_n_step_return = 0;
float learner_learning_rate = 0.02;
float learner_momentum = 0.9;
float learner_weight_decay = 0.0001;
float learner_value_loss_scale = 1.0f;
int learner_num_thread = 8;

// network parameters
std::string nn_file_name = "";
int nn_num_input_channels = 4;
int nn_input_channel_height = 3;
int nn_input_channel_width = 3;
int nn_num_hidden_channels = 16;
int nn_hidden_channel_height = 3;
int nn_hidden_channel_width = 3;
int nn_num_action_feature_channels = 1;
int nn_num_blocks = 1;
int nn_action_size = 9;
int nn_num_value_hidden_channels = 256;
int nn_discrete_value_size = 1;
std::string nn_type_name = "alphazero";

// environment parameters
#if GO
int env_board_size = 9;
#elif HEX
int env_board_size = 11;
#elif KILLALLGO
int env_board_size = 7;
#elif OTHELLO
int env_board_size = 8;
#elif GOMOKU
int env_board_size = 15;
#elif CHESS
int env_board_size = 8;
#elif NOGO
int env_board_size = 9;
#elif TICTACTOE
int env_board_size = 3;
#elif PUZZLE2048
int env_board_size = 4;
#elif RUBIKS
int env_board_size = 3;
#else
int env_board_size = 0;
#endif

// environment parameters for specific game
std::string env_atari_rom_dir = "/opt/atari57/";
std::string env_atari_name = "ms_pacman";
float env_go_komi = 7.5;
std::string env_go_ko_rule = "positional";
std::string env_gomoku_rule = "normal";
int env_rubiks_scramble_rotate = 5;

void setConfiguration(ConfigureLoader& cl)
{
    // program parameters
    cl.addParameter("program_seed", program_seed, "", "Program");
    cl.addParameter("program_auto_seed", program_auto_seed, "", "Program");
    cl.addParameter("program_quiet", program_quiet, "", "Program");

    // actor parameters
    cl.addParameter("actor_num_simulation", actor_num_simulation, "", "Actor");
    cl.addParameter("actor_mcts_puct_base", actor_mcts_puct_base, "", "Actor");
    cl.addParameter("actor_mcts_puct_init", actor_mcts_puct_init, "", "Actor");
    cl.addParameter("actor_mcts_reward_discount", actor_mcts_reward_discount, "", "Actor");
    cl.addParameter("actor_mcts_value_rescale", actor_mcts_value_rescale, "", "Actor");
    cl.addParameter("actor_mcts_think_batch_size", actor_mcts_think_batch_size, "", "Actor");
    cl.addParameter("actor_mcts_think_time_limit", actor_mcts_think_time_limit, "MCTS time limit (in seconds), 0 represents searching without using the time limit", "Actor");
    cl.addParameter("actor_select_action_by_count", actor_select_action_by_count, "", "Actor");
    cl.addParameter("actor_select_action_by_softmax_count", actor_select_action_by_softmax_count, "", "Actor");
    cl.addParameter("actor_select_action_softmax_temperature", actor_select_action_softmax_temperature, "", "Actor");
    cl.addParameter("actor_select_action_softmax_temperature_decay", actor_select_action_softmax_temperature_decay, "decay temperature based on zero_end_iteration; use 1, 0.5, and 0.25 for 0%-50%, 50%-75%, and 75%-100% of total iterations, respectively", "Actor");
    cl.addParameter("actor_use_random_rotation_features", actor_use_random_rotation_features, "randomly rotate input features, currently only supports alphazero mode", "Actor");
    cl.addParameter("actor_use_dirichlet_noise", actor_use_dirichlet_noise, "", "Actor");
    cl.addParameter("actor_dirichlet_noise_alpha", actor_dirichlet_noise_alpha, "1 / sqrt(num of actions)", "Actor");
    cl.addParameter("actor_dirichlet_noise_epsilon", actor_dirichlet_noise_epsilon, "", "Actor");
    cl.addParameter("actor_use_gumbel", actor_use_gumbel, "", "Actor");
    cl.addParameter("actor_use_gumbel_noise", actor_use_gumbel_noise, "", "Actor");
    cl.addParameter("actor_gumbel_sample_size", actor_gumbel_sample_size, "", "Actor");
    cl.addParameter("actor_gumbel_sigma_visit_c", actor_gumbel_sigma_visit_c, "", "Actor");
    cl.addParameter("actor_gumbel_sigma_scale_c", actor_gumbel_sigma_scale_c, "", "Actor");
    cl.addParameter("actor_resign_threshold", actor_resign_threshold, "", "Actor");

    // zero parameters
    cl.addParameter("zero_num_threads", zero_num_threads, "", "Zero");
    cl.addParameter("zero_num_parallel_games", zero_num_parallel_games, "", "Zero");
    cl.addParameter("zero_server_port", zero_server_port, "", "Zero");
    cl.addParameter("zero_training_directory", zero_training_directory, "", "Zero");
    cl.addParameter("zero_num_games_per_iteration", zero_num_games_per_iteration, "", "Zero");
    cl.addParameter("zero_start_iteration", zero_start_iteration, "", "Zero");
    cl.addParameter("zero_end_iteration", zero_end_iteration, "", "Zero");
    cl.addParameter("zero_replay_buffer", zero_replay_buffer, "", "Zero");
    cl.addParameter("zero_disable_resign_ratio", zero_disable_resign_ratio, "", "Zero");
    cl.addParameter("zero_actor_intermediate_sequence_length", zero_actor_intermediate_sequence_length, "board games: 0; atari: 200", "Zero");
    cl.addParameter("zero_actor_ignored_command", zero_actor_ignored_command, "format: command1 command2 ...", "Zero");
    cl.addParameter("zero_actor_stop_after_enough_games", zero_actor_stop_after_enough_games, "", "Zero");
    cl.addParameter("zero_server_accept_different_model_games", zero_server_accept_different_model_games, "", "Zero");

    // learner parameters
    cl.addParameter("learner_use_per", learner_use_per, "Prioritized Experience Replay", "Learner");
    cl.addParameter("learner_per_alpha", learner_per_alpha, "Prioritized Experience Replay", "Learner");
    cl.addParameter("learner_per_init_beta", learner_per_init_beta, "Prioritized Experience Replay", "Learner");
    cl.addParameter("learner_per_beta_anneal", learner_per_beta_anneal, "linearly anneal PER init beta to 1 based on zero_end_iteration", "Learner");
    cl.addParameter("learner_training_step", learner_training_step, "", "Learner");
    cl.addParameter("learner_training_display_step", learner_training_display_step, "", "Learner");
    cl.addParameter("learner_batch_size", learner_batch_size, "", "Learner");
    cl.addParameter("learner_muzero_unrolling_step", learner_muzero_unrolling_step, "", "Learner");
    cl.addParameter("learner_n_step_return", learner_n_step_return, "board games: 0, atari: 10", "Learner");
    cl.addParameter("learner_learning_rate", learner_learning_rate, "", "Learner");
    cl.addParameter("learner_momentum", learner_momentum, "", "Learner");
    cl.addParameter("learner_weight_decay", learner_weight_decay, "", "Learner");
    cl.addParameter("learner_value_loss_scale", learner_value_loss_scale, "", "Learner");
    cl.addParameter("learner_num_thread", learner_num_thread, "", "Learner");

    // network parameters
    cl.addParameter("nn_file_name", nn_file_name, "", "Network");
    cl.addParameter("nn_num_input_channels", nn_num_input_channels, "", "Network");
    cl.addParameter("nn_input_channel_height", nn_input_channel_height, "", "Network");
    cl.addParameter("nn_input_channel_width", nn_input_channel_width, "", "Network");
    cl.addParameter("nn_num_hidden_channels", nn_num_hidden_channels, "", "Network");
    cl.addParameter("nn_hidden_channel_height", nn_hidden_channel_height, "", "Network");
    cl.addParameter("nn_hidden_channel_width", nn_hidden_channel_width, "", "Network");
    cl.addParameter("nn_num_action_feature_channels", nn_num_action_feature_channels, "", "Network");
    cl.addParameter("nn_num_blocks", nn_num_blocks, "", "Network");
    cl.addParameter("nn_action_size", nn_action_size, "", "Network");
    cl.addParameter("nn_num_value_hidden_channels", nn_num_value_hidden_channels, "", "Network");
    cl.addParameter("nn_discrete_value_size", nn_discrete_value_size, "set to 1 for the games which doesn't use discrete value", "Network");
    cl.addParameter("nn_type_name", nn_type_name, "alphazero/muzero", "Network");

    // environment parameters
    cl.addParameter("env_board_size", env_board_size, "", "Environment");

    // environment parameters for specific game
#if ATARI
    cl.addParameter("env_atari_rom_dir", env_atari_rom_dir, "", "Environment");
    cl.addParameter("env_atari_name", env_atari_name, "Atari 57 Games:\n"
                                                      "#\talien amidar assault asterix asteroids atlantis bank_heist battle_zone beam_rider berzerk\n"
                                                      "#\tbowling boxing breakout centipede chopper_command crazy_climber defender demon_attack double_dunk enduro\n"
                                                      "#\tfishing_derby freeway frostbite gopher gravitar hero ice_hockey jamesbond kangaroo krull\n"
                                                      "#\tkung_fu_master montezuma_revenge ms_pacman name_this_game phoenix pitfall pong private_eye qbert riverraid\n"
                                                      "#\troad_runner robotank seaquest skiing solaris space_invaders star_gunner surround tennis time_pilot\n"
                                                      "#\ttutankham up_n_down venture video_pinball wizard_of_wor yars_revenge zaxxon",
                    "Environment");
#elif GO
    cl.addParameter("env_go_komi", env_go_komi, "", "Environment");
    cl.addParameter("env_go_ko_rule", env_go_ko_rule, "positional/situational", "Environment");
#elif KILLALLGO
    cl.addParameter("env_killallgo_ko_rule", env_go_ko_rule, "positional/situational", "Environment");
#elif GOMOKU
    cl.addParameter("env_gomoku_rule", env_gomoku_rule, "normal/outer_open", "Environment");
#elif RUBIKS
    cl.addParameter("env_rubiks_scramble_rotate", env_rubiks_scramble_rotate, "", "Enviroment");
#endif
}

} // namespace minizero::config

#include "configuration.h"
#include <string>

namespace minizero::config {

// program parameters
int program_seed = 0;
bool program_auto_seed = false;
bool program_quiet = false;

// actor parameters
bool actor_use_remote_environment = false;
int actor_server_port = 9998;
int actor_num_threads = 4;
int actor_num_parallel_games = 32;
int actor_num_simulation = 50;
float actor_mcts_puct_base = 19652;
float actor_mcts_puct_init = 1.25;
float actor_mcts_reward_discount = 1.0f;
bool actor_mcts_value_rescale = false;
bool actor_select_action_by_count = false;
bool actor_select_action_by_softmax_count = true;
float actor_select_action_softmax_temperature = 1.0f;
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
int zero_server_port = 9999;
std::string zero_training_directory = "";
int zero_num_games_per_iteration = 5000;
int zero_start_iteration = 0;
int zero_end_iteration = 100;
int zero_replay_buffer = 20;
float zero_disable_resign_ratio = 0.1;
std::string zero_actor_ignored_command = "";
bool zero_server_accept_different_model_games = false;

// learner parameters
int learner_training_step = 500;
int learner_training_display_step = 100;
int learner_batch_size = 1024;
float learner_learning_rate = 0.02;
float learner_momentum = 0.9;
float learner_weight_decay = 0.0001;
int learner_num_process = 2;

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
int nn_num_action_channels = 1;
int nn_action_size = 9;
int nn_num_value_hidden_channels = 256;
int nn_discrete_value_size = 601;
std::string nn_type_name = "alphazero";

// environment parameters
std::string env_atari_name = "ALE/MsPacman-v5";
#if GO
int env_board_size = 19;
#elif HEX
int env_board_size = 11;
#elif KILLALLGO
int env_board_size = 7;
#elif OTHELLO
int env_board_size = 8;
#elif GOMOKU
int env_board_size = 15;
#else
int env_board_size = 0;
#endif
float env_go_komi = 7.5;
std::string env_go_ko_rule = "positional";

void setConfiguration(ConfigureLoader& cl)
{
    // program parameters
    cl.addParameter("program_seed", program_seed, "", "Program");
    cl.addParameter("program_auto_seed", program_auto_seed, "", "Program");
    cl.addParameter("program_quiet", program_quiet, "", "Program");

    // actor parameters
    cl.addParameter("actor_use_remote_environment", actor_use_remote_environment, "", "Actor");
    cl.addParameter("actor_server_port", actor_server_port, "", "Actor");
    cl.addParameter("actor_num_threads", actor_num_threads, "", "Actor");
    cl.addParameter("actor_num_parallel_games", actor_num_parallel_games, "", "Actor");
    cl.addParameter("actor_num_simulation", actor_num_simulation, "", "Actor");
    cl.addParameter("actor_mcts_puct_base", actor_mcts_puct_base, "", "Actor");
    cl.addParameter("actor_mcts_puct_init", actor_mcts_puct_init, "", "Actor");
    cl.addParameter("actor_mcts_reward_discount", actor_mcts_reward_discount, "", "Actor");
    cl.addParameter("actor_mcts_value_rescale", actor_mcts_value_rescale, "", "Actor");
    cl.addParameter("actor_select_action_by_count", actor_select_action_by_count, "", "Actor");
    cl.addParameter("actor_select_action_by_softmax_count", actor_select_action_by_softmax_count, "", "Actor");
    cl.addParameter("actor_select_action_softmax_temperature", actor_select_action_softmax_temperature, "", "Actor");
    cl.addParameter("actor_use_random_rotation_features", actor_use_random_rotation_features, "randomly rotate input features, currently only supports alphazero mode", "Actor");
    cl.addParameter("actor_use_dirichlet_noise", actor_use_dirichlet_noise, "", "Actor");
    cl.addParameter("actor_dirichlet_noise_alpha", actor_dirichlet_noise_alpha, "", "Actor");
    cl.addParameter("actor_dirichlet_noise_epsilon", actor_dirichlet_noise_epsilon, "", "Actor");
    cl.addParameter("actor_use_gumbel", actor_use_gumbel, "", "Actor");
    cl.addParameter("actor_use_gumbel_noise", actor_use_gumbel_noise, "", "Actor");
    cl.addParameter("actor_gumbel_sample_size", actor_gumbel_sample_size, "", "Actor");
    cl.addParameter("actor_gumbel_sigma_visit_c", actor_gumbel_sigma_visit_c, "", "Actor");
    cl.addParameter("actor_gumbel_sigma_scale_c", actor_gumbel_sigma_scale_c, "", "Actor");
    cl.addParameter("actor_resign_threshold", actor_resign_threshold, "", "Actor");

    // zero parameters
    cl.addParameter("zero_server_port", zero_server_port, "", "Zero");
    cl.addParameter("zero_training_directory", zero_training_directory, "", "Zero");
    cl.addParameter("zero_num_games_per_iteration", zero_num_games_per_iteration, "", "Zero");
    cl.addParameter("zero_start_iteration", zero_start_iteration, "", "Zero");
    cl.addParameter("zero_end_iteration", zero_end_iteration, "", "Zero");
    cl.addParameter("zero_replay_buffer", zero_replay_buffer, "", "Zero");
    cl.addParameter("zero_disable_resign_ratio", zero_disable_resign_ratio, "", "Zero");
    cl.addParameter("zero_actor_ignored_command", zero_actor_ignored_command, "format: command1 command2 ...", "Zero");
    cl.addParameter("zero_server_accept_different_model_games", zero_server_accept_different_model_games, "", "Zero");

    // learner parameters
    cl.addParameter("learner_training_step", learner_training_step, "", "Learner");
    cl.addParameter("learner_training_display_step", learner_training_display_step, "", "Learner");
    cl.addParameter("learner_batch_size", learner_batch_size, "", "Learner");
    cl.addParameter("learner_learning_rate", learner_learning_rate, "", "Learner");
    cl.addParameter("learner_momentum", learner_momentum, "", "Learner");
    cl.addParameter("learner_weight_decay", learner_weight_decay, "", "Learner");
    cl.addParameter("learner_num_process", learner_num_process, "", "Learner");

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
    cl.addParameter("nn_num_action_channels", nn_num_action_channels, "", "Network");
    cl.addParameter("nn_action_size", nn_action_size, "", "Network");
    cl.addParameter("nn_num_value_hidden_channels", nn_num_value_hidden_channels, "", "Network");
    cl.addParameter("nn_discrete_value_size", nn_discrete_value_size, "", "Network");
    cl.addParameter("nn_type_name", nn_type_name, "", "Network");

    // environment parameters
    cl.addParameter("env_board_size", env_board_size, "", "Environment");
#if ATARI
    cl.addParameter("env_atari_name", env_atari_name, "", "Environment");
#elif GO
    cl.addParameter("env_go_komi", env_go_komi, "", "Environment");
    cl.addParameter("env_go_ko_rule", env_go_ko_rule, "positional/situational", "Environment");
#elif KILLALLGO
    cl.addParameter("env_killallgo_ko_rule", env_go_ko_rule, "positional/situational", "Environment");
#endif
}

} // namespace minizero::config

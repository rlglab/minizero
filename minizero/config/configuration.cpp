#include "configuration.h"

namespace minizero::config {

int seed = 0;
bool auto_seed = false;

// actor parameters
int actor_num_threads = 4;
int actor_num_parallel_games = 32;
int actor_num_simulation = 50;
float actor_mcts_puct_base = 19652;
float actor_mcts_puct_init = 1.25;
bool actor_select_action_by_count = false;
bool actor_select_action_by_softmax_count = true;
float actor_select_action_softmax_temperature = 1.0f;
bool actor_use_dirichlet_noise = true;
float actor_dirichlet_noise_alpha = 0.03f;
float actor_dirichlet_noise_epsilon = 0.25f;
float actor_resign_threshold = -0.9f;

// zero parameters
int zero_server_port = 9999;
std::string zero_training_directory;
int zero_num_games_per_iteration = 5000;
int zero_start_iteration = 0;
int zero_end_iteration = 100;
int zero_replay_buffer = 20;
float zero_disable_resign_ratio = 0.1;

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
std::string nn_type_name = "alphazero";

// environment parameters
int env_go_board_size = 19;
float env_go_komi = 7.5;
std::string env_go_ko_rule = "positional";

void setConfiguration(ConfigureLoader& cl)
{
    cl.addParameter("seed", seed, "", "Actor");
    cl.addParameter("auto_seed", auto_seed, "", "Actor");

    // actor parameters
    cl.addParameter("actor_num_threads", actor_num_threads, "", "Actor");
    cl.addParameter("actor_num_parallel_games", actor_num_parallel_games, "", "Actor");
    cl.addParameter("actor_num_simulation", actor_num_simulation, "", "Actor");
    cl.addParameter("actor_mcts_puct_base", actor_mcts_puct_base, "", "Actor");
    cl.addParameter("actor_mcts_puct_init", actor_mcts_puct_init, "", "Actor");
    cl.addParameter("actor_select_action_by_count", actor_select_action_by_count, "", "Actor");
    cl.addParameter("actor_select_action_by_softmax_count", actor_select_action_by_softmax_count, "", "Actor");
    cl.addParameter("actor_select_action_softmax_temperature", actor_select_action_softmax_temperature, "", "Actor");
    cl.addParameter("actor_use_dirichlet_noise", actor_use_dirichlet_noise, "", "Actor");
    cl.addParameter("actor_dirichlet_noise_alpha", actor_dirichlet_noise_alpha, "", "Actor");
    cl.addParameter("actor_dirichlet_noise_epsilon", actor_dirichlet_noise_epsilon, "", "Actor");
    cl.addParameter("actor_resign_threshold", actor_resign_threshold, "", "Actor");

    // zero parameters
    cl.addParameter("zero_server_port", zero_server_port, "", "Zero");
    cl.addParameter("zero_training_directory", zero_training_directory, "", "Zero");
    cl.addParameter("zero_num_games_per_iteration", zero_num_games_per_iteration, "", "Zero");
    cl.addParameter("zero_start_iteration", zero_start_iteration, "", "Zero");
    cl.addParameter("zero_end_iteration", zero_end_iteration, "", "Zero");
    cl.addParameter("zero_replay_buffer", zero_replay_buffer, "", "Zero");
    cl.addParameter("zero_disable_resign_ratio", zero_disable_resign_ratio, "", "Zero");

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
    cl.addParameter("nn_type_name", nn_type_name, "", "Network");

    // environment parameters
#if GO
    cl.addParameter("env_go_board_size", env_go_board_size, "", "Environment");
    cl.addParameter("env_go_komi", env_go_komi, "", "Environment");
    cl.addParameter("env_go_ko_rule", env_go_ko_rule, "positional/situational", "Environment");
#endif
}

} // namespace minizero::config

#pragma once

#include "configure_loader.h"

namespace minizero::config {

extern int seed;
extern bool auto_seed;

// actor parameters
extern int actor_num_threads;
extern int actor_num_parallel_games;
extern int actor_num_simulation;
extern float actor_mcts_puct_base;
extern float actor_mcts_puct_init;
extern bool actor_select_action_by_count;
extern bool actor_select_action_by_softmax_count;
extern float actor_select_action_softmax_temperature;
extern bool actor_use_dirichlet_noise;
extern float actor_dirichlet_noise_alpha;
extern float actor_dirichlet_noise_epsilon;
extern bool actor_use_gumbel_noise;
extern int actor_gumbel_sample_size;
extern float actor_gumbel_sigma_visit_c;
extern float actor_gumbel_sigma_scale_c;
extern float actor_resign_threshold;

// zero parameters
extern int zero_server_port;
extern std::string zero_training_directory;
extern int zero_num_games_per_iteration;
extern int zero_start_iteration;
extern int zero_end_iteration;
extern int zero_replay_buffer;
extern float zero_disable_resign_ratio;

// learner parameters
extern int learner_training_step;
extern int learner_training_display_step;
extern int learner_batch_size;
extern float learner_learning_rate;
extern float learner_momentum;
extern float learner_weight_decay;
extern int learner_num_process;

// network parameters
extern std::string nn_file_name;
extern int nn_num_input_channels;
extern int nn_input_channel_height;
extern int nn_input_channel_width;
extern int nn_num_hidden_channels;
extern int nn_hidden_channel_height;
extern int nn_hidden_channel_width;
extern int nn_num_action_feature_channels;
extern int nn_num_blocks;
extern int nn_num_action_channels;
extern int nn_action_size;
extern int nn_num_value_hidden_channels;
extern std::string nn_type_name;

// environment parameters
extern int env_go_board_size;
extern float env_go_komi;
extern std::string env_go_ko_rule;
extern bool env_killallgo_use_rzone;

void setConfiguration(ConfigureLoader& cl);

} // namespace minizero::config

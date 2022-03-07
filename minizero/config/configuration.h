#pragma once

#include "configure_loader.h"

namespace minizero::config {

extern int seed;
extern bool auto_seed;
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

extern int zero_server_port;
extern std::string zero_training_directory;
extern int zero_num_games_per_iteration;
extern int zero_start_iteration;
extern int zero_end_iteration;
extern int zero_replay_buffer;

extern int learner_training_step;
extern int learner_training_display_step;
extern int learner_batch_size;
extern float learner_learning_rate;
extern float learner_momentum;
extern float learner_weight_decay;
extern int learner_num_process;

extern std::string nn_file_name;
extern int nn_num_input_channels;
extern int nn_input_channel_height;
extern int nn_input_channel_width;
extern int nn_num_hidden_channels;
extern int nn_hidden_channel_height;
extern int nn_hidden_channel_width;
extern int nn_num_blocks;
extern int nn_num_action_channels;
extern int nn_action_size;
extern std::string nn_type_name;

void SetConfiguration(ConfigureLoader& cl);

} // namespace minizero::config
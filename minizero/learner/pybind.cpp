#include "configuration.h"
#include "data_loader.h"
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;
using namespace minizero;

PYBIND11_MODULE(minizero_py, m)
{
    m.def("load_config_file", [](std::string file_name) {
        minizero::env::setUpEnv();
        minizero::config::ConfigureLoader cl;
        minizero::config::setConfiguration(cl);
        cl.loadFromFile(file_name);
    });
    m.def("load_config_string", [](std::string conf_str) {
        minizero::config::ConfigureLoader cl;
        minizero::config::setConfiguration(cl);
        return cl.loadFromString(conf_str);
    });
    m.def("use_gumbel", []() { return config::actor_use_gumbel; });
    m.def("get_zero_replay_buffer", []() { return config::zero_replay_buffer; });
    m.def("use_per", []() { return config::learner_use_per; });
    m.def("get_training_step", []() { return config::learner_training_step; });
    m.def("get_training_display_step", []() { return config::learner_training_display_step; });
    m.def("get_batch_size", []() { return config::learner_batch_size; });
    m.def("get_muzero_unrolling_step", []() { return config::learner_muzero_unrolling_step; });
    m.def("get_n_step_return", []() { return config::learner_n_step_return; });
    m.def("get_learning_rate", []() { return config::learner_learning_rate; });
    m.def("get_momentum", []() { return config::learner_momentum; });
    m.def("get_weight_decay", []() { return config::learner_weight_decay; });
    m.def("get_game_name", []() { return Environment().name(); });
    m.def("get_nn_num_input_channels", []() { return config::nn_num_input_channels; });
    m.def("get_nn_input_channel_height", []() { return config::nn_input_channel_height; });
    m.def("get_nn_input_channel_width", []() { return config::nn_input_channel_width; });
    m.def("get_nn_num_hidden_channels", []() { return config::nn_num_hidden_channels; });
    m.def("get_nn_hidden_channel_height", []() { return config::nn_hidden_channel_height; });
    m.def("get_nn_hidden_channel_width", []() { return config::nn_hidden_channel_width; });
    m.def("get_nn_num_action_feature_channels", []() { return config::nn_num_action_feature_channels; });
    m.def("get_nn_num_blocks", []() { return config::nn_num_blocks; });
    m.def("get_nn_action_size", []() { return config::nn_action_size; });
    m.def("get_nn_num_value_hidden_channels", []() { return config::nn_num_value_hidden_channels; });
    m.def("get_nn_discrete_value_size", []() { return config::nn_discrete_value_size; });
    m.def("get_nn_type_name", []() { return config::nn_type_name; });

    py::class_<learner::DataLoader>(m, "DataLoader")
        .def(py::init<std::string>())
        .def("initialize", &learner::DataLoader::initialize)
        .def("load_data_from_file", &learner::DataLoader::loadDataFromFile, py::call_guard<py::gil_scoped_release>())
        .def(
            "update_priority", [](learner::DataLoader& data_loader, py::array_t<int>& sampled_index, py::array_t<float>& batch_values) {
                data_loader.updatePriority(static_cast<int*>(sampled_index.request().ptr), static_cast<float*>(batch_values.request().ptr));
            },
            py::call_guard<py::gil_scoped_release>())
        .def(
            "sample_data", [](learner::DataLoader& data_loader, py::array_t<float>& features, py::array_t<float>& action_features, py::array_t<float>& policy, py::array_t<float>& value, py::array_t<float>& reward, py::array_t<float>& loss_scale, py::array_t<int>& sampled_index) {
                data_loader.getSharedData()->getDataPtr()->features_ = static_cast<float*>(features.request().ptr);
                data_loader.getSharedData()->getDataPtr()->action_features_ = static_cast<float*>(action_features.request().ptr);
                data_loader.getSharedData()->getDataPtr()->policy_ = static_cast<float*>(policy.request().ptr);
                data_loader.getSharedData()->getDataPtr()->value_ = static_cast<float*>(value.request().ptr);
                data_loader.getSharedData()->getDataPtr()->reward_ = static_cast<float*>(reward.request().ptr);
                data_loader.getSharedData()->getDataPtr()->loss_scale_ = static_cast<float*>(loss_scale.request().ptr);
                data_loader.getSharedData()->getDataPtr()->sampled_index_ = static_cast<int*>(sampled_index.request().ptr);
                data_loader.sampleData();
            },
            py::call_guard<py::gil_scoped_release>());
}

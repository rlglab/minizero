#include "configuration.h"
#include "data_loader.h"
#include "environment.h"
#include <algorithm>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace py = pybind11;
using namespace minizero;

std::shared_ptr<Environment> kEnvInstance;

namespace {

minizero::env::Player playerFromInt(int player)
{
    switch (player) {
        case 1: return minizero::env::Player::kPlayer1;
        case 2: return minizero::env::Player::kPlayer2;
        default: throw std::invalid_argument("player must be 1 or 2");
    }
}

utils::Rotation rotationFromInt(int rotation)
{
    if (rotation < 0 || rotation >= static_cast<int>(utils::Rotation::kRotateSize)) {
        throw std::invalid_argument("rotation must be in [0, 7]; 0 means no rotation");
    }
    return static_cast<utils::Rotation>(rotation);
}

bool isPolicyActionID(const Environment& env, int action_id)
{
    return action_id >= 0 && action_id < env.getPolicySize();
}

std::vector<int> getActionIDs(const std::vector<Action>& actions)
{
    std::vector<int> action_ids;
    action_ids.reserve(actions.size());
    for (const auto& action : actions) { action_ids.push_back(action.getActionID()); }
    return action_ids;
}

py::array_t<float> makeFloatArray(const std::vector<float>& values, const std::vector<py::ssize_t>& shape)
{
    py::ssize_t expected_size = 1;
    for (py::ssize_t dim : shape) { expected_size *= dim; }
    if (static_cast<py::ssize_t>(values.size()) != expected_size) {
        throw std::runtime_error("array size does not match requested shape");
    }

    py::array_t<float> array(shape);
    py::buffer_info buffer = array.request();
    std::copy(values.begin(), values.end(), static_cast<float*>(buffer.ptr));
    return array;
}

std::vector<py::ssize_t> getInputShape(const Environment& env)
{
    return {env.getNumInputChannels(), env.getInputChannelHeight(), env.getInputChannelWidth()};
}

std::vector<py::ssize_t> getActionFeatureShape(const Environment& env)
{
    return {env.getNumActionFeatureChannels(), env.getHiddenChannelHeight(), env.getHiddenChannelWidth()};
}

std::vector<int> getActionHistoryIDs(const Environment& env)
{
    return getActionIDs(env.getActionHistory());
}

} // namespace

Environment& getEnvInstance()
{
    if (!kEnvInstance) { kEnvInstance = std::make_shared<Environment>(); }
    return *kEnvInstance;
}

PYBIND11_MODULE(minizero_py, m)
{
    m.def("load_config_file", [](std::string file_name) {
        minizero::env::setUpEnv();
        minizero::config::ConfigureLoader cl;
        minizero::config::setConfiguration(cl);
        bool success = cl.loadFromFile(file_name);
        if (success) { kEnvInstance = std::make_shared<Environment>(); }
        return success;
    });
    m.def("load_config_string", [](std::string conf_str) {
        minizero::config::ConfigureLoader cl;
        minizero::config::setConfiguration(cl);
        bool success = cl.loadFromString(conf_str);
        if (success) { kEnvInstance = std::make_shared<Environment>(); }
        return success;
    });
    m.def("use_gumbel", []() { return config::actor_use_gumbel; });
    m.def("get_zero_replay_buffer", []() { return config::zero_replay_buffer; });
    m.def("use_per", []() { return config::learner_use_per; });
    m.def("get_training_step", []() { return config::learner_training_step; });
    m.def("get_training_display_step", []() { return config::learner_training_display_step; });
    m.def("get_batch_size", []() { return config::learner_batch_size; });
    m.def("get_muzero_unrolling_step", []() { return config::learner_muzero_unrolling_step; });
    m.def("get_n_step_return", []() { return config::learner_n_step_return; });
    m.def("get_optimizer", []() { return config::learner_optimizer; });
    m.def("get_learning_rate", []() { return config::learner_learning_rate; });
    m.def("get_momentum", []() { return config::learner_momentum; });
    m.def("get_weight_decay", []() { return config::learner_weight_decay; });
    m.def("get_value_loss_scale", []() { return config::learner_value_loss_scale; });
    m.def("get_game_name", []() { return getEnvInstance().name(); });
    m.def("get_nn_num_input_channels", []() { return getEnvInstance().getNumInputChannels(); });
    m.def("get_nn_input_channel_height", []() { return getEnvInstance().getInputChannelHeight(); });
    m.def("get_nn_input_channel_width", []() { return getEnvInstance().getInputChannelWidth(); });
    m.def("get_nn_num_hidden_channels", []() { return config::nn_num_hidden_channels; });
    m.def("get_nn_hidden_channel_height", []() { return getEnvInstance().getHiddenChannelHeight(); });
    m.def("get_nn_hidden_channel_width", []() { return getEnvInstance().getHiddenChannelWidth(); });
    m.def("get_nn_num_action_feature_channels", []() { return getEnvInstance().getNumActionFeatureChannels(); });
    m.def("get_nn_num_blocks", []() { return config::nn_num_blocks; });
    m.def("get_nn_action_size", []() { return getEnvInstance().getPolicySize(); });
    m.def("get_nn_num_value_hidden_channels", []() { return config::nn_num_value_hidden_channels; });
    m.def("get_nn_discrete_value_size", []() { return kEnvInstance->getDiscreteValueSize(); });
    m.def("get_nn_type_name", []() { return config::nn_type_name; });

    py::class_<Environment>(m, "Environment", "MiniZero environment for the compiled GAME_TYPE.")
        .def(py::init<>())
        .def("reset", &Environment::reset)
        .def(
            "act",
            [](Environment& env, int action_id) {
                if (!isPolicyActionID(env, action_id)) { return false; }
                return env.act(Action(action_id, env.getTurn()));
            },
            py::arg("action_id"))
        .def(
            "act",
            [](Environment& env, int action_id, int player) {
                if (!isPolicyActionID(env, action_id)) { return false; }
                return env.act(Action(action_id, playerFromInt(player)));
            },
            py::arg("action_id"),
            py::arg("player"))
        .def("legal_actions", [](const Environment& env) { return getActionIDs(env.getLegalActions()); })
        .def(
            "is_legal_action",
            [](const Environment& env, int action_id) {
                if (!isPolicyActionID(env, action_id)) { return false; }
                return env.isLegalAction(Action(action_id, env.getTurn()));
            },
            py::arg("action_id"))
        .def(
            "is_legal_action",
            [](const Environment& env, int action_id, int player) {
                if (!isPolicyActionID(env, action_id)) { return false; }
                return env.isLegalAction(Action(action_id, playerFromInt(player)));
            },
            py::arg("action_id"),
            py::arg("player"))
        .def("is_terminal", &Environment::isTerminal)
        .def("turn", [](const Environment& env) { return static_cast<int>(env.getTurn()); })
        .def("reward", &Environment::getReward)
        .def("eval_score", &Environment::getEvalScore, py::arg("is_resign") = false)
        .def(
            "features",
            [](const Environment& env, int rotation) {
                return makeFloatArray(env.getFeatures(rotationFromInt(rotation)), getInputShape(env));
            },
            py::arg("rotation") = 0)
        .def(
            "action_features",
            [](const Environment& env, int action_id, int rotation) {
                if (!isPolicyActionID(env, action_id)) { throw std::invalid_argument("action_id out of policy range"); }
                return makeFloatArray(env.getActionFeatures(Action(action_id, env.getTurn()), rotationFromInt(rotation)), getActionFeatureShape(env));
            },
            py::arg("action_id"),
            py::arg("rotation") = 0)
        .def("action_history", &getActionHistoryIDs)
        .def("name", &Environment::name)
        .def("policy_size", &Environment::getPolicySize)
        .def("num_players", &Environment::getNumPlayer)
        .def(
            "input_shape",
            [](const Environment& env) {
                return std::vector<int>{
                    env.getNumInputChannels(),
                    env.getInputChannelHeight(),
                    env.getInputChannelWidth()};
            })
        .def(
            "action_feature_shape",
            [](const Environment& env) {
                return std::vector<int>{
                    env.getNumActionFeatureChannels(),
                    env.getHiddenChannelHeight(),
                    env.getHiddenChannelWidth()};
            })
        .def(
            "hidden_shape",
            [](const Environment& env) {
                return std::vector<int>{
                    config::nn_num_hidden_channels,
                    env.getHiddenChannelHeight(),
                    env.getHiddenChannelWidth()};
            })
        .def("discrete_value_size", &Environment::getDiscreteValueSize)
        .def("to_string", &Environment::toString)
        .def("__str__", &Environment::toString);

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

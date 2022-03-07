#include "configuration.h"
#include "data_loader.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;
using namespace minizero;
using namespace minizero::learner;

class Conf {
public:
    Conf(std::string file_name)
    {
        minizero::env::SetUpEnv();
        minizero::config::ConfigureLoader cl;
        minizero::config::SetConfiguration(cl);
        cl.LoadFromFile(file_name);
    }

    inline int GetTrainingStep() const { return config::learner_training_step; }
    inline int GetTrainingDisplayStep() const { return config::learner_training_display_step; }
    inline int GetBatchSize() const { return config::learner_batch_size; }
    inline int GetLearningRate() const { return config::learner_learning_rate; }
    inline int GetMomentum() const { return config::learner_momentum; }
    inline int GetWeightDecay() const { return config::learner_weight_decay; }
    inline int GetNumProcess() const { return config::learner_num_process; }
    inline std::string GetGameName() const { return Environment().Name(); }
    inline int GetNNNumInputChannels() const { return config::nn_num_input_channels; }
    inline int GetNNInputChannelHeight() const { return config::nn_input_channel_height; }
    inline int GetNNInputChannelWidth() const { return config::nn_input_channel_width; }
    inline int GetNNNumHiddenChannels() const { return config::nn_num_hidden_channels; }
    inline int GetNNHiddenChannelHeight() const { return config::nn_hidden_channel_height; }
    inline int GetNNHiddenChannelWidth() const { return config::nn_hidden_channel_width; }
    inline int GetNNNumBlocks() const { return config::nn_num_blocks; }
    inline int GetNNNumActionChannels() const { return config::nn_num_action_channels; }
    inline int GetNNActionSize() const { return config::nn_action_size; }
    inline std::string GetNNTypeName() const { return config::nn_type_name; }
};

PYBIND11_MODULE(minizero_py, m)
{
    py::class_<Conf>(m, "Conf")
        .def(py::init<std::string>())
        .def("get_training_step", &Conf::GetTrainingStep)
        .def("get_training_display_step", &Conf::GetTrainingDisplayStep)
        .def("get_batch_size", &Conf::GetBatchSize)
        .def("get_learning_rate", &Conf::GetLearningRate)
        .def("get_momentum", &Conf::GetMomentum)
        .def("get_weight_decay", &Conf::GetWeightDecay)
        .def("get_num_process", &Conf::GetNumProcess)
        .def("get_game_name", &Conf::GetGameName)
        .def("get_nn_num_input_channels", &Conf::GetNNNumInputChannels)
        .def("get_nn_input_channel_height", &Conf::GetNNInputChannelHeight)
        .def("get_nn_input_channel_width", &Conf::GetNNInputChannelWidth)
        .def("get_nn_num_hidden_channels", &Conf::GetNNNumHiddenChannels)
        .def("get_nn_hidden_channel_height", &Conf::GetNNHiddenChannelHeight)
        .def("get_nn_hidden_channel_width", &Conf::GetNNHiddenChannelWidth)
        .def("get_nn_num_blocks", &Conf::GetNNNumBlocks)
        .def("get_nn_num_action_channels", &Conf::GetNNNumActionChannels)
        .def("get_nn_action_size", &Conf::GetNNActionSize)
        .def("get_nn_type_name", &Conf::GetNNTypeName);

    py::class_<DataLoader>(m, "DataLoader")
        .def(py::init<>())
        .def("load_data_from_directory", &DataLoader::LoadDataFromDirectory)
        .def("load_data_from_file", &DataLoader::LoadDataFromFile)
        .def("get_data_size", &DataLoader::GetDataSize)
        .def("get_feature_and_label", [](DataLoader& data_loader, int index) {
            data_loader.CalculateFeaturesAndLabel(index);
            py::dict res;
            res["features"] = py::cast(data_loader.GetFeatures());
            res["policy"] = py::cast(data_loader.GetPolicy());
            res["value"] = data_loader.GetValue();
            return res;
        });
}
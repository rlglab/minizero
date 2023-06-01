#pragma once

#include <memory>
#include <string>
#include <torch/script.h>
#include <vector>

namespace minizero::network {

class NetworkOutput {
public:
    virtual ~NetworkOutput() = default;
};

class Network {
public:
    Network();
    virtual ~Network() = default;

    virtual void loadModel(const std::string& nn_file_name, const int gpu_id);
    virtual std::string toString() const;

    inline int getGPUID() const { return gpu_id_; }
    inline int getNumInputChannels() const { return num_input_channels_; }
    inline int getInputChannelHeight() const { return input_channel_height_; }
    inline int getInputChannelWidth() const { return input_channel_width_; }
    inline int getNumHiddenChannels() const { return num_hidden_channels_; }
    inline int getHiddenChannelHeight() const { return hidden_channel_height_; }
    inline int getHiddenChannelWidth() const { return hidden_channel_width_; }
    inline int getNumBlocks() const { return num_blocks_; }
    inline int getActionSize() const { return action_size_; }
    inline int getNumValueHiddenChannels() const { return num_value_hidden_channels_; }
    inline int getDiscreteValueSize() const { return discrete_value_size_; }
    inline std::string getGameName() const { return game_name_; }
    inline std::string getNetworkTypeName() const { return network_type_name_; }
    inline std::string getNetworkFileName() const { return network_file_name_; }

protected:
    inline torch::Device getDevice() const { return (gpu_id_ == -1 ? torch::Device("cpu") : torch::Device(torch::kCUDA, gpu_id_)); }

    int gpu_id_;
    int num_input_channels_;
    int input_channel_height_;
    int input_channel_width_;
    int num_hidden_channels_;
    int hidden_channel_height_;
    int hidden_channel_width_;
    int num_blocks_;
    int action_size_;
    int num_value_hidden_channels_;
    int discrete_value_size_;
    std::string game_name_;
    std::string network_type_name_;
    std::string network_file_name_;
    torch::jit::script::Module network_;
};

} // namespace minizero::network

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

    void LoadModel(const std::string& nn_file_name, const int gpu_id);
    std::string ToString() const;

    inline int GetGPUID() const { return gpu_id_; }
    inline int GetNumInputChannels() const { return num_input_channels_; }
    inline int GetInputChannelHeight() const { return input_channel_height_; }
    inline int GetInputChannelWidth() const { return input_channel_width_; }
    inline int GetNumHiddenChannels() const { return num_hidden_channels_; }
    inline int GetHiddenChannelHeight() const { return hidden_channel_height_; }
    inline int GetHiddenChannelWidth() const { return hidden_channel_width_; }
    inline int GetNumBlocks() const { return num_blocks_; }
    inline std::string GetGameName() const { return game_name_; }
    inline std::string GetNetworkTypeName() const { return network_type_name_; }
    inline std::string GetNetworkFileName() const { return network_file_name_; }

protected:
    inline torch::Device GetDevice() const { return (gpu_id_ == -1 ? torch::Device("cpu") : torch::Device(torch::kCUDA, gpu_id_)); }

    int gpu_id_;
    int num_input_channels_;
    int input_channel_height_;
    int input_channel_width_;
    int num_hidden_channels_;
    int hidden_channel_height_;
    int hidden_channel_width_;
    int num_blocks_;
    std::string game_name_;
    std::string network_type_name_;
    std::string network_file_name_;
    torch::jit::script::Module network_;
};

std::shared_ptr<Network> CreateNetwork(std::string nn_file_name, const int gpu_id);

} // namespace minizero::network
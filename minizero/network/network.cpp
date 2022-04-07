#include "network.h"
#include "alphazero_network.h"
#include "muzero_network.h"

namespace minizero::network {

Network::Network()
{
    gpu_id_ = -1;
    num_input_channels_ = input_channel_height_ = input_channel_width_ = -1;
    num_hidden_channels_ = hidden_channel_height_ = hidden_channel_width_ = -1;
    num_blocks_ = action_size_ = -1;
    network_type_name_ = network_file_name_ = "";
}

void Network::loadModel(const std::string& nn_file_name, const int gpu_id)
{
    gpu_id_ = gpu_id;
    network_file_name_ = nn_file_name;

    // load model weights
    try {
        network_ = torch::jit::load(network_file_name_, getDevice());
        network_.eval();
    } catch (const c10::Error& e) {
        assert(false);
    }

    // network hyper-parameter
    std::vector<torch::jit::IValue> dummy;
    num_input_channels_ = network_.get_method("get_num_input_channels")(dummy).toInt();
    input_channel_height_ = network_.get_method("get_input_channel_height")(dummy).toInt();
    input_channel_width_ = network_.get_method("get_input_channel_width")(dummy).toInt();
    num_hidden_channels_ = network_.get_method("get_num_hidden_channels")(dummy).toInt();
    hidden_channel_height_ = network_.get_method("get_hidden_channel_height")(dummy).toInt();
    hidden_channel_width_ = network_.get_method("get_hidden_channel_width")(dummy).toInt();
    num_blocks_ = network_.get_method("get_num_blocks")(dummy).toInt();
    action_size_ = network_.get_method("get_action_size")(dummy).toInt();
    game_name_ = network_.get_method("get_game_name")(dummy).toString()->string();
    network_type_name_ = network_.get_method("get_type_name")(dummy).toString()->string();
}

std::string Network::toString() const
{
    std::ostringstream oss;
    oss << "GPU ID: " << gpu_id_ << std::endl;
    oss << "Number of input channels: " << num_input_channels_ << std::endl;
    oss << "Input channel height: " << input_channel_height_ << std::endl;
    oss << "Input channel width: " << input_channel_width_ << std::endl;
    oss << "Number of hidden channels: " << num_hidden_channels_ << std::endl;
    oss << "Hidden channel height: " << hidden_channel_height_ << std::endl;
    oss << "Hidden channel width: " << hidden_channel_width_ << std::endl;
    oss << "Number of blocks: " << num_blocks_ << std::endl;
    oss << "Action size: " << action_size_ << std::endl;
    oss << "Game name: " << game_name_ << std::endl;
    oss << "Network type name: " << network_type_name_ << std::endl;
    oss << "Network file name: " << network_file_name_ << std::endl;
    return oss.str();
}

std::shared_ptr<Network> createNetwork(const std::string& nn_file_name, const int gpu_id)
{
    // TODO: how to speed up?
    Network base_network;
    base_network.loadModel(nn_file_name, -1);

    std::shared_ptr<Network> network;
    if (base_network.getNetworkTypeName() == "alphazero") {
        network = std::make_shared<AlphaZeroNetwork>();
        std::dynamic_pointer_cast<AlphaZeroNetwork>(network)->loadModel(nn_file_name, gpu_id);
    } else if (base_network.getNetworkTypeName() == "muzero") {
        network = std::make_shared<MuZeroNetwork>();
        std::dynamic_pointer_cast<MuZeroNetwork>(network)->loadModel(nn_file_name, gpu_id);
    } else {
        // should not be here
        assert(false);
    }

    return network;
}

} // namespace minizero::network
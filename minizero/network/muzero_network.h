#pragma once

#include "network.h"
#include <mutex>

namespace minizero::network {

class MuZeroNetworkOutput : public NetworkOutput {
public:
    float value_;
    std::vector<float> policy_;
    std::vector<float> hidden_state_;
};

class MuZeroNetwork : public Network {
public:
    MuZeroNetwork()
    {
        num_action_channels_ = action_size_ = -1;
        initial_input_batch_size_ = recurrent_input_batch_size_ = 0;
    }

    void LoadModel(const std::string& nn_file_name, const int gpu_id)
    {
        Network::LoadModel(nn_file_name, gpu_id);

        std::vector<torch::jit::IValue> dummy;
        num_action_channels_ = network_.get_method("get_num_action_channels")(dummy).toInt();
        action_size_ = network_.get_method("get_action_size")(dummy).toInt();
        initial_input_batch_size_ = 0;
        recurrent_input_batch_size_ = 0;
    }

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << Network::ToString();
        oss << "Number of action channels: " << num_action_channels_ << std::endl;
        oss << "Action size: " << action_size_ << std::endl;
        return oss.str();
    }

    int PushBackInitialData(std::vector<float> features)
    {
        assert(static_cast<int>(features.size()) == GetNumInputChannels() * GetInputChannelHeight() * GetInputChannelWidth());

        int index;
        {
            std::lock_guard<std::mutex> lock(initial_mutex_);
            index = initial_input_batch_size_++;
            initial_tensor_input_.resize(initial_input_batch_size_);
        }
        initial_tensor_input_[index] = torch::from_blob(features.data(), {1, GetNumInputChannels(), GetInputChannelHeight(), GetInputChannelWidth()}).clone().to(GetDevice());
        return index;
    }

    int PushBackRecurrentData(std::vector<float> features, std::vector<float> actions)
    {
        assert(static_cast<int>(features.size()) == GetNumHiddenChannels() * GetHiddenChannelHeight() * GetHiddenChannelWidth());
        assert(static_cast<int>(actions.size()) == GetNumActionChannels() * GetHiddenChannelHeight() * GetHiddenChannelWidth());

        int index;
        {
            std::lock_guard<std::mutex> lock(recurrent_mutex_);
            index = recurrent_input_batch_size_++;
            recurrent_tensor_feature_input_.resize(recurrent_input_batch_size_);
            recurrent_tensor_action_input_.resize(recurrent_input_batch_size_);
        }
        recurrent_tensor_feature_input_[index] = torch::from_blob(features.data(), {1, GetNumHiddenChannels(), GetHiddenChannelHeight(), GetHiddenChannelWidth()}).clone().to(GetDevice());
        recurrent_tensor_action_input_[index] = torch::from_blob(actions.data(), {1, GetNumActionChannels(), GetHiddenChannelHeight(), GetHiddenChannelWidth()}).clone().to(GetDevice());
        return index;
    }

    inline std::vector<MuZeroNetworkOutput> InitialInference()
    {
        auto outputs = forward("initial_inference", {torch::cat(initial_tensor_input_)}, initial_input_batch_size_);
        initial_tensor_input_.clear();
        initial_input_batch_size_ = 0;
        return outputs;
    }

    inline std::vector<MuZeroNetworkOutput> RecurrentInference()
    {
        auto outputs = forward("recurrent_inference",
                               {{torch::cat(recurrent_tensor_feature_input_)}, {torch::cat(recurrent_tensor_action_input_)}},
                               recurrent_input_batch_size_);
        recurrent_tensor_feature_input_.clear();
        recurrent_tensor_action_input_.clear();
        recurrent_input_batch_size_ = 0;
        return outputs;
    }

    inline int GetNumActionChannels() const { return num_action_channels_; }
    inline int GetActionSize() const { return action_size_; }
    inline int GetInitialInputBatchSize() const { return initial_input_batch_size_; }
    inline int GetRecurrentInputBatchSize() const { return recurrent_input_batch_size_; }

private:
    std::vector<MuZeroNetworkOutput> forward(const std::string& method, const std::vector<torch::jit::IValue>& inputs, int batch_size)
    {
        assert(network_.find_method(method));

        auto forward_result = network_.get_method(method)(inputs).toGenericDict();
        auto policy_output = torch::softmax(forward_result.at("policy").toTensor(), 1).to(at::kCPU);
        auto value_output = forward_result.at("value").toTensor().to(at::kCPU);
        auto hidden_state_output = forward_result.at("hidden_state").toTensor().to(at::kCPU);

        const int policy_size = GetActionSize();
        const int hidden_state_size = GetNumHiddenChannels() * GetHiddenChannelHeight() * GetHiddenChannelWidth();
        std::vector<MuZeroNetworkOutput> network_outputs(batch_size);
        for (int i = 0; i < batch_size; ++i) {
            network_outputs[i].value_ = value_output[i].item<float>();
            network_outputs[i].policy_.assign(policy_output.data_ptr<float>() + i * policy_size,
                                              policy_output.data_ptr<float>() + (i + 1) * policy_size);
            network_outputs[i].hidden_state_.assign(hidden_state_output.data_ptr<float>() + i * hidden_state_size,
                                                    hidden_state_output.data_ptr<float>() + (i + 1) * hidden_state_size);
        }

        return network_outputs;
    }

    int num_action_channels_;
    int action_size_;
    int initial_input_batch_size_;
    int recurrent_input_batch_size_;
    std::mutex initial_mutex_;
    std::mutex recurrent_mutex_;
    std::vector<torch::Tensor> initial_tensor_input_;
    std::vector<torch::Tensor> recurrent_tensor_feature_input_;
    std::vector<torch::Tensor> recurrent_tensor_action_input_;
};

} // namespace minizero::network
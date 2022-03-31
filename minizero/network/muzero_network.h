#pragma once

#include "network.h"
#include <mutex>

namespace minizero::network {

class MuZeroNetworkOutput : public NetworkOutput {
public:
    float value_;
    std::vector<float> policy_;
    std::vector<float> hidden_state_;

    MuZeroNetworkOutput(int policy_size, int hidden_state_size)
    {
        value_ = 0.0f;
        policy_.resize(policy_size, 0.0f);
        hidden_state_.resize(hidden_state_size, 0.0f);
    }
};

class MuZeroNetwork : public Network {
public:
    MuZeroNetwork()
    {
        num_action_feature_channels_ = action_size_ = -1;
        initial_input_batch_size_ = recurrent_input_batch_size_ = 0;
        initial_tensor_input_.clear();
        initial_tensor_input_.reserve(kReserved_batch_size);
        recurrent_tensor_feature_input_.clear();
        recurrent_tensor_feature_input_.reserve(kReserved_batch_size);
        recurrent_tensor_action_input_.clear();
        recurrent_tensor_action_input_.reserve(kReserved_batch_size);
    }

    void loadModel(const std::string& nn_file_name, const int gpu_id)
    {
        Network::loadModel(nn_file_name, gpu_id);

        std::vector<torch::jit::IValue> dummy;
        num_action_feature_channels_ = network_.get_method("get_num_action_feature_channels")(dummy).toInt();
        action_size_ = network_.get_method("get_action_size")(dummy).toInt();
        initial_input_batch_size_ = 0;
        recurrent_input_batch_size_ = 0;
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << Network::toString();
        oss << "Number of action feature channels: " << num_action_feature_channels_ << std::endl;
        oss << "Action size: " << action_size_ << std::endl;
        return oss.str();
    }

    int pushBackInitialData(std::vector<float> features)
    {
        assert(static_cast<int>(features.size()) == getNumInputChannels() * getInputChannelHeight() * getInputChannelWidth());

        int index;
        {
            std::lock_guard<std::mutex> lock(initial_mutex_);
            index = initial_input_batch_size_++;
            initial_tensor_input_.resize(initial_input_batch_size_);
        }
        initial_tensor_input_[index] = torch::from_blob(features.data(), {1, getNumInputChannels(), getInputChannelHeight(), getInputChannelWidth()}).clone();
        return index;
    }

    int pushBackRecurrentData(std::vector<float> features, std::vector<float> actions)
    {
        assert(static_cast<int>(features.size()) == getNumHiddenChannels() * getHiddenChannelHeight() * getHiddenChannelWidth());
        assert(static_cast<int>(actions.size()) == getNumActionFeatureChannels() * getHiddenChannelHeight() * getHiddenChannelWidth());

        int index;
        {
            std::lock_guard<std::mutex> lock(recurrent_mutex_);
            index = recurrent_input_batch_size_++;
            recurrent_tensor_feature_input_.resize(recurrent_input_batch_size_);
            recurrent_tensor_action_input_.resize(recurrent_input_batch_size_);
        }
        recurrent_tensor_feature_input_[index] = torch::from_blob(features.data(), {1, getNumHiddenChannels(), getHiddenChannelHeight(), getHiddenChannelWidth()}).clone();
        recurrent_tensor_action_input_[index] = torch::from_blob(actions.data(), {1, getNumActionFeatureChannels(), getHiddenChannelHeight(), getHiddenChannelWidth()}).clone();
        return index;
    }

    inline std::vector<std::shared_ptr<NetworkOutput>> initialInference()
    {
        assert(initial_input_batch_size_ > 0);
        auto outputs = forward("initial_inference", {torch::cat(initial_tensor_input_).to(getDevice())}, initial_input_batch_size_);
        initial_tensor_input_.clear();
        initial_tensor_input_.reserve(kReserved_batch_size);
        initial_input_batch_size_ = 0;
        return outputs;
    }

    inline std::vector<std::shared_ptr<NetworkOutput>> recurrentInference()
    {
        assert(recurrent_input_batch_size_ > 0);
        auto outputs = forward("recurrent_inference",
                               {{torch::cat(recurrent_tensor_feature_input_).to(getDevice())}, {torch::cat(recurrent_tensor_action_input_).to(getDevice())}},
                               recurrent_input_batch_size_);
        recurrent_tensor_feature_input_.clear();
        recurrent_tensor_feature_input_.reserve(kReserved_batch_size);
        recurrent_tensor_action_input_.clear();
        recurrent_tensor_action_input_.reserve(kReserved_batch_size);
        recurrent_input_batch_size_ = 0;
        return outputs;
    }

    inline int getNumActionFeatureChannels() const { return num_action_feature_channels_; }
    inline int getActionSize() const { return action_size_; }
    inline int getInitialInputBatchSize() const { return initial_input_batch_size_; }
    inline int getRecurrentInputBatchSize() const { return recurrent_input_batch_size_; }

private:
    std::vector<std::shared_ptr<NetworkOutput>> forward(const std::string& method, const std::vector<torch::jit::IValue>& inputs, int batch_size)
    {
        assert(network_.find_method(method));

        auto forward_result = network_.get_method(method)(inputs).toGenericDict();
        auto policy_output = torch::softmax(forward_result.at("policy").toTensor(), 1).to(at::kCPU);
        auto value_output = forward_result.at("value").toTensor().to(at::kCPU);
        auto hidden_state_output = forward_result.at("hidden_state").toTensor().to(at::kCPU);
        assert(policy_output.numel() == batch_size * getActionSize());
        assert(value_output.numel() == batch_size);
        assert(hidden_state_output.numel() == batch_size * getNumHiddenChannels() * getHiddenChannelHeight() * getHiddenChannelWidth());

        const int policy_size = getActionSize();
        const int hidden_state_size = getNumHiddenChannels() * getHiddenChannelHeight() * getHiddenChannelWidth();
        std::vector<std::shared_ptr<NetworkOutput>> network_outputs;
        for (int i = 0; i < batch_size; ++i) {
            network_outputs.emplace_back(std::make_shared<MuZeroNetworkOutput>(policy_size, hidden_state_size));
            auto muzero_network_output = std::static_pointer_cast<MuZeroNetworkOutput>(network_outputs.back());
            muzero_network_output->value_ = value_output[i].item<float>();
            std::copy(policy_output.data_ptr<float>() + i * policy_size,
                      policy_output.data_ptr<float>() + (i + 1) * policy_size,
                      muzero_network_output->policy_.begin());
            std::copy(hidden_state_output.data_ptr<float>() + i * hidden_state_size,
                      hidden_state_output.data_ptr<float>() + (i + 1) * hidden_state_size,
                      muzero_network_output->hidden_state_.begin());
        }

        return network_outputs;
    }

    int num_action_feature_channels_;
    int action_size_;
    int initial_input_batch_size_;
    int recurrent_input_batch_size_;
    std::mutex initial_mutex_;
    std::mutex recurrent_mutex_;
    std::vector<torch::Tensor> initial_tensor_input_;
    std::vector<torch::Tensor> recurrent_tensor_feature_input_;
    std::vector<torch::Tensor> recurrent_tensor_action_input_;

    const int kReserved_batch_size = 4096;
};

} // namespace minizero::network
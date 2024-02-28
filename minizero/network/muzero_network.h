#pragma once

#include "network.h"
#include "utils.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace minizero::network {

class MuZeroNetworkOutput : public NetworkOutput {
public:
    float value_;
    float reward_;
    std::vector<float> policy_;
    std::vector<float> policy_logits_;
    std::vector<float> hidden_state_;

    MuZeroNetworkOutput(int policy_size, int hidden_state_size)
    {
        value_ = 0.0f;
        reward_ = 0.0f;
        policy_.resize(policy_size, 0.0f);
        policy_logits_.resize(policy_size, 0.0f);
        hidden_state_.resize(hidden_state_size, 0.0f);
    }
};

class MuZeroNetwork : public Network {
public:
    MuZeroNetwork()
    {
        num_action_feature_channels_ = -1;
        initial_input_batch_size_ = recurrent_input_batch_size_ = 0;
        initial_tensor_input_.clear();
        initial_tensor_input_.reserve(kReserved_batch_size);
        recurrent_tensor_feature_input_.clear();
        recurrent_tensor_feature_input_.reserve(kReserved_batch_size);
        recurrent_tensor_action_input_.clear();
        recurrent_tensor_action_input_.reserve(kReserved_batch_size);
    }

    void loadModel(const std::string& nn_file_name, const int gpu_id) override
    {
        Network::loadModel(nn_file_name, gpu_id);

        std::vector<torch::jit::IValue> dummy;
        num_action_feature_channels_ = network_.get_method("get_num_action_feature_channels")(dummy).toInt();
        initial_input_batch_size_ = 0;
        recurrent_input_batch_size_ = 0;
    }

    std::string toString() const override
    {
        std::ostringstream oss;
        oss << Network::toString();
        oss << "Number of action feature channels: " << num_action_feature_channels_ << std::endl;
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
    inline int getInitialInputBatchSize() const { return initial_input_batch_size_; }
    inline int getRecurrentInputBatchSize() const { return recurrent_input_batch_size_; }

protected:
    std::vector<std::shared_ptr<NetworkOutput>> forward(const std::string& method, const std::vector<torch::jit::IValue>& inputs, int batch_size)
    {
        assert(network_.find_method(method));

        auto forward_result = network_.get_method(method)(inputs).toGenericDict();
        auto policy_output = forward_result.at("policy").toTensor().to(at::kCPU);
        auto policy_logits_output = forward_result.at("policy_logit").toTensor().to(at::kCPU);
        auto value_output = forward_result.at("value").toTensor().to(at::kCPU);
        auto reward_output = (forward_result.contains("reward") ? forward_result.at("reward").toTensor().to(at::kCPU) : torch::zeros(0));
        auto hidden_state_output = forward_result.at("hidden_state").toTensor().to(at::kCPU);
        assert(policy_output.numel() == batch_size * getActionSize());
        assert(policy_logits_output.numel() == batch_size * getActionSize());
        assert((getNetworkTypeName() != "muzero_atari" && value_output.numel() == batch_size) || (getNetworkTypeName() == "muzero_atari" && value_output.numel() == batch_size * getDiscreteValueSize()));
        assert(!forward_result.contains("reward") || (forward_result.contains("reward") && reward_output.numel() == batch_size * getDiscreteValueSize()));
        assert(hidden_state_output.numel() == batch_size * getNumHiddenChannels() * getHiddenChannelHeight() * getHiddenChannelWidth());

        const int policy_size = getActionSize();
        const int hidden_state_size = getNumHiddenChannels() * getHiddenChannelHeight() * getHiddenChannelWidth();
        std::vector<std::shared_ptr<NetworkOutput>> network_outputs;
        for (int i = 0; i < batch_size; ++i) {
            network_outputs.emplace_back(std::make_shared<MuZeroNetworkOutput>(policy_size, hidden_state_size));
            auto muzero_network_output = std::static_pointer_cast<MuZeroNetworkOutput>(network_outputs.back());

            std::copy(policy_output.data_ptr<float>() + i * policy_size,
                      policy_output.data_ptr<float>() + (i + 1) * policy_size,
                      muzero_network_output->policy_.begin());
            std::copy(policy_logits_output.data_ptr<float>() + i * policy_size,
                      policy_logits_output.data_ptr<float>() + (i + 1) * policy_size,
                      muzero_network_output->policy_logits_.begin());
            std::copy(hidden_state_output.data_ptr<float>() + i * hidden_state_size,
                      hidden_state_output.data_ptr<float>() + (i + 1) * hidden_state_size,
                      muzero_network_output->hidden_state_.begin());

            if (getNetworkTypeName() == "muzero_atari") {
                int start_value = -getDiscreteValueSize() / 2;
                muzero_network_output->value_ = std::accumulate(value_output.data_ptr<float>() + i * getDiscreteValueSize(),
                                                                value_output.data_ptr<float>() + (i + 1) * getDiscreteValueSize(),
                                                                0.0f,
                                                                [&start_value](const float& sum, const float& value) { return sum + value * start_value++; });
                muzero_network_output->value_ = utils::invertValue(muzero_network_output->value_);
                if (forward_result.contains("reward")) {
                    start_value = -getDiscreteValueSize() / 2;
                    muzero_network_output->reward_ = std::accumulate(reward_output.data_ptr<float>() + i * getDiscreteValueSize(),
                                                                     reward_output.data_ptr<float>() + (i + 1) * getDiscreteValueSize(),
                                                                     0.0f,
                                                                     [&start_value](const float& sum, const float& value) { return sum + value * start_value++; });
                    muzero_network_output->reward_ = utils::invertValue(muzero_network_output->reward_);
                }
            } else {
                muzero_network_output->value_ = value_output[i].item<float>();
            }
        }

        return network_outputs;
    }

    int num_action_feature_channels_;
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

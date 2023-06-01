#pragma once

#include "network.h"
#include "utils.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace minizero::network {

class AlphaZeroNetworkOutput : public NetworkOutput {
public:
    float value_;
    std::vector<float> policy_;
    std::vector<float> policy_logits_;

    AlphaZeroNetworkOutput(int policy_size)
    {
        value_ = 0.0f;
        policy_.resize(policy_size, 0.0f);
        policy_logits_.resize(policy_size, 0.0f);
    }
};

class AlphaZeroNetwork : public Network {
public:
    AlphaZeroNetwork()
    {
        clear();
    }

    void loadModel(const std::string& nn_file_name, const int gpu_id) override
    {
        assert(batch_size_ == 0); // should avoid loading model when batch size is not 0
        Network::loadModel(nn_file_name, gpu_id);
        clear();
    }

    std::string toString() const override
    {
        std::ostringstream oss;
        oss << Network::toString();
        return oss.str();
    }

    int pushBack(std::vector<float> features)
    {
        assert(static_cast<int>(features.size()) == getNumInputChannels() * getInputChannelHeight() * getInputChannelWidth());
        assert(batch_size_ < kReserved_batch_size);

        int index;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            index = batch_size_++;
            tensor_input_.resize(batch_size_);
        }
        tensor_input_[index] = torch::from_blob(features.data(), {1, getNumInputChannels(), getInputChannelHeight(), getInputChannelWidth()}).clone();
        return index;
    }

    std::vector<std::shared_ptr<NetworkOutput>> forward()
    {
        assert(batch_size_ > 0);
        auto forward_result = network_.forward(std::vector<torch::jit::IValue>{torch::cat(tensor_input_).to(getDevice())}).toGenericDict();

        auto policy_output = forward_result.at("policy").toTensor().to(at::kCPU);
        auto policy_logits_output = forward_result.at("policy_logit").toTensor().to(at::kCPU);
        auto value_output = forward_result.at("value").toTensor().to(at::kCPU);
        assert(policy_output.numel() == batch_size_ * getActionSize());
        assert(policy_logits_output.numel() == batch_size_ * getActionSize());
        assert(value_output.numel() == batch_size_ * getDiscreteValueSize());

        const int policy_size = getActionSize();
        std::vector<std::shared_ptr<NetworkOutput>> network_outputs;
        for (int i = 0; i < batch_size_; ++i) {
            network_outputs.emplace_back(std::make_shared<AlphaZeroNetworkOutput>(policy_size));
            auto alphazero_network_output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_outputs.back());

            // policy & policy logits
            std::copy(policy_output.data_ptr<float>() + i * policy_size,
                      policy_output.data_ptr<float>() + (i + 1) * policy_size,
                      alphazero_network_output->policy_.begin());
            std::copy(policy_logits_output.data_ptr<float>() + i * policy_size,
                      policy_logits_output.data_ptr<float>() + (i + 1) * policy_size,
                      alphazero_network_output->policy_logits_.begin());

            // value
            if (getDiscreteValueSize() == 1) {
                alphazero_network_output->value_ = value_output[i].item<float>();
            } else {
                int start_value = -getDiscreteValueSize() / 2;
                alphazero_network_output->value_ = std::accumulate(value_output.data_ptr<float>() + i * getDiscreteValueSize(),
                                                                   value_output.data_ptr<float>() + (i + 1) * getDiscreteValueSize(),
                                                                   0.0f,
                                                                   [&start_value](const float& sum, const float& value) { return sum + value * start_value++; });
                alphazero_network_output->value_ = utils::invertValue(alphazero_network_output->value_);
            }
        }

        clear();
        return network_outputs;
    }

    inline int getBatchSize() const { return batch_size_; }

private:
    inline void clear()
    {
        batch_size_ = 0;
        tensor_input_.clear();
        tensor_input_.reserve(kReserved_batch_size);
    }

    int batch_size_;
    std::mutex mutex_;
    std::vector<torch::Tensor> tensor_input_;

    const int kReserved_batch_size = 4096;
};

} // namespace minizero::network

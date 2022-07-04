#pragma once

#include "network.h"
#include <mutex>

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
        batch_size_ = 0;
        clearTensorInput();
    }

    void loadModel(const std::string& nn_file_name, const int gpu_id)
    {
        Network::loadModel(nn_file_name, gpu_id);
        batch_size_ = 0;
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << Network::toString();
        return oss.str();
    }

    int pushBack(std::vector<float> features)
    {
        assert(static_cast<int>(features.size()) == getNumInputChannels() * getInputChannelHeight() * getInputChannelWidth());

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
        assert(value_output.numel() == batch_size_);

        const int policy_size = getActionSize();
        std::vector<std::shared_ptr<NetworkOutput>> network_outputs;
        for (int i = 0; i < batch_size_; ++i) {
            network_outputs.emplace_back(std::make_shared<AlphaZeroNetworkOutput>(policy_size));
            auto alphazero_network_output = std::static_pointer_cast<AlphaZeroNetworkOutput>(network_outputs.back());
            alphazero_network_output->value_ = value_output[i].item<float>();
            std::copy(policy_output.data_ptr<float>() + i * policy_size,
                      policy_output.data_ptr<float>() + (i + 1) * policy_size,
                      alphazero_network_output->policy_.begin());
            std::copy(policy_logits_output.data_ptr<float>() + i * policy_size,
                      policy_logits_output.data_ptr<float>() + (i + 1) * policy_size,
                      alphazero_network_output->policy_logits_.begin());
        }

        batch_size_ = 0;
        clearTensorInput();
        return network_outputs;
    }

    inline int getBatchSize() const { return batch_size_; }

private:
    inline void clearTensorInput()
    {
        tensor_input_.clear();
        tensor_input_.reserve(kReserved_batch_size);
    }

    int batch_size_;
    std::mutex mutex_;
    std::vector<torch::Tensor> tensor_input_;

    const int kReserved_batch_size = 4096;
};

} // namespace minizero::network
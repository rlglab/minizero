#pragma once

#include "network.h"
#include <mutex>

namespace solver {

class ProofCostNetworkOutput : public minizero::network::NetworkOutput {
public:
    float value_n_;
    float value_m_;
    std::vector<float> policy_;
    std::vector<float> policy_logits_;

    ProofCostNetworkOutput(int policy_size)
    {
        value_n_ = value_m_ = 0.0f;
        policy_.resize(policy_size, 0.0f);
        policy_logits_.resize(policy_size, 0.0f);
    }
};

class ProofCostNetwork : public minizero::network::Network {
public:
    ProofCostNetwork()
    {
        batch_size_ = 0;
        clearTensorInput();
    }

    void loadModel(const std::string& nn_file_name, const int gpu_id)
    {
        Network::loadModel(nn_file_name, gpu_id);
        std::vector<torch::jit::IValue> dummy;
        value_size_ = network_.get_method("get_value_size")(dummy).toInt();
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

    std::vector<std::shared_ptr<minizero::network::NetworkOutput>> forward()
    {
        assert(batch_size_ > 0);
        auto forward_result = network_.forward(std::vector<torch::jit::IValue>{torch::cat(tensor_input_).to(getDevice())}).toGenericDict();

        auto policy_output = torch::softmax(forward_result.at("policy").toTensor(), 1).to(at::kCPU);
        auto policy_logits_output = forward_result.at("policy").toTensor().to(at::kCPU);
        auto value_n_output = torch::softmax(forward_result.at("value_n").toTensor().to(at::kCPU), 1).to(at::kCPU);
        auto value_m_output = torch::softmax(forward_result.at("value_m").toTensor().to(at::kCPU), 1).to(at::kCPU);
        assert(policy_output.numel() == batch_size_ * getActionSize());
        assert(policy_logits_output.numel() == batch_size_ * getActionSize());
        assert(value_n_output.numel() == batch_size_ * getValueSize());
        assert(value_m_output.numel() == batch_size_ * getValueSize());

        const int policy_size = getActionSize();
        std::vector<std::shared_ptr<minizero::network::NetworkOutput>> network_outputs;
        for (int i = 0; i < batch_size_; ++i) {
            network_outputs.emplace_back(std::make_shared<ProofCostNetworkOutput>(policy_size));
            auto proof_cost_network_output = std::static_pointer_cast<ProofCostNetworkOutput>(network_outputs.back());
            std::copy(policy_output.data_ptr<float>() + i * policy_size,
                      policy_output.data_ptr<float>() + (i + 1) * policy_size,
                      proof_cost_network_output->policy_.begin());
            std::copy(policy_logits_output.data_ptr<float>() + i * policy_size,
                      policy_logits_output.data_ptr<float>() + (i + 1) * policy_size,
                      proof_cost_network_output->policy_logits_.begin());
            proof_cost_network_output->value_n_ = proof_cost_network_output->value_m_ = 0.0f;

            // change value distribution to scalar
            std::vector<float> value_n(getValueSize()), value_m(getValueSize());
            std::copy(value_n_output.data_ptr<float>() + i * value_size_,
                      value_n_output.data_ptr<float>() + (i + 1) * value_size_,
                      value_n.begin());
            std::copy(value_m_output.data_ptr<float>() + i * value_size_,
                      value_m_output.data_ptr<float>() + (i + 1) * value_size_,
                      value_m.begin());
            for (int j = 0; j < getValueSize(); ++j) {
                proof_cost_network_output->value_n_ += value_n[j] * j;
                proof_cost_network_output->value_m_ += value_m[j] * j;
            }
        }

        batch_size_ = 0;
        clearTensorInput();
        return network_outputs;
    }

    inline int getValueSize() const { return value_size_; }
    inline int getBatchSize() const { return batch_size_; }

private:
    inline void clearTensorInput()
    {
        tensor_input_.clear();
        tensor_input_.reserve(kReserved_batch_size);
    }

    int value_size_;
    int batch_size_;
    std::mutex mutex_;
    std::vector<torch::Tensor> tensor_input_;

    const int kReserved_batch_size = 4096;
};

} // namespace solver
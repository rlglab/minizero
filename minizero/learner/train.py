#!/usr/bin/env python

import sys
import time
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
from minizero.network.py.create_network import create_network
from tools.analysis import analysis


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


class MinizeroDadaLoader:
    def __init__(self, conf_file_name):
        self.conf_file_name = conf_file_name
        self.data_loader = py.DataLoader(self.conf_file_name)
        self.data_loader.initialize()
        self.data_list = []

        # allocate memory
        self.sampled_index = np.zeros(py.get_batch_size() * 2, dtype=np.int32)
        self.features = np.zeros(py.get_batch_size() * py.get_nn_num_input_channels() * py.get_nn_input_channel_height() * py.get_nn_input_channel_width(), dtype=np.float32)
        self.loss_scale = np.zeros(py.get_batch_size(), dtype=np.float32)
        self.value_accumulator = np.ones(1) if py.get_nn_discrete_value_size() == 1 else np.arange(-int(py.get_nn_discrete_value_size() / 2), int(py.get_nn_discrete_value_size() / 2) + 1)
        if py.get_nn_type_name() == "alphazero":
            self.action_features = None
            self.policy = np.zeros(py.get_batch_size() * py.get_nn_action_size(), dtype=np.float32)
            self.value = np.zeros(py.get_batch_size() * py.get_nn_discrete_value_size(), dtype=np.float32)
            self.reward = None
        else:
            self.action_features = np.zeros(py.get_batch_size() * py.get_muzero_unrolling_step() * py.get_nn_num_action_feature_channels()
                                            * py.get_nn_hidden_channel_height() * py.get_nn_hidden_channel_width(), dtype=np.float32)
            self.policy = np.zeros(py.get_batch_size() * (py.get_muzero_unrolling_step() + 1) * py.get_nn_action_size(), dtype=np.float32)
            self.value = np.zeros(py.get_batch_size() * (py.get_muzero_unrolling_step() + 1) * py.get_nn_discrete_value_size(), dtype=np.float32)
            self.reward = np.zeros(py.get_batch_size() * py.get_muzero_unrolling_step() * py.get_nn_discrete_value_size(), dtype=np.float32)

    def load_data(self, training_dir, start_iter, end_iter):
        for i in range(start_iter, end_iter + 1):
            file_name = f"{training_dir}/sgf/{i}.sgf"
            if file_name in self.data_list:
                continue
            self.data_loader.load_data_from_file(file_name)
            self.data_list.append(file_name)
            if len(self.data_list) > py.get_zero_replay_buffer():
                self.data_list.pop(0)

    def sample_data(self, device='cpu'):
        self.data_loader.sample_data(self.features, self.action_features, self.policy, self.value, self.reward, self.loss_scale, self.sampled_index)
        features = torch.FloatTensor(self.features).view(py.get_batch_size(), py.get_nn_num_input_channels(), py.get_nn_input_channel_height(), py.get_nn_input_channel_width()).to(device)
        action_features = None if self.action_features is None else torch.FloatTensor(self.action_features).view(py.get_batch_size(),
                                                                                                                 -1,
                                                                                                                 py.get_nn_num_action_feature_channels(),
                                                                                                                 py.get_nn_hidden_channel_height(),
                                                                                                                 py.get_nn_hidden_channel_width()).to(device)
        policy = torch.FloatTensor(self.policy).view(py.get_batch_size(), -1, py.get_nn_action_size()).to(device)
        value = torch.FloatTensor(self.value).view(py.get_batch_size(), -1, py.get_nn_discrete_value_size()).to(device)
        reward = None if self.reward is None else torch.FloatTensor(self.reward).view(py.get_batch_size(), -1, py.get_nn_discrete_value_size()).to(device)
        loss_scale = torch.FloatTensor(self.loss_scale / np.amax(self.loss_scale)).to(device)
        sampled_index = self.sampled_index

        return features, action_features, policy, value, reward, loss_scale, sampled_index

    def update_priority(self, sampled_index, batch_values):
        batch_values = (batch_values * self.value_accumulator).sum(axis=1)
        self.data_loader.update_priority(sampled_index, batch_values)


class Model:
    def __init__(self):
        self.training_step = 0
        self.network = None
        self.device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
        self.optimizer = None
        self.scheduler = None

    def load_model(self, training_dir, model_file):
        self.training_step = 0
        self.network = create_network(py.get_game_name(),
                                      py.get_nn_num_input_channels(),
                                      py.get_nn_input_channel_height(),
                                      py.get_nn_input_channel_width(),
                                      py.get_nn_num_hidden_channels(),
                                      py.get_nn_hidden_channel_height(),
                                      py.get_nn_hidden_channel_width(),
                                      py.get_nn_num_action_feature_channels(),
                                      py.get_nn_num_blocks(),
                                      py.get_nn_action_size(),
                                      py.get_nn_num_value_hidden_channels(),
                                      py.get_nn_discrete_value_size(),
                                      py.get_nn_type_name())
        self.network.to(self.device)
        self.optimizer = optim.SGD(self.network.parameters(),
                                   lr=py.get_learning_rate(),
                                   momentum=py.get_momentum(),
                                   weight_decay=py.get_weight_decay())
        self.scheduler = optim.lr_scheduler.StepLR(self.optimizer, step_size=1000000, gamma=0.1)

        if model_file:
            snapshot = torch.load(f"{training_dir}/model/{model_file}", map_location=torch.device('cpu'))
            self.training_step = snapshot['training_step']
            self.network.load_state_dict(snapshot['network'])
            self.optimizer.load_state_dict(snapshot['optimizer'])
            self.optimizer.param_groups[0]["lr"] = py.get_learning_rate()
            self.scheduler.load_state_dict(snapshot['scheduler'])

        # for multi-gpu
        self.network = nn.DataParallel(self.network)

    def save_model(self, training_dir):
        snapshot = {'training_step': self.training_step,
                    'network': self.network.module.state_dict(),
                    'optimizer': self.optimizer.state_dict(),
                    'scheduler': self.scheduler.state_dict()}
        torch.save(snapshot, f"{training_dir}/model/weight_iter_{self.training_step}.pkl")
        torch.jit.script(self.network.module).save(f"{training_dir}/model/weight_iter_{self.training_step}.pt")


def calculate_loss(network_output, label_policy, label_value, label_reward, loss_scale):
    # policy
    if py.use_gumbel():
        loss_policy = (nn.functional.kl_div(nn.functional.log_softmax(network_output["policy_logit"], dim=1), label_policy, reduction='none').sum(dim=1) * loss_scale).mean()
    else:
        loss_policy = -((label_policy * nn.functional.log_softmax(network_output["policy_logit"], dim=1)).sum(dim=1) * loss_scale).mean()

    # value
    if py.get_nn_discrete_value_size() == 1:
        loss_value = (nn.functional.mse_loss(network_output["value"], label_value, reduction='none') * loss_scale).mean()
    else:
        loss_value = -((label_value * nn.functional.log_softmax(network_output["value_logit"], dim=1)).sum(dim=1) * loss_scale).mean()

    # reward
    loss_reward = 0
    if label_reward is not None and "reward_logit" in network_output:
        loss_reward = -((label_reward * nn.functional.log_softmax(network_output["reward_logit"], dim=1)).sum(dim=1) * loss_scale).mean()

    return loss_policy, loss_value, loss_reward


def add_training_info(training_info, key, value):
    if key not in training_info:
        training_info[key] = 0
    training_info[key] += value


def calculate_accuracy(output, label, batch_size):
    max_output = np.argmax(output.to('cpu').detach().numpy(), axis=1)
    max_label = np.argmax(label.to('cpu').detach().numpy(), axis=1)
    return (max_output == max_label).sum() / batch_size


def train(model, training_dir, data_loader, start_iter, end_iter):
    if start_iter == -1:
        model.save_model(training_dir)
        return

    # load data
    data_loader.load_data(training_dir, start_iter, end_iter)

    training_info = {}
    for i in range(1, py.get_training_step() + 1):
        model.optimizer.zero_grad()
        features, action_features, label_policy, label_value, label_reward, loss_scale, sampled_index = data_loader.sample_data(model.device)

        if py.get_nn_type_name() == "alphazero":
            network_output = model.network(features)
            loss_policy, loss_value, _ = calculate_loss(network_output, label_policy[:, 0], label_value[:, 0], None, loss_scale)
            loss = loss_policy + py.get_value_loss_scale() * loss_value

            # record training info
            add_training_info(training_info, 'loss_policy', loss_policy.item())
            add_training_info(training_info, 'accuracy_policy', calculate_accuracy(network_output["policy_logit"], label_policy[:, 0], py.get_batch_size()))
            add_training_info(training_info, 'loss_value', loss_value.item())
        elif py.get_nn_type_name() == "muzero":
            network_output = model.network(features)
            batch_values = network_output['value'].to('cpu').detach().numpy()
            loss_step_policy, loss_step_value, loss_step_reward = calculate_loss(network_output, label_policy[:, 0], label_value[:, 0], None, loss_scale)
            add_training_info(training_info, 'loss_policy_0', loss_step_policy.item())
            add_training_info(training_info, 'accuracy_policy_0', calculate_accuracy(network_output["policy_logit"], label_policy[:, 0], py.get_batch_size()))
            add_training_info(training_info, 'loss_value_0', loss_step_value.item())
            loss_policy = loss_step_policy
            loss_value = loss_step_value
            loss_reward = loss_step_reward
            for i in range(py.get_muzero_unrolling_step()):
                network_output = model.network(network_output["hidden_state"], action_features[:, i])
                batch_values = np.concatenate((batch_values, network_output['value'].to('cpu').detach().numpy()), axis=0)
                loss_step_policy, loss_step_value, loss_step_reward = calculate_loss(network_output, label_policy[:, i + 1], label_value[:, i + 1], label_reward[:, i], loss_scale)
                add_training_info(training_info, f'loss_policy_{i+1}', loss_step_policy.item() / py.get_muzero_unrolling_step())
                add_training_info(training_info, f'accuracy_policy_{i+1}', calculate_accuracy(network_output["policy_logit"], label_policy[:, i + 1], py.get_batch_size()))
                add_training_info(training_info, f'loss_value_{i+1}', loss_step_value.item() / py.get_muzero_unrolling_step())
                if "reward_logit" in network_output:
                    add_training_info(training_info, f'loss_reward_{i+1}', loss_step_reward.item() / py.get_muzero_unrolling_step())
                loss_policy += loss_step_policy / py.get_muzero_unrolling_step()
                loss_value += loss_step_value / py.get_muzero_unrolling_step()
                loss_reward += loss_step_reward / py.get_muzero_unrolling_step()
                network_output["hidden_state"].register_hook(lambda grad: grad / 2)
            if py.use_per():
                data_loader.update_priority(sampled_index, batch_values)
            loss = loss_policy + py.get_value_loss_scale() * loss_value + loss_reward

            add_training_info(training_info, 'loss_policy', loss_policy.item())
            add_training_info(training_info, 'loss_value', loss_value.item())
            if "reward_logit" in network_output:
                add_training_info(training_info, 'loss_reward', loss_reward.item())

        loss.backward()
        model.optimizer.step()
        model.scheduler.step()

        model.training_step += 1
        if model.training_step != 0 and model.training_step % py.get_training_display_step() == 0:
            eprint("[{}] nn step {}, lr: {}.".format(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), model.training_step, round(model.optimizer.param_groups[0]["lr"], 6)))
            for loss in training_info:
                eprint("\t{}: {}".format(loss, round(training_info[loss] / py.get_training_display_step(), 5)))
            training_info = {}

    model.save_model(training_dir)
    print("Optimization_Done", model.training_step, flush=True)
    eprint("Optimization_Done", model.training_step)
    analysis(training_dir, "analysis")


if __name__ == '__main__':
    if len(sys.argv) == 4:
        game_type = sys.argv[1]
        training_dir = sys.argv[2]
        conf_file_name = sys.argv[3]

        # import pybind library
        _temps = __import__(f'build.{game_type}', globals(), locals(), ['minizero_py'], 0)
        py = _temps.minizero_py
    else:
        eprint("python train.py game_type training_dir conf_file")
        exit(0)

    py.load_config_file(conf_file_name)
    data_loader = MinizeroDadaLoader(conf_file_name)
    model = Model()

    while True:
        try:
            command = input()
            command_prefix = command.split()[0]
            if command == "keep_alive":
                continue

            eprint("[{}] [command] {}".format(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), command))
            if command_prefix == "update_config":
                conf_str = command.split(" ", 1)[-1]
                if not py.load_config_string(conf_str):
                    eprint("Failed to load configuration string.")
                    exit(0)
            elif command_prefix == "train":
                _, model_file, start_iter, end_iter = command.split()
                model_file = model_file.replace('"', '')

                # skip loading model if the model is loaded
                if model.network is None:
                    model.load_model(training_dir, model_file)

                train(model, training_dir, data_loader, int(start_iter), int(end_iter))
            elif command_prefix == "quit":
                exit(0)

        except (KeyboardInterrupt, EOFError) as e:
            break

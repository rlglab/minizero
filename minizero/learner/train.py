#!/usr/bin/env python

import sys
import time
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
from minizero.network.py.create_network import create_network


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


class MinizeroDadaLoader:
    def __init__(self, conf_file_name):
        self.conf_file_name = conf_file_name
        self.data_loader = minizero_py.DataLoader(self.conf_file_name)
        self.data_list = []

        # allocate memory
        if conf.get_nn_type_name() == "alphazero":
            self.features = np.zeros(conf.get_batch_size() * conf.get_nn_num_input_channels() * conf.get_nn_input_channel_height() * conf.get_nn_input_channel_width(), dtype=np.float32)
            self.action_features = None
            self.policy = np.zeros(conf.get_batch_size() * conf.get_nn_action_size(), dtype=np.float32)
            self.value = np.zeros(conf.get_batch_size() * conf.get_nn_discrete_value_size(), dtype=np.float32)
            self.reward = None
            self.loss_scale = np.zeros(conf.get_batch_size(), dtype=np.float32)
        else:
            self.features = np.zeros(conf.get_batch_size() * conf.get_nn_num_input_channels() * conf.get_nn_input_channel_height() * conf.get_nn_input_channel_width(), dtype=np.float32)
            self.action_features = np.zeros(conf.get_batch_size() * conf.get_muzero_unrolling_step() * conf.get_nn_num_action_feature_channels()
                                            * conf.get_nn_hidden_channel_height() * conf.get_nn_hidden_channel_width(), dtype=np.float32)
            self.policy = np.zeros(conf.get_batch_size() * (conf.get_muzero_unrolling_step() + 1) * conf.get_nn_action_size(), dtype=np.float32)
            self.value = np.zeros(conf.get_batch_size() * (conf.get_muzero_unrolling_step() + 1) * conf.get_nn_discrete_value_size(), dtype=np.float32)
            self.reward = np.zeros(conf.get_batch_size() * conf.get_muzero_unrolling_step() * conf.get_nn_discrete_value_size(), dtype=np.float32)
            self.loss_scale = np.zeros(conf.get_batch_size(), dtype=np.float32)

    def load_data(self, training_dir, start_iter, end_iter):
        for i in range(start_iter, end_iter + 1):
            file_name = f"{training_dir}/sgf/{i}.sgf"
            if file_name in self.data_list:
                continue
            self.data_loader.load_data_from_file(file_name)
            self.data_list.append(file_name)
            if len(self.data_list) > conf.get_zero_replay_buffer():
                self.data_list.pop(0)

    def sample_data(self, conf):
        self.data_loader.sample_data(self.features, self.action_features, self.policy, self.value, self.reward, self.loss_scale)
        features = torch.FloatTensor(self.features).view(conf.get_batch_size(),
                                                         conf.get_nn_num_input_channels(),
                                                         conf.get_nn_input_channel_height(),
                                                         conf.get_nn_input_channel_width())
        action_features = None if self.action_features is None else torch.FloatTensor(self.action_features).view(conf.get_batch_size(),
                                                                                                                 -1,
                                                                                                                 conf.get_nn_num_action_feature_channels(),
                                                                                                                 conf.get_nn_hidden_channel_height(),
                                                                                                                 conf.get_nn_hidden_channel_width())
        policy = torch.FloatTensor(self.policy).view(conf.get_batch_size(), -1, conf.get_nn_action_size())
        value = torch.FloatTensor(self.value).view(conf.get_batch_size(), -1, conf.get_nn_discrete_value_size())
        reward = None if self.reward is None else torch.FloatTensor(self.reward).view(conf.get_batch_size(), -1, conf.get_nn_discrete_value_size())
        loss_scale = torch.FloatTensor(self.loss_scale / np.amax(self.loss_scale))

        return features, action_features, policy, value, reward, loss_scale


def load_model(game_type, training_dir, model_file, conf):
    # training_step, network, device, optimizer, scheduler
    training_step = 0
    network = create_network(conf.get_game_name(),
                             conf.get_nn_num_input_channels(),
                             conf.get_nn_input_channel_height(),
                             conf.get_nn_input_channel_width(),
                             conf.get_nn_num_hidden_channels(),
                             conf.get_nn_hidden_channel_height(),
                             conf.get_nn_hidden_channel_width(),
                             conf.get_nn_num_action_feature_channels(),
                             conf.get_nn_num_blocks(),
                             conf.get_nn_num_action_channels(),
                             conf.get_nn_action_size(),
                             conf.get_nn_num_value_hidden_channels(),
                             conf.get_nn_discrete_value_size(),
                             conf.get_nn_type_name())
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    network.to(device)
    optimizer = optim.SGD(network.parameters(),
                          lr=conf.get_learning_rate(),
                          momentum=conf.get_momentum(),
                          weight_decay=conf.get_weight_decay())
    scheduler = optim.lr_scheduler.StepLR(optimizer, step_size=1000000, gamma=0.1)

    if model_file:
        snapshot = torch.load(f"{training_dir}/model/{model_file}", map_location=torch.device('cpu'))
        training_step = snapshot['training_step']
        network.load_state_dict(snapshot['network'])
        optimizer.load_state_dict(snapshot['optimizer'])
        optimizer.param_groups[0]["lr"] = conf.get_learning_rate()
        scheduler.load_state_dict(snapshot['scheduler'])

    return training_step, network, device, optimizer, scheduler


def save_model(training_step, network, optimizer, scheduler, training_dir):
    snapshot = {'training_step': training_step,
                'network': network.module.state_dict(),
                'optimizer': optimizer.state_dict(),
                'scheduler': scheduler.state_dict()}
    torch.save(snapshot, f"{training_dir}/model/weight_iter_{training_step}.pkl")
    torch.jit.script(network.module).save(f"{training_dir}/model/weight_iter_{training_step}.pt")


def calculate_loss(conf, network_output, label_policy, label_value, label_reward, loss_scale):
    # policy
    if conf.use_gumbel():
        loss_policy = nn.functional.kl_div(nn.functional.log_softmax(network_output["policy_logit"], dim=1), label_policy, reduction='batchmean')
    else:
        loss_policy = -((label_policy * nn.functional.log_softmax(network_output["policy_logit"], dim=1)).sum(dim=1) * loss_scale).mean()

    # value
    if conf.get_nn_discrete_value_size() == 1:
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


def train(game_type, training_dir, conf, model_file, data_loader, start_iter, end_iter):
    training_step, network, device, optimizer, scheduler = load_model(game_type, training_dir, model_file, conf)
    network = nn.DataParallel(network)

    if start_iter == -1:
        save_model(training_step, network, optimizer, scheduler, training_dir)
        return

    # load data
    data_loader.load_data(training_dir, start_iter, end_iter)

    training_info = {}
    for i in range(1, conf.get_training_step() + 1):
        optimizer.zero_grad()
        features, action_features, label_policy, label_value, label_reward, loss_scale = data_loader.sample_data(conf)

        if conf.get_nn_type_name() == "alphazero":
            network_output = network(features.to(device))
            loss_policy, loss_value, _ = calculate_loss(conf, network_output, label_policy[:, 0].to(device), label_value[:, 0].to(device), None, loss_scale.to(device))
            loss = loss_policy + loss_value

            # record training info
            add_training_info(training_info, 'loss_policy', loss_policy.item())
            add_training_info(training_info, 'accuracy_policy', calculate_accuracy(network_output["policy_logit"], label_policy[:, 0], conf.get_batch_size()))
            add_training_info(training_info, 'loss_value', loss_value.item())
        elif conf.get_nn_type_name() == "muzero":
            network_output = network(features.to(device))
            loss_step_policy, loss_step_value, loss_step_reward = calculate_loss(conf, network_output, label_policy[:, 0].to(device), label_value[:, 0].to(device), None, loss_scale.to(device))
            add_training_info(training_info, 'loss_policy_0', loss_step_policy.item())
            add_training_info(training_info, 'accuracy_policy_0', calculate_accuracy(network_output["policy_logit"], label_policy[:, 0], conf.get_batch_size()))
            add_training_info(training_info, 'loss_value_0', loss_step_value.item())
            loss_policy = loss_step_policy
            loss_value = loss_step_value
            loss_reward = loss_step_reward
            for i in range(conf.get_muzero_unrolling_step()):
                network_output = network(network_output["hidden_state"], action_features[:, i].to(device))
                loss_step_policy, loss_step_value, loss_step_reward = calculate_loss(
                    conf, network_output, label_policy[:, i + 1].to(device), label_value[:, i + 1].to(device), label_reward[:, i].to(device), loss_scale.to(device))
                add_training_info(training_info, f'loss_policy_{i+1}', loss_step_policy.item() / conf.get_muzero_unrolling_step())
                add_training_info(training_info, f'accuracy_policy_{i+1}', calculate_accuracy(network_output["policy_logit"], label_policy[:, i + 1], conf.get_batch_size()))
                add_training_info(training_info, f'loss_value_{i+1}', loss_step_value.item() / conf.get_muzero_unrolling_step())
                if "reward_logit" in network_output:
                    add_training_info(training_info, f'loss_reward_{i+1}', loss_step_reward.item() / conf.get_muzero_unrolling_step())
                loss_policy += loss_step_policy / conf.get_muzero_unrolling_step()
                loss_value += loss_step_value / conf.get_muzero_unrolling_step()
                loss_reward += loss_step_reward / conf.get_muzero_unrolling_step()
                network_output["hidden_state"].register_hook(lambda grad: grad / 2)
            loss = loss_policy + loss_value + loss_reward

            add_training_info(training_info, 'loss_policy', loss_policy.item())
            add_training_info(training_info, 'loss_value', loss_value.item())
            if "reward_logit" in network_output:
                add_training_info(training_info, 'loss_reward', loss_reward.item())

        loss.backward()
        optimizer.step()
        scheduler.step()

        training_step += 1
        if training_step != 0 and training_step % conf.get_training_display_step() == 0:
            eprint("[{}] nn step {}, lr: {}.".format(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), training_step, round(optimizer.param_groups[0]["lr"], 6)))
            for loss in training_info:
                eprint("\t{}: {}".format(loss, round(training_info[loss] / conf.get_training_display_step(), 5)))
            training_info = {}

    save_model(training_step, network, optimizer, scheduler, training_dir)
    print("Optimization_Done", training_step)
    eprint("Optimization_Done", training_step)


if __name__ == '__main__':
    if len(sys.argv) == 4:
        game_type = sys.argv[1]
        training_dir = sys.argv[2]
        conf_file_name = sys.argv[3]

        # import pybind library
        _temps = __import__(f'build.{game_type}', globals(), locals(), ['minizero_py'], 0)
        minizero_py = _temps.minizero_py
    else:
        eprint("python train.py game_type training_dir conf_file")
        exit(0)

    conf = minizero_py.Conf(conf_file_name)
    data_loader = MinizeroDadaLoader(conf_file_name)

    while True:
        try:
            command = input()
            if command == "keep_alive":
                continue
            elif command == "quit":
                eprint(f"[command] {command}")
                exit(0)
            eprint(f"[command] {command}")
            model_file, start_iter, end_iter = command.split()
            train(game_type, training_dir, conf, model_file.replace('"', ''), data_loader, int(start_iter), int(end_iter))
        except (KeyboardInterrupt, EOFError) as e:
            break

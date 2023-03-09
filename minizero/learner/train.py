#!/usr/bin/env python

import sys
import time
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
from torch.utils.data import DataLoader
from torch.utils.data import IterableDataset
from torch.utils.data import get_worker_info
from minizero.network.py.create_network import create_network


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


class MinizeroDataset(IterableDataset):
    def __init__(self, training_dir, start_iter, end_iter, conf, conf_file_name):
        self.training_dir = training_dir
        self.start_iter = start_iter
        self.end_iter = end_iter
        self.conf = conf
        self.conf_file_name = conf_file_name
        self.data_loader = minizero_py.DataLoader(self.conf_file_name)
        for i in range(self.start_iter, self.end_iter + 1):
            self.data_loader.load_data_from_file(f"{self.training_dir}/sgf/{i}.sgf")

    def __iter__(self):
        self.data_loader.seed(get_worker_info().id)
        while True:
            if self.conf.get_nn_type_name() == "alphazero":
                result_dict = self.data_loader.sample_alphazero_training_data()
                features = torch.FloatTensor(result_dict["features"]).view(self.conf.get_nn_num_input_channels(),
                                                                           self.conf.get_nn_input_channel_height(),
                                                                           self.conf.get_nn_input_channel_width())
                policy = torch.FloatTensor(result_dict["policy"])
                value = torch.FloatTensor(result_dict["value"])
                yield features, policy, value
            elif self.conf.get_nn_type_name() == "muzero":
                result_dict = self.data_loader.sample_muzero_training_data(self.conf.get_muzero_unrolling_step())
                features = torch.FloatTensor(result_dict["features"]).view(self.conf.get_nn_num_input_channels(),
                                                                           self.conf.get_nn_input_channel_height(),
                                                                           self.conf.get_nn_input_channel_width())
                actions = torch.FloatTensor(result_dict["actions"]).view(self.conf.get_muzero_unrolling_step(),
                                                                         self.conf.get_nn_num_action_feature_channels(),
                                                                         self.conf.get_nn_hidden_channel_height(),
                                                                         self.conf.get_nn_hidden_channel_width())
                policy = torch.FloatTensor(result_dict["policy"]).view(-1, self.conf.get_nn_action_size())
                value = torch.FloatTensor(result_dict["value"])
                yield features, actions, policy, value


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


def calculate_loss(conf, output_policy_logit, output_value, label_policy, label_value):
    if conf.use_gumbel():
        loss_policy = nn.functional.kl_div(nn.functional.log_softmax(output_policy_logit, dim=1), label_policy, reduction='batchmean')
    else:
        loss_policy = -(label_policy * nn.functional.log_softmax(output_policy_logit, dim=1)).sum() / output_policy_logit.shape[0]
    loss_value = torch.nn.functional.mse_loss(output_value, label_value)
    return loss_policy, loss_value


def add_training_info(training_info, key, value):
    if key not in training_info:
        training_info[key] = 0
    training_info[key] += value


def calculate_accuracy(output, label, batch_size):
    max_output = np.argmax(output.to('cpu').detach().numpy(), axis=1)
    max_label = np.argmax(label.to('cpu').detach().numpy(), axis=1)
    return (max_output == max_label).sum() / batch_size


def train(game_type, training_dir, conf_file_name, conf, model_file, start_iter, end_iter):
    training_step, network, device, optimizer, scheduler = load_model(game_type, training_dir, model_file, conf)
    network = nn.DataParallel(network)

    if start_iter == -1:
        save_model(training_step, network, optimizer, scheduler, training_dir)
        return

    # create dataset & dataloader
    dataset = MinizeroDataset(training_dir, start_iter, end_iter, conf, conf_file_name)
    data_loader = DataLoader(dataset, batch_size=conf.get_batch_size(), num_workers=conf.get_num_process())
    data_loader_iterator = iter(data_loader)

    training_info = {}
    for i in range(1, conf.get_training_step() + 1):
        optimizer.zero_grad()

        if conf.get_nn_type_name() == "alphazero":
            features, label_policy, label_value = next(data_loader_iterator)
            network_output = network(features.to(device))
            output_policy_logit, output_value = network_output["policy_logit"], network_output["value"]
            loss_policy, loss_value = calculate_loss(conf, output_policy_logit, output_value, label_policy.to(device), label_value.to(device))
            loss = loss_policy + loss_value

            # record training info
            add_training_info(training_info, 'loss_policy', loss_policy.item())
            add_training_info(training_info, 'accuracy_policy', calculate_accuracy(output_policy_logit, label_policy, conf.get_batch_size()))
            add_training_info(training_info, 'loss_value', loss_value.item())
        elif conf.get_nn_type_name() == "muzero":
            features, actions, label_policy, label_value = next(data_loader_iterator)
            network_output = network(features.to(device))
            output_policy_logit, output_value = network_output["policy_logit"], network_output["value"]
            loss_step_policy, loss_step_value = calculate_loss(conf, output_policy_logit, output_value, label_policy[:, 0].to(device), label_value.to(device))
            add_training_info(training_info, 'loss_policy_0', loss_step_policy.item())
            add_training_info(training_info, 'accuracy_policy_0', calculate_accuracy(output_policy_logit, label_policy[:, 0], conf.get_batch_size()))
            add_training_info(training_info, 'loss_value_0', loss_step_value.item())
            loss_policy = loss_step_policy
            loss_value = loss_step_value
            for i in range(conf.get_muzero_unrolling_step()):
                network_output = network(network_output["hidden_state"], actions[:, i].to(device))
                output_policy_logit, output_value = network_output["policy_logit"], network_output["value"]
                loss_step_policy, loss_step_value = calculate_loss(conf, output_policy_logit, output_value, label_policy[:, i + 1].to(device), label_value.to(device))
                add_training_info(training_info, f'loss_policy_{i+1}', loss_step_policy.item() / conf.get_muzero_unrolling_step())
                add_training_info(training_info, f'accuracy_policy_{i+1}', calculate_accuracy(output_policy_logit, label_policy[:, i + 1], conf.get_batch_size()))
                add_training_info(training_info, f'loss_value_{i+1}', loss_step_value.item() / conf.get_muzero_unrolling_step())
                loss_policy += loss_step_policy / conf.get_muzero_unrolling_step()
                loss_value += loss_step_value / conf.get_muzero_unrolling_step()
                network_output["hidden_state"].register_hook(lambda grad: grad / 2)
            loss = loss_policy + loss_value

            add_training_info(training_info, 'loss_policy', loss_policy.item())
            add_training_info(training_info, 'loss_value', loss_value.item())

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
            train(game_type, training_dir, conf_file_name, conf, model_file.replace('"', ''), int(start_iter), int(end_iter))
        except (KeyboardInterrupt, EOFError) as e:
            break

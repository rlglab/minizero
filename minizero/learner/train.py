import os
import sys
import time
import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
import minizero_py
from torch.utils.data import DataLoader
from torch.utils.data import IterableDataset
from torch.utils.data import get_worker_info
from minizero.network.py.create_network import create_network


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


class MinizeroDataset(IterableDataset):
    def __init__(self, training_dir, start_iter, end_iter, conf):
        self.training_dir = training_dir
        self.start_iter = start_iter
        self.end_iter = end_iter
        self.conf = conf

    def __iter__(self):
        np.random.seed(get_worker_info().id)
        self.data_loader = minizero_py.DataLoader()
        for i in range(self.start_iter, self.end_iter + 1):
            self.data_loader.load_data_from_file(f"{self.training_dir}/sgf/{i}.sgf")

        while True:
            index = np.random.randint(self.data_loader.get_data_size())
            result_dict = self.data_loader.get_feature_and_label(index)

            features = torch.FloatTensor(result_dict["features"]).view(self.conf.get_nn_num_input_channels(),
                                                                       self.conf.get_nn_input_channel_height(),
                                                                       self.conf.get_nn_input_channel_width())
            policy = torch.FloatTensor(result_dict["policy"])
            value = result_dict["value"]
            yield features, policy, value


def load_model(training_dir, model_file, conf):
    # training_step, network, device, optimizer, scheduler
    training_step = 0
    network = create_network(conf.get_game_name(),
                             conf.get_nn_num_input_channels(),
                             conf.get_nn_input_channel_height(),
                             conf.get_nn_input_channel_width(),
                             conf.get_nn_num_hidden_channels(),
                             conf.get_nn_hidden_channel_height(),
                             conf.get_nn_hidden_channel_width(),
                             conf.get_nn_num_blocks(),
                             conf.get_nn_num_action_channels(),
                             conf.get_nn_action_size(),
                             conf.get_nn_type_name())
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    network.to(device)
    optimizer = optim.SGD(network.parameters(),
                          lr=conf.get_learning_rate(),
                          momentum=conf.get_momentum(),
                          weight_decay=conf.get_weight_decay())
    scheduler = optim.lr_scheduler.StepLR(optimizer,
                                          step_size=1000000,
                                          gamma=0.1)

    if model_file:
        eprint(f"load model {training_dir}/model/{model_file}")
        snapshot = torch.load(f"{training_dir}/model/{model_file}",
                              map_location=torch.device('cpu'))
        training_step = snapshot['training_step']
        network.load_state_dict(snapshot['network'])
        optimizer.load_state_dict(snapshot['optimizer'])
        scheduler.load_state_dict(snapshot['scheduler'])

    return training_step, network, device, optimizer, scheduler


def save_model(training_step, network, optimizer, scheduler, training_dir):
    snapshot = {'training_step': training_step,
                'network': network.module.state_dict(),
                'optimizer': optimizer.state_dict(),
                'scheduler': scheduler.state_dict()}
    torch.save(snapshot, f"{training_dir}/model/weight_iter_{training_step}.pkl")
    torch.jit.script(network.module).save(f"{training_dir}/model/weight_iter_{training_step}.pt")


def calculate_loss(output_policy, output_value, label_policy, label_value):
    loss_policy = -(label_policy * nn.functional.log_softmax(output_policy, dim=1)).sum() / output_policy.shape[0]
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


if __name__ == '__main__':
    if len(sys.argv) == 6:
        training_dir = sys.argv[1]
        model_file = sys.argv[2]
        start_iter = int(sys.argv[3])
        end_iter = int(sys.argv[4])
        conf = minizero_py.Conf(sys.argv[5])
    else:
        eprint("python train.py training_dir model_file start_iter end_iter conf_file")
        exit(0)

    training_step, network, device, optimizer, scheduler = load_model(training_dir, model_file, conf)
    network = nn.DataParallel(network)

    if start_iter == -1:
        save_model(training_step, network, optimizer, scheduler, training_dir)
        exit(0)

    # create dataset & dataloader
    dataset = MinizeroDataset(training_dir, start_iter, end_iter, conf)
    data_loader = DataLoader(dataset,
                             batch_size=conf.get_batch_size(),
                             num_workers=conf.get_num_process())
    data_loader_iterator = iter(data_loader)

    training_info = {}
    for i in range(1, conf.get_training_step() + 1):
        optimizer.zero_grad()

        features, label_policy, label_value = next(data_loader_iterator)
        output_policy, output_value = network(features.to(device))
        loss_policy, loss_value = calculate_loss(output_policy, output_value, label_policy.to(device), label_value.to(device))
        loss = loss_policy + loss_value

        loss.backward()
        optimizer.step()
        scheduler.step()

        # record training info
        add_training_info(training_info, 'loss_policy', loss_policy.item())
        add_training_info(training_info, 'accuracy_policy', calculate_accuracy(output_policy, label_policy, conf.get_batch_size()))
        add_training_info(training_info, 'loss_value', loss_value.item())

        training_step += 1
        if training_step != 0 and training_step % conf.get_training_display_step() == 0:
            eprint("[{}] nn step {}, lr: {}.".format(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), training_step, round(optimizer.param_groups[0]["lr"], 6)), end=" ")
            for loss in training_info:
                eprint("{}: {}".format(loss, round(training_info[loss]/conf.get_training_display_step(), 5)), end=" ")
            eprint()
            training_info = {}

    save_model(training_step, network, optimizer, scheduler, training_dir)
    print("Optimization_Done", training_step)
    eprint("Optimization_Done", training_step)

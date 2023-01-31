#!/usr/bin/env python

import gym
import sys
import time
import numpy as np
from multiprocessing import Process, Manager


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


class AtariEnv:
    def __init__(self, game_name, seed=None):
        self.downsample_size = 96
        self.game_name = game_name
        self.env = gym.make(game_name, full_action_space=True)
        self.env = gym.wrappers.ResizeObservation(self.env, (self.downsample_size, self.downsample_size))
        self.reset(seed)

    def reset(self, seed=None):
        self.seed = np.random.randint(2147483647) if seed is None else seed
        self.done = False
        self.total_reward = 0
        self.observation, self.info = self.env.reset(seed=self.seed)

    def act(self, action_id):
        self.observation, self.reward, self.done, _, self.info = self.env.step(action_id)
        self.total_reward += self.reward

    def is_terminal(self):
        return self.done

    def get_features(self):
        features = np.transpose(self.observation, (2, 0, 1))
        features = np.reshape(features, (-1, self.downsample_size, self.downsample_size))
        return features

    def get_eval_score(self):
        return self.total_reward


def env_group(env_name, num_env, actor_start_id, input_commands, output_results, seed):
    np.random.seed(seed)
    envs = [AtariEnv(env_name, np.random.randint(2147483647)) for i in range(num_env)]
    while True:
        if len(input_commands) > 0:
            commands = input_commands[0].split()
            if commands[0] == "obs":
                for actor_id in range(len(envs)):
                    observation_string = " ".join([str(f) for f in envs[actor_id].get_features().flatten()])
                    output_results.append(f"obs {actor_id+actor_start_id} {observation_string}")
            elif commands[0] == "act":
                for act_string in zip(*[iter(commands[1:])] * 2):
                    actor_id, action_id = act_string
                    actor_id = int(actor_id) - actor_start_id
                    if actor_id < 0 or actor_id >= len(envs):
                        continue
                    envs[actor_id].act(int(action_id))

                    # return game info when game is ended
                    if envs[actor_id].is_terminal():
                        output_results.append(f"game {actor_id+actor_start_id} SD {envs[actor_id].seed} RE {envs[actor_id].get_eval_score()}")
                        envs[actor_id].reset()

                    # sync observation after act
                    observation_string = " ".join([str(f) for f in envs[actor_id].get_features().flatten()])
                    output_results.append(f"obs {actor_id+actor_start_id} {observation_string}")
            input_commands.pop(0)
        else:
            time.sleep(0.01)


def run_commands(command, num_process, input_commands, output_results):
    # send command to each process
    for i in range(num_process):
        input_commands[i].append(command)

    # wait for each process to finish
    for input_command in input_commands:
        while len(input_command) > 0:
            pass

    # write back the result
    for output_result in output_results:
        while len(output_result) > 0:
            print(output_result[-1], end=";", flush=True)
            output_result.pop()

    # write ending mark
    print("", flush=True)


if __name__ == '__main__':
    if len(sys.argv) == 3:
        env_name = sys.argv[1]
        num_process = int(sys.argv[2])
    else:
        eprint("python atari_env.py env_name num_process")
        exit(0)

    num_env = int(input().split()[1])
    num_env_per_process = num_env // num_process
    manager = Manager()
    processes = []
    input_commands = []
    output_results = []
    for i in range(num_process):
        input_commands.append(manager.list())
        output_results.append(manager.list())
        processes.append(Process(target=env_group, args=(env_name, num_env_per_process, i * num_env_per_process, input_commands[-1], output_results[-1], np.random.randint(2147483647))))
        processes[-1].start()
    run_commands("obs", num_process, input_commands, output_results)

    # main loop
    while True:
        commands = input()
        run_commands(commands, num_process, input_commands, output_results)

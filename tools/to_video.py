#!/usr/bin/env python

import gym
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import sys
import argparse
import os
import time
from multiprocessing import Process
import warnings

ACTION_MEANING = {
    0: "NOOP",
    1: "FIRE",
    2: "UP",
    3: "RIGHT",
    4: "LEFT",
    5: "DOWN",
    6: "UPRIGHT",
    7: "UPLEFT",
    8: "DOWNRIGHT",
    9: "DOWNLEFT",
    10: "UPFIRE",
    11: "RIGHTFIRE",
    12: "LEFTFIRE",
    13: "DOWNFIRE",
    14: "UPRIGHTFIRE",
    15: "UPLEFTFIRE",
    16: "DOWNRIGHTFIRE",
    17: "DOWNLEFTFIRE",
}


class AtariEnv:
    def __init__(self, game_name, gym_game_name, seed=None):
        self.game_name = game_name
        self.env = gym.make(gym_game_name, frameskip=4, repeat_action_probability=0.25, full_action_space=True, render_mode="rgb_array")
        self.reset(seed)

    def reset(self, seed):
        self.seed = seed
        self.done = False
        self.total_reward = 0
        self.env.ale.setInt("random_seed", seed)
        self.env.ale.loadROM(f"/opt/atari57/{self.game_name}.bin")
        self.observation, self.info = self.env.reset()
        self.frames = [self.env.render()]

    def act(self, action_id):
        self.observation, self.reward, self.done, _, self.info = self.env.step(action_id)
        self.frames.append(self.env.render())
        self.total_reward += self.reward

    def is_terminal(self):
        return self.done

    def get_eval_score(self):
        return self.total_reward


def save_video(video_file_name, index, record, fps, force):
    # collect frames
    seed = int(record.split("SD[")[1].split("]")[0])
    env_name = record.split("GM[")[1].split("]")[0].replace("atari_", "")
    if not force and os.path.isfile(f'{video_file_name}/{env_name}-{index}.mp4'):
        print(f'*** {video_file_name}/{env_name}-{index}.mp4 exists! Use --force to overwrite it. ***')
        return
    env = AtariEnv(env_name, 'ALE/' + ''.join([w.capitalize() for w in env_name.split('_')]) + '-v5', seed)
    for action in record.split("B[")[1:]:
        action_id = int(action.split("|")[0].split(']')[0])
        # mapping action
        action_id = env.env.get_action_meanings().index(ACTION_MEANING[action_id])
        env.act(action_id)

    # check consistency
    if env.get_eval_score() != float(record.split("RE[")[1].split("]")[0]):
        print(f"replay mismatch, score: {env.get_eval_score()}, record_score: {record.split('RE[')[1].split(']')[0]}")

    # save video
    img = plt.imshow(env.frames[0])
    plt.axis('off')
    plt.tight_layout()
    video = FuncAnimation(plt.gcf(), lambda i: img.set_data(env.frames[i]), frames=len(env.frames))
    video.save(f'{video_file_name}/{env_name}-{index}.mp4', writer=matplotlib.animation.FFMpegWriter(fps=fps))


def join_all_processes(all_processes):
    for i in range(len(all_processes)):
        all_processes[i].join()
    all_processes.clear()


def process_datas(video_file_name, source, num_processes, fps, force):
    assert num_processes > 0
    all_processes = []
    for index, record in enumerate(source):
        all_processes.append(Process(target=save_video, args=(video_file_name, index, record, fps, force)))
        all_processes[len(all_processes) - 1].start()
        if len(all_processes) >= num_processes:
            join_all_processes(all_processes)
    if len(all_processes) > 0:
        join_all_processes(all_processes)


def mkdir(dir):
    if not os.path.isdir(dir):
        os.mkdir(dir)


if __name__ == '__main__':
    warnings.filterwarnings("ignore")
    parser = argparse.ArgumentParser()
    parser.add_argument('-in_file', dest='fin_name', type=str, help='input flie')
    parser.add_argument('-out_dir', dest='dir_name', type=str, default=time.strftime(
        '%Y-%m-%d %H:%M:%S', time.localtime()), help='output directory (default: current time)')
    parser.add_argument('-c', dest='num_processes', type=int,
                        default=8, help='process number (default: 8)')
    parser.add_argument('-fps', dest='fps', type=int,
                        default=60, help='fps (default: 60)')
    parser.add_argument('--force', action='store_true',
                        dest='force', help='overwrite files')
    args = parser.parse_args()
    mkdir(args.dir_name)
    if args.fin_name:
        if os.path.isfile(args.fin_name):
            with open(args.fin_name, 'r') as fin:
                process_datas(video_file_name=args.dir_name, source=fin.readlines(
                ), num_processes=args.num_processes, fps=args.fps, force=args.force)
        else:
            print(f'\"{args.fin_name}\" does not exist!')
            exit(1)
    else:
        process_datas(video_file_name=args.dir_name, source=sys.stdin,
                      num_processes=args.num_processes, fps=args.fps, force=args.force)

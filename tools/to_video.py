#!/usr/bin/env python

import gym
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


class AtariEnv:
    def __init__(self, game_name, gym_game_name, seed=None):
        self.game_name = game_name
        self.env = gym.make(gym_game_name, frameskip=4, repeat_action_probability=0.0, full_action_space=True, render_mode="rgb_array")
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


def save_video(record, video_file_name=None):
    # collect frames
    seed = int(record.split("SD[")[1].split("]")[0])
    env_name = record.split("GM[")[1].split("]")[0].replace("atari_", "")
    env = AtariEnv(env_name, 'ALE/' + ''.join([w.capitalize() for w in env_name.split('_')]) + '-v5', seed)
    for action in record.split("B[")[1:]:
        action_id = int(action.split("|")[0])
        env.act(action_id)

    # check consistency
    if env.get_eval_score() != float(record.split("RE[")[1].split("]")[0]):
        print(f"replay mismatch, score: {env.get_eval_score()}, record_score: {record.split('RE[')[1].split(']')[0]}")

    # save video
    print(env.frames[0].shape)
    img = plt.imshow(env.frames[0])
    plt.axis('off')
    plt.tight_layout()
    video = FuncAnimation(plt.gcf(), lambda i: img.set_data(env.frames[i]), frames=len(env.frames))
    video_file_name = env_name + ".mp4" if video_file_name is None else video_file_name
    # video.save(video_file_name, writer='imagemagick', fps=60)
    video.save(video_file_name, writer=matplotlib.animation.FFMpegWriter(fps=60))


if __name__ == '__main__':
    save_video(input())

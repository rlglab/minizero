import numpy as np
import pandas as pd
import os
import argparse
import matplotlib.pyplot as plt


def getReturns(file):
    rewards = []
    game_lengths = []
    game_name = ''
    with open(file, 'r') as f:
        lines = f.readlines()
        step_num = int(lines[0].split('EV[weight_iter_')[1].split('.pt]')[0])
        game_name = lines[0].split('GM[')[1].split(']')[0]
        for line in lines:
            rewards.append(float(line.split('RE[')[1].split(']')[0]))
            game_lengths.append(int(line.split('DLEN[0-')[1].split(']')[0]))
    return step_num, rewards, game_name, game_lengths


def plot_curve(game_name, fig_name, step_nums, all_means, all_stds):
    _, ax = plt.subplots()
    ax.set_title(game_name)
    ax.set_xlabel('nn steps')
    ax.set_ylabel('return')
    ax.plot(step_nums, all_means)
    plt.fill_between(step_nums, all_means - all_stds, all_means +
                     all_stds, color='green', alpha=0.1)
    plt.savefig(fig_name)


def eval(dir, fout_name, isplot):
    game_name = ''
    result = {'nn steps': [],
              'Min': [],
              'Max': [],
              'Median': [],
              'Mean': [],
              'Std': [],
              'Avg. Len.': [],
              'Total': [], }
    for file in os.listdir(dir):
        sgf = os.path.join(dir, file)
        if os.path.isfile(sgf):
            try:
                step_num, rewards, game_name, game_lengths = getReturns(sgf)
            except BaseException:
                continue
            result['nn steps'].append(step_num)
            result['Min'].append(np.min(rewards))
            result['Max'].append(np.max(rewards))
            result['Median'].append(np.median(rewards))
            result['Mean'].append(np.mean(rewards))
            result['Std'].append(np.std(rewards))
            result['Avg. Len.'].append(np.array(game_lengths).mean().round(2))
            result['Total'].append(len(rewards))
    result['Mean'] = np.array(result['Mean']).round(2)
    result['Median'] = np.array(result['Median']).round(2)
    result['Std'] = np.array(result['Std']).round(2)
    result_df = pd.DataFrame(result).sort_values(['nn steps'])
    result_df.to_csv(os.path.join(dir, f'{fout_name}.csv'), index=False)
    with open(os.path.join(dir, f'{fout_name}.txt'), 'w') as f:
        print(result_df.to_string(index=False), file=f)
    print(result_df.to_string(index=False))
    if isplot:
        plot_curve(game_name, os.path.join(
            dir, f'{fout_name}.png'), result_df['nn steps'], result_df['Mean'], result_df['Std'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-in_dir', dest='dir', type=str, help='dir')
    parser.add_argument('-out_file', dest='fout_name', type=str, default='reward',
                        help='output flie')
    parser.add_argument('--plot', action='store_true', dest='isplot',
                        help='plot return curve')
    args = parser.parse_args()
    if args.dir and os.path.isdir(args.dir):
        eval(args.dir, args.fout_name, args.isplot)
    else:
        parser.print_help()
        print(f'{args.dir}: No such directory!')

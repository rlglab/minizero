#!/usr/bin/env python

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import argparse
import os
import sys
import time
import re
from datetime import datetime
plt.rcParams.update({'figure.max_open_warning': 100})


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


def get_time(log_entry):
    timestamp_pattern = r"\[(\d{4}/\d{2}/\d{2}_\d{2}:\d{2}:\d{2}\.\d{3})\]"
    time_str = re.search(timestamp_pattern, log_entry).group(1)
    return datetime.strptime(time_str, "%Y/%m/%d_%H:%M:%S.%f")


def analysis(training_dir, path, iter: int = -1, all: bool = False, name: bool = False):
    path = os.path.join(training_dir, path)
    if not os.path.isdir(path):
        os.mkdir(path)
    analysis_(training_dir, path, iter)


def get_myDict(lines, iter):
    set_learner_training_display_step = True
    set_learner_training_step = True
    learner_training_display_step = 0
    learner_training_step = 0
    myDict = {"[Iteration]": [], "Time": [], "OP Time": [], "SP Time": []}
    # parse log
    for index in range(2):
        counter = 1
        for line in lines[index]:
            if not line:
                continue
            if iter != -1 and counter >= iter:
                break
            ret3 = re.findall(r'(loss|accuracy)_(\S+)(_\d)?:\s(-?\d+\.?\d*(?:[Ee]-?\d+)?)', line)
            if ret3 != []:
                key = ret3[0][0] + "_" + ret3[0][1] + ret3[0][2]
                if key not in myDict:
                    myDict[key] = []
                myDict[key].append(float(ret3[0][3]))
                continue
            ret1 = re.findall(r'(\[SelfPlay\s[^Std]\w+.\s[^Data]\w+\s(Lengths|Returns)\])\s(-?\d+\.\d+|\d+)', line)
            if ret1 != []:
                key = ret1[0][0]
                if key not in myDict:
                    myDict[key] = []
                myDict[key].append(float(ret1[0][2]))
                if re.findall(r'(\[SelfPlay Avg. Game Returns\])', line):
                    myDict["[Iteration]"].append(counter)
                continue
            ret2 = re.findall(r'((\[Iteration\])\s={5}(\d+)={5})', line)
            if ret2 != []:
                counter += 1
            if index == 0 and re.findall(r'(Optimization_Done)\s(\d+)', line) != []:  # op.log
                counter += 1
            if index == 0:  # op.log
                ret4 = re.findall(r'(nn\sstep)\s(\d+),', line)
                if ret4 != [] and set_learner_training_display_step:
                    set_learner_training_display_step = False
                    learner_training_display_step = int(ret4[0][1])
                ret5 = re.findall(r'(Optimization_Done)\s(\d+)', line)
                if ret5 != [] and set_learner_training_step:
                    learner_training_step = int(ret5[0][1])
                    set_learner_training_step = False
            if index == 1:  # Training.log
                ret1 = re.findall(r'((\[.*\])\s\[Iteration\]\s={5}(\d+)={5})', line)
                if ret1 != []:
                    Start_time = get_time(ret1[0][1])
                ret2 = re.findall(r'((\[.*\])\s\[Optimization\]\sStart.)', line)
                if ret2 != []:
                    OP_start_time = get_time(ret2[0][1])
                ret3 = re.findall(r'((\[.*\])\s\[Optimization\]\sFinished.)', line)
                if ret3 != []:
                    Finished_time = get_time(ret3[0][1])
                    myDict["Time"].append((Finished_time - Start_time).total_seconds())
                    myDict["OP Time"].append((Finished_time - OP_start_time).total_seconds())
                    myDict["SP Time"].append((OP_start_time - Start_time).total_seconds())
    return myDict, learner_training_display_step, learner_training_step


def graph_print(tmp, iter):
    lines = []
    op_lines = []
    myDict = {}
    for item in tmp:
        log = open(os.path.join(item, "Training.log"), 'r')
        op_log = open(os.path.join(item, "op.log"), 'r')
        lines.append(log.readlines())
        op_lines.append(op_log.readlines())
        log.close()
        op_log.close()
    nn_step = []
    learner_training_step = 0
    for index in range(len(op_lines)):
        set_learner_training_step = True
        for line in op_lines[index]:
            ret5 = re.findall(r'(Optimization_Done)\s(\d+)', line)
            if ret5 != [] and set_learner_training_step:
                learner_training_step = int(ret5[0][1])
                nn_step.append(int(ret5[0][1]))
                set_learner_training_step = False
    for index in range(len(lines)):
        counter = 0
        for line in lines[index]:
            if iter != -1 and counter >= iter:
                break
            ret2 = re.findall(r'((\[Iteration\])\s={5}(\d+)={5})', line)
            if ret2 != []:
                counter = int(ret2[0][2])
            ret1 = re.findall(r'(\[SelfPlay Avg. Game Returns\])\s(-?\d+\.\d+|\d+)', line)
            if ret1 != []:
                key = f"[SelfPlay Avg. Game Returns] {index}"
                if key not in myDict:
                    myDict[key] = []
                myDict[key].append(float(ret1[0][1]))
                if f"[Iteration] {index}" not in myDict:
                    myDict[f"[Iteration] {index}"] = []
                myDict[f"[Iteration] {index}"].append(counter)
    plt.rcParams.update({'font.size': 30})
    plt.figure(figsize=(25, 20))
    for index in range(len(tmp)):
        bool_print = True
        width = 4
        plt.plot([x * nn_step[index] for x in myDict[f"[Iteration] {index}"]], myDict[f"[SelfPlay Avg. Game Returns] {index}"], label=tmp[index], linewidth=width)
    if bool_print:
        plt.title(f'[SelfPlay Avg. Game Returns] graph', fontsize=30)
        plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.05), ncol=1)
        plt.xlabel('nn steps', fontsize=30)
        plt.ylabel('Return', fontsize=30)
        plt.grid()
        plt.tight_layout()
        plt.show()
        plt.savefig("compare_graph")
        eprint("compare_graph")


def format_y_axis_labels(value, pos):
    if value >= 1000:
        value /= 1000
        return f'{value:.0f}k'
    else:
        return f'{value:.2f}'


def analysis_(dir, path, iter, all: bool = False, name: bool = False):
    # read log
    op_log = open(os.path.join(dir, "op.log"), 'r')
    Training_log = open(os.path.join(dir, "Training.log"), 'r')
    lines = []
    lines.append(op_log.readlines())
    lines.append(Training_log.readlines())
    op_log.close()
    Training_log.close()
    # plt target
    myDict, learner_training_display_step, learner_training_step = get_myDict(lines, iter)
    Fig_list = list(set(["Lengths", "Time", "Returns"] + [re.sub(r"_\d+$", "", key) for key in myDict if re.match(r'^(loss|accuracy)_', key)]))
    # plt figure
    counter_subplot = sum([1 for word in Fig_list if word in ' '.join(myDict.keys())])
    if counter_subplot == 0 or len(myDict["Time"]) == 0:
        return
    plt.rcParams.update({'font.size': 30})
    fig, axs = plt.subplots(1, counter_subplot, figsize=(190, 30))
    counter_fig = 0
    for item in Fig_list:
        fig_one = plt.figure(figsize=(25, 20))
        ax1 = fig_one.add_subplot(111)
        ax2 = ax1.twiny()
        width = 4
        bool_print = False
        for key in myDict:
            if item in key:
                bool_print = True
                if "Returns" in item or "Lengths" in item:
                    step_interval = learner_training_step
                    if "[SelfPlay Avg. Game Returns]" in key:
                        ax1.plot([x * step_interval for x in myDict["[Iteration]"]], myDict[key], color='red', label=f'{key}', linewidth=width)
                    else:
                        ax1.plot([x * step_interval for x in myDict["[Iteration]"]], myDict[key], label=f'{key}', linewidth=width)
                    axs[counter_fig].plot([x * step_interval for x in myDict["[Iteration]"]], myDict[key], label=f'{key}', linewidth=width)
                    ax1.yaxis.set_major_formatter(ticker.FuncFormatter(format_y_axis_labels))
                else:
                    step_interval = learner_training_display_step
                    if "Time" in item:
                        step_interval = learner_training_step
                    ax1.plot([(x + 1) * step_interval for x in list(range(len(myDict[key])))], myDict[key], label=f'{key}', linewidth=width)
                    axs[counter_fig].plot([(x + 1) * step_interval for x in list(range(len(myDict[key])))], myDict[key], label=f'{key}', linewidth=width)
                ax2.set_xlim([ax1.get_xlim()[0] / learner_training_step, ax1.get_xlim()[1] / learner_training_step])
                axs_twiny = axs[counter_fig].twiny()
                axs_twiny.set_xlim([ax1.get_xlim()[0] / learner_training_step, ax1.get_xlim()[1] / learner_training_step])
        if bool_print:
            plt.title(f'{item} of {dir} in op.log', fontsize=30)
            axs[counter_fig].set_title(f'{item} of {dir} in op.log')
            axs[counter_fig].legend(loc='upper center', bbox_to_anchor=(0.5, -0.05), ncol=3)
            ax1.legend(loc='upper center', bbox_to_anchor=(0.5, -0.05), ncol=3)
            ax1.set_xlabel('nn steps', fontsize=30)
            ax2.set_xlabel('iterations', fontsize=30)
            axs[counter_fig].set(xlabel='nn steps', ylabel=f'{item}')
            axs_twiny.set_xlabel('iterations', fontsize=30)
            ax1.set_ylabel(f'{item}', fontsize=30)
            ax1.grid()
            axs[counter_fig].grid()
            plt.tight_layout()
            dir = os.path.dirname(dir + "/")
            plt.savefig(os.path.join(f'{path}', f'{str(dir.split("/")[-1])}_{item}.png'))
            if name:
                eprint(os.path.join(f'{path}', f'{str(dir.split("/")[-1])}_{item}.png'))
            plt.cla()
            counter_fig += 1
    if all:
        fig.tight_layout()
        fig.savefig(os.path.join(f'{path}', f'{str(dir.split("/")[-1])}.png'))
        eprint(os.path.join(f'{path}', f'{str(dir.split("/")[-1])}.png'))
    plt.close('all')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-in_dir', dest='in_dir', default='', help='dir to anaylisis')
    parser.add_argument('-out_dir', dest='out_dir', type=str, default="analysis_", help='output directory (default: analysis_{iter})')
    parser.add_argument('-iter', dest='iter', default=-1, type=int, help='iteration to anaylisis')
    parser.add_argument('-compare', dest='compare', default='', type=str, help='compare many version of same game, use commas as separators. ex: -compare a,b')
    parser.add_argument('--all', dest='all', default=False, action="store_true", help='all analysis in a graph, default false')
    args = parser.parse_args()
    if args.iter != -1:
        out_dir = args.out_dir + str(args.iter)
    else:
        out_dir = args.out_dir
    compare = args.compare
    tmp = compare.split(',')
    if args.in_dir:
        dir = args.in_dir
        if dir and os.path.isdir(dir):
            # use analysis
            name = True
            # create dir analysis
            path = os.path.join(dir, f'{out_dir}')
            if not os.path.isdir(path):
                os.mkdir(path)
            analysis_(dir, path, args.iter, args.all, name)
        else:
            eprint(f'\"{dir}\" does not exist!')
            exit(1)
    elif tmp != ['']:
        for item in tmp:
            if not os.path.isdir(item):
                eprint(f'\"{item}\" does not exist!')
                exit(1)
        graph_print(tmp, args.iter)
    else:
        parser.print_help()
        exit(1)

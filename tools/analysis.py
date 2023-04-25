import matplotlib.pyplot as plt
from matplotlib.pyplot import MultipleLocator
import argparse
import os


def analysis(dir):
    # read log
    op_log = open(f'./{dir}/op.log', 'r')
    Training_log = open(f'./{dir}/Training.log', 'r')
    lines = []
    lines.append([fi for fi in op_log.read().splitlines()])
    lines.append([fi for fi in Training_log.read().splitlines()])
    op_log.close()
    Training_log.close()

    # plt target
    Returns_list = ["[SelfPlay Min. Game Returns]", "[SelfPlay Max. Game Returns]", "[SelfPlay Avg. Game Returns]"]
    Game_list = ["[SelfPlay Max. Game Lengths]", "[SelfPlay Avg. Game Lengths]"]
    word_list = ["accuracy_policy", "loss_value", "loss_reward", "loss_policy", "Lengths", "Returns"]

    # init variables
    counter = 0
    set_learner_training_display_step = True
    myDict = {}
    video_list = []
    # parse log
    for index in range(2):
        for line in lines[index]:
            if not line:
                continue
            if "accuracy_policy" in line or "loss" in line:
                x = line.split("\t")[1].split(": ")  # get key and value
                key = x[0]
                if key not in myDict:
                    myDict[key] = []
                myDict[key].append(float(x[1]))
            if "[SelfPlay Avg. Game Lengths]" in line or "[SelfPlay Max. Game Lengths]" in line:
                x = line.split(" ")
                key = x[1] + " " + x[2] + " " + x[3] + " " + x[4]
                if key not in myDict:
                    myDict[key] = []
                myDict[key].append(float(x[5]))
            if "[SelfPlay Min. Game Returns]" in line or "[SelfPlay Max. Game Returns]" in line or "[SelfPlay Avg. Game Returns]" in line:
                x = line.split(" ")
                key = x[1] + " " + x[2] + " " + x[3] + " " + x[4]
                if key not in myDict:
                    myDict[key] = []
                myDict[key].append(float(x[5]))
                if "[SelfPlay Min. Game Returns]" in line:
                    myDict["[Iteration]"].append(counter)
                if key == "[SelfPlay Max. Game Returns]":
                    video_list.append((float(x[5]), counter))
            if index == 1:  # Training.log
                if "[Iteration]" in line:
                    x = line.split(" ")
                    key = x[1]
                    if key not in myDict:
                        myDict[key] = []
                    counter += 1
            if index == 0 and set_learner_training_display_step:  # op.log
                if "nn step" in line:
                    x = line.split(" ")[4].split(",")[0]
                    learner_training_display_step = int(x)
                    set_learner_training_display_step = False
    # plt figure
    counter_subplot = 0
    for item in word_list:
        for key in myDict:
            if item in key:
                counter_subplot += 1
                break
    plt.rcParams.update({'font.size': 30})
    fig, axs = plt.subplots(counter_subplot, figsize=(30, 120))
    counter_fig = 0
    for item in word_list:
        plt.figure(figsize=(25, 20))
        width = 4
        bool_print = False

        for key in myDict:
            if item in key:
                bool_print = True
                if "Returns" in item:
                    plt.plot(myDict["[Iteration]"], myDict[key], label=f'{key}', linewidth=width)
                    axs[counter_fig].plot(myDict["[Iteration]"], myDict[key], label=f'{key}', linewidth=width)
                else:
                    plt.plot([x * learner_training_display_step for x in list(range(len(myDict[key])))], myDict[key], label=f'{key}', linewidth=width)
                    axs[counter_fig].plot([x * learner_training_display_step for x in list(range(len(myDict[key])))], myDict[key], label=f'{key}', linewidth=width)
        if bool_print:
            ax = plt.gca()
            plt.title(f'{item} of {dir} in op.log', fontsize=30)
            axs[counter_fig].set_title(f'{item} of {dir} in op.log')
            axs[counter_fig].legend(loc='upper center', bbox_to_anchor=(0.5, -0.05), ncol=3)
            plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.05), ncol=3)
            if "Returns" in item:
                plt.xlabel('iteration', fontsize=30)
                axs[counter_fig].set(xlabel='iteration', ylabel=f'{item}')
            else:
                plt.xlabel('nn step', fontsize=30)
                axs[counter_fig].set(xlabel='nn step', ylabel=f'{item}')
            plt.ylabel(f'{item}', fontsize=30)
            plt.grid()
            axs[counter_fig].grid()
            plt.tight_layout()
            plt.show()
            plt.savefig(f'{path}/{dir}_{item}')
            print(f'{path[2:]}/{dir}_{item}.png')
            counter_fig += 1
    fig.tight_layout()
    fig.savefig(f'{path}/{dir}.png')
    print(f'{path[2:]}/{dir}.png')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-dir', dest='dir', default='', help='dir to anaylisis')
    args = parser.parse_args()
    if args.dir:
        dir = args.dir
        if dir[-1] == "/":
            dir = dir[:-1]
        if dir and os.path.isdir(dir):
            # create dir analysis
            path = f'./{dir}/analysis'
            if not os.path.isdir(path):
                os.mkdir(path)
            analysis(dir)
        else:
            print(f'\"{args.dir}\" does not exist!')
            exit(1)
    else:
        print(f'No dir')
        exit(1)

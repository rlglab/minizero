import matplotlib.pyplot as plt
import argparse
import os


def analysis(dir):
    # read log
    op_log = open(os.path.join(dir, "op.log"), 'r')
    Training_log = open(os.path.join(dir, "Training.log"), 'r')
    lines = []
    lines.append([fi for fi in op_log.read().splitlines()])
    lines.append([fi for fi in Training_log.read().splitlines()])
    op_log.close()
    Training_log.close()
    # plt target
    Returns_list = ["[SelfPlay Min. Game Returns]", "[SelfPlay Max. Game Returns]", "[SelfPlay Avg. Game Returns]"]
    Game_list = ["[SelfPlay Max. Game Lengths]", "[SelfPlay Avg. Game Lengths]"]
    Fig_list = ["accuracy_policy", "loss_value", "loss_reward", "loss_policy", "Lengths", "Returns"]
    # init variables
    counter = 0
    set_learner_training_display_step = True
    set_learner_training_step = True
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
            if index == 0 and set_learner_training_step:  # op.log
                if "nn step" in line and set_learner_training_display_step:
                    x = line.split(" ")[4].split(",")[0]
                    set_learner_training_display_step = False
                    learner_training_display_step = int(x)
                if "Optimization_Done" in line:
                    x = line.split(" ")[1]
                    learner_training_step = int(x)
                    set_learner_training_step = False
    # plt figure
    counter_subplot = sum([1 for word in Fig_list if word in ' '.join(myDict.keys())])
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
                else:
                    step_interval = learner_training_display_step
                ax1.plot([x * step_interval for x in list(range(len(myDict[key])))], myDict[key], label=f'{key}', linewidth=width)
                axs[counter_fig].plot([x * step_interval for x in list(range(len(myDict[key])))], myDict[key], label=f'{key}', linewidth=width)
                ax2.set_xlim([ax1.get_xlim()[0] / learner_training_step, ax1.get_xlim()[1] / learner_training_step])
                axs_twiny = axs[counter_fig].twiny()
                axs_twiny.set_xlim([ax1.get_xlim()[0] / learner_training_step, ax1.get_xlim()[1] / learner_training_step])
        if bool_print:
            ax = plt.gca()
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
            plt.show()
            path = os.path.join(dir, "analysis")
            plt.savefig(os.path.join(dir, "analysis", f'{os.path.dirname(dir+"/")}_{item}.png'))
            print(os.path.join(dir, "analysis", f'{os.path.dirname(dir+"/")}_{item}.png'))
            counter_fig += 1
    fig.tight_layout()
    fig.savefig(os.path.join(dir, "analysis", f'{os.path.dirname(dir+"/")}.png'))
    print(os.path.join(dir, "analysis", f'{os.path.dirname(dir+"/")}.png'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--dir', dest='dir', default='', help='dir to anaylisis')
    args = parser.parse_args()
    if args.dir:
        dir = args.dir
        if dir and os.path.isdir(dir):
            # create dir analysis
            path = os.path.join(dir, "analysis")
            if not os.path.isdir(path):
                os.mkdir(path)
            analysis(dir)
        else:
            print(f'\"{args.dir}\" does not exist!')
            exit(1)
    else:
        parser.print_help()
        exit(1)

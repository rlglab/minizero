#include "mode_handler.h"
#include "actor_group.h"
#include "console.h"
#include "git_info.h"
#include "obs_recover.h"
#include "obs_remover.h"
#include "ostream_redirector.h"
#include "random.h"
#include "zero_server.h"
#include "base_actor.h"
#include "network.h"
#include "create_actor.h"
#include "create_network.h"
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include <assert.h>

namespace minizero::console {

using namespace minizero::utils;
using namespace minizero::network;

ModeHandler::ModeHandler()
{
    RegisterFunction("console", this, &ModeHandler::runConsole);
    RegisterFunction("sp", this, &ModeHandler::runSelfPlay);
    RegisterFunction("zero_server", this, &ModeHandler::runZeroServer);
    RegisterFunction("zero_training_name", this, &ModeHandler::runZeroTrainingName);
    RegisterFunction("env_test", this, &ModeHandler::runEnvTest);
    RegisterFunction("env_test2", this, &ModeHandler::runEnvTest2);
    RegisterFunction("env_test3", this, &ModeHandler::runEnvTest3);
    RegisterFunction("env_evaluation", this, &ModeHandler::runEvaluation);
    RegisterFunction("remove_obs", this, &ModeHandler::runRemoveObs);
    RegisterFunction("recover_obs", this, &ModeHandler::runRecoverObs);
}

void ModeHandler::run(int argc, char* argv[])
{
    if (argc % 2 != 1) { usage(); }

    env::setUpEnv();

    std::string mode_string = "console";
    std::string config_file = "";
    std::string config_string = "";
    config::ConfigureLoader cl;
    setDefaultConfiguration(cl);

    std::string gen_config = "";
    for (int i = 1; i < argc; i += 2) {
        std::string sCommand = std::string(argv[i]);

        if (sCommand == "-mode") {
            mode_string = argv[i + 1];
        } else if (sCommand == "-gen") {
            gen_config = argv[i + 1];
        } else if (sCommand == "-conf_file") {
            config_file = argv[i + 1];
        } else if (sCommand == "-conf_str") {
            config_string = argv[i + 1];
        } else {
            std::cerr << "unknown argument: " << sCommand << std::endl;
            usage();
        }
    }

    if (!readConfiguration(cl, config_file, config_string)) { exit(-1); }
    utils::OstreamRedirector::silence(std::cerr, config::program_quiet);                                  // silence std::cerr if program_quiet
    utils::Random::seed(config::program_auto_seed ? static_cast<int>(time(NULL)) : config::program_seed); // setup random seed

    if (!gen_config.empty()) {
        // generate configuration file after reading cfg file
        genConfiguration(cl, gen_config);
        exit(0);
    } else {
        std::cerr << "(Version: " << GIT_SHORT_HASH << ")" << std::endl;
        // run mode
        if (!function_map_.count(mode_string)) { usage(); }
        (*function_map_[mode_string])();
    }
}

void ModeHandler::usage()
{
    std::cout << "./minizero [arguments]" << std::endl;
    std::cout << "arguments:" << std::endl;
    std::cout << "\t-mode [" << getAllModesString() << "]" << std::endl;
    std::cout << "\t-gen configuration_file" << std::endl;
    std::cout << "\t-conf_file configuration_file" << std::endl;
    std::cout << "\t-conf_str configuration_string" << std::endl;
    exit(-1);
}

std::string ModeHandler::getAllModesString()
{
    std::string mode_string;
    for (const auto& m : function_map_) { mode_string += (mode_string.empty() ? "" : "|") + m.first; }
    return mode_string;
}

void ModeHandler::genConfiguration(config::ConfigureLoader& cl, const std::string& config_file)
{
    // check configure file is exist
    std::ifstream f(config_file);
    if (f.good()) {
        char ans = ' ';
        while (ans != 'y' && ans != 'n') {
            std::cerr << config_file << " already exist, do you want to overwrite it? [y/n]" << std::endl;
            std::cin >> ans;
        }
        if (ans == 'y') { std::cerr << "overwrite " << config_file << std::endl; }
        if (ans == 'n') {
            std::cerr << "didn't overwrite " << config_file << std::endl;
            f.close();
            return;
        }
    }
    f.close();

    std::ofstream fout(config_file);
    fout << cl.toString();
    fout.close();
}

bool ModeHandler::readConfiguration(config::ConfigureLoader& cl, const std::string& config_file, const std::string& config_string)
{
    if (!config_file.empty() && !cl.loadFromFile(config_file)) {
        std::cerr << "Failed to load configuration file." << std::endl;
        return false;
    }
    if (!config_string.empty() && !cl.loadFromString(config_string)) {
        std::cerr << "Failed to load configuration string." << std::endl;
        return false;
    }

    if (!config::program_quiet) { std::cerr << cl.toString(); }
    return true;
}

void ModeHandler::runConsole()
{
    console::Console console;
    std::string command;
    console.initialize();
    std::cerr << "Successfully started console mode" << std::endl;
    while (getline(std::cin, command)) {
        if (command == "quit") { break; }
        console.executeCommand(command);
    }
}

void ModeHandler::runSelfPlay()
{
    actor::ActorGroup ag;
    ag.run();
}

void ModeHandler::runZeroServer()
{
    zero::ZeroServer server;
    server.run();
}

void ModeHandler::runZeroTrainingName()
{
    std::cout << Environment().name()                                                           // name for environment
              << "_" << (config::actor_use_gumbel ? "g" : "") << config::nn_type_name[0] << "z" // network & training algorithm
              << "_" << config::nn_num_blocks << "b"                                            // number of blocks
              << "x" << config::nn_num_hidden_channels                                          // number of hidden channels
              << "_n" << config::actor_num_simulation                                           // number of simulations
              << "-" << GIT_SHORT_HASH << std::endl;                                            // git hash info
}

void ModeHandler::runEnvTest()
{
    Environment env;
    env.reset();
    while (!env.isTerminal()) {
        std::vector<Action> legal_actions = env.getLegalActions();
        int index = utils::Random::randInt() % legal_actions.size();
        env.act(legal_actions[index]);
    }
    std::cout << env.toString() << std::endl;

    EnvironmentLoader env_loader;
    env_loader.loadFromEnvironment(env);
    std::cout << env_loader.toString() << std::endl;
}

#include <termio.h>
int getch(void)
{
    struct termios tm, tm_old;
    int fd = 0, ch;
    
    if (tcgetattr(fd, &tm) < 0) {
        return -1;
    }
    
    tm_old = tm;
    cfmakeraw(&tm);
    if (tcsetattr(fd, TCSANOW, &tm) < 0) {
        return -1;
    }
    
    ch = getchar();
    
    if (tcsetattr(fd, TCSANOW, &tm_old) < 0) {
        return -1;
    }
    
    return ch;
}
void ModeHandler::runEnvTest2()
{
    Environment env;
    env.reset();
    std::cout << "Game started. Enter your moves or 'q' to quit." << std::endl;

    while (!env.isTerminal()) {
        system("clear");

        std::cout << env.toString() << std::endl;

        char ch = getch();

        Action action;
        switch(ch) {
            case 'w':
            case 'W':
                action = Action({"U"});
                break;
            case 'a':
            case 'A':
                action = Action({"L"});
                break;
            case 'd':
            case 'D':
                action = Action({"R"});
                break;
            case 's':
            case 'S':
                action = Action({"D"});
                break;
            case ' ':
                action = Action({"drop"});
                break;
            case 'q':
            case 'Q':
                std::cout << "Quitting the game." << std::endl;
                return;
            default:
                action = Action({"no_action"});
        }

        env.act(action);
    }

    std::cout << "Game over. Final state:" << std::endl;
    std::cout << env.toString() << std::endl;

    EnvironmentLoader env_loader;
    env_loader.loadFromEnvironment(env);
    std::cout << "Game record:" << std::endl;
    std::cout << env_loader.toString() << std::endl;
}

void ModeHandler::runEnvTest3()
{
    Environment env;
    std::string input;
    std::string filepath;
    std::cout << "Enter the SGF file path: ";
    std::getline(std::cin, filepath);
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return;
    }

    while(std::getline(file, input)) {
        EnvironmentLoader env_loader;
        env_loader.loadFromString(input);
        int seed = std::stoi(env_loader.getTag("SD"));
        env.reset(seed);
        auto actions = env_loader.getActionPairs();
        std::cout << "Initial state:" << std::endl;
        std::cout << env.toString() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Pause for 0.5 seconds at the start

        for (int i = 0; i < actions.size(); ++i) {
            env.act(actions[i].first);
            
            // Clear the console (this is platform-dependent)
            system("clear");

            std::cout << "Move " << i + 1 << "/" << actions.size() << std::endl;
            std::cout << "Action: " << actions[i].first.toConsoleString() << std::endl;
            std::cout << env.toString() << std::endl;
            std::cout << "Reward: " << env.getEvalScore() << std::endl;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Wait for 0.1 seconds
        }

        std::cout << "Game replay finished. Press Enter to continue or 'q' to quit." << std::endl;
        std::string response;
        std::getline(std::cin, response);
        if (response == "q" || response == "Q") {
            break;
        }
    }
}

void ModeHandler::runEvaluation()
{
    
    std::shared_ptr<minizero::network::Network> network_;
    std::shared_ptr<actor::BaseActor> actor_;
    std::shared_ptr<network::AlphaZeroNetwork> alphazero_network_;
    if (!network_) { network_ = createNetwork(config::nn_file_name, 0); }
    if (!actor_) {
        uint64_t tree_node_size = static_cast<uint64_t>(config::actor_num_simulation + 1) * network_->getActionSize();
        actor_ = actor::createActor(tree_node_size, network_);
    }
    actor_->setNetwork(network_);
    alphazero_network_ = std::static_pointer_cast<AlphaZeroNetwork>(network_);

    float total_return = 0;
    int n = 1;
    bool attack = 0;
    for (int iter = 0; iter < n; ++iter) {
        actor_->reset();
        while (!actor_->isEnvTerminal()) {
            const Action action = actor_->think(false, true);
            actor_->getEnvironment().act(action, !attack);
            if(actor_->isEnvTerminal()) {
                std::cout << actor_->getEnvironment().toString() << std::endl;
                break;
            }
            if(attack) {
                auto events = actor_->getEnvironment().getLegalChanceEvents();
                float mn = 1e9+7;
                auto worst_event = events[0];
                for (auto event : events) {
                    Environment env = actor_->getEnvironment();
                    env.actChanceEvent(event);
                    auto features = env.getFeatures();
                    auto nn_evaluation_batch_id_ = alphazero_network_->pushBack(features);
                    auto network_output = alphazero_network_->forward();
                    auto output = network_output[nn_evaluation_batch_id_];
                    std::shared_ptr<AlphaZeroNetworkOutput> alphazero_output = std::static_pointer_cast<AlphaZeroNetworkOutput>(output);
                    auto value = alphazero_output->value_;
                    if (value < mn) {
                        mn = value;
                        worst_event = event;
                    }
                }
                actor_->getEnvironment().actChanceEvent(worst_event);
            }
        }
        total_return += actor_->getEnvironment().getEvalScore();
    }
    std::cout << "Return: " << total_return / n << std::endl;
}

void ModeHandler::runRemoveObs()
{
    std::string obs_file_path;
    std::cin >> obs_file_path;

    minizero::env::atari::ObsRemover ob;
    ob.initialize();
    ob.run(obs_file_path);
}

void ModeHandler::runRecoverObs()
{
    std::string obs_file_path;
    std::cin >> obs_file_path;

#if ATARI
    minizero::env::atari::ObsRecover ob;
    ob.initialize();
    ob.run(obs_file_path);
#else
    std::cout << "Currently, only support recover observation for atari games" << std::endl;
#endif
}

} // namespace minizero::console

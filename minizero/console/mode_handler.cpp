#include "mode_handler.h"
#include "actor_group.h"
#include "console.h"
#include "git_info.h"
#include "random.h"
#include "zero_server.h"
#include <string>
#include <vector>

namespace minizero::console {

using namespace minizero::utils;

ModeHandler::ModeHandler()
{
    RegisterFunction("console", this, &ModeHandler::runConsole);
    RegisterFunction("sp", this, &ModeHandler::runSelfPlay);
    RegisterFunction("zero_server", this, &ModeHandler::runZeroServer);
    RegisterFunction("zero_training_name", this, &ModeHandler::runZeroTrainingName);
    RegisterFunction("env_test", this, &ModeHandler::runEnvTest);
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
    if (!config::program_quiet) { std::cerr << "Successfully started console mode" << std::endl; }
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

} // namespace minizero::console

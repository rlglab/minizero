#include "console.h"
#include "configuration.h"
#include "create_actor.h"
#include "create_network.h"
#include "sgf_loader.h"
#include <climits>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace minizero::console {

using namespace network;

Console::Console()
    : network_(nullptr),
      actor_(nullptr)
{
    RegisterFunction("list_commands", this, &Console::cmdListCommands);
    RegisterFunction("name", this, &Console::cmdName);
    RegisterFunction("version", this, &Console::cmdVersion);
    RegisterFunction("protocol_version", this, &Console::cmdProtocalVersion);
    RegisterFunction("clear_board", this, &Console::cmdClearBoard);
    RegisterFunction("showboard", this, &Console::cmdShowBoard);
    RegisterFunction("play", this, &Console::cmdPlay);
    RegisterFunction("boardsize", this, &Console::cmdBoardSize);
    RegisterFunction("genmove", this, &Console::cmdGenmove);
    RegisterFunction("reg_genmove", this, &Console::cmdGenmove);
    RegisterFunction("final_score", this, &Console::cmdFinalScore);
    RegisterFunction("pv", this, &Console::cmdPV);
    RegisterFunction("loadmodel", this, &Console::cmdLoadModel);
}

void Console::executeCommand(std::string command)
{
    if (!network_ || !actor_) { initialize(); }
    if (command.back() == '\r') { command.pop_back(); }
    if (command.empty()) { return; }

    // parse command to args
    std::stringstream ss(command);
    std::string tmp;
    std::vector<std::string> args;
    while (std::getline(ss, tmp, ' ')) { args.push_back(tmp); }

    // execute function
    if (function_map_.count(args[0]) == 0) { return reply(ConsoleResponse::kFail, "Unknown command: " + command); }
    (*function_map_[args[0]])(args);
}

void Console::initialize()
{
    network_ = createNetwork(config::nn_file_name, 0);
    long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * network_->getActionSize();
    actor_ = actor::createActor(tree_node_size, network_);
}

void Console::cmdLoadModel(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }
    minizero::config::nn_file_name = args[1];
    initialize();
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdPV(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    float output_value;
    std::vector<std::pair<std::string, float>> ouput_policy_vector;
    std::vector<float> output_policy;
    if (network_->getNetworkTypeName() == "alphazero") {
        std::shared_ptr<network::AlphaZeroNetwork> alphazero_network = std::static_pointer_cast<network::AlphaZeroNetwork>(network_);
        int index = alphazero_network->pushBack(actor_->getEnvironment().getFeatures());
        std::shared_ptr<NetworkOutput> network_output = alphazero_network->forward()[index];
        std::shared_ptr<minizero::network::AlphaZeroNetworkOutput> zero_output = std::static_pointer_cast<minizero::network::AlphaZeroNetworkOutput>(network_output);
        output_policy = zero_output->policy_;
        output_value = zero_output->value_;
    } else if (network_->getNetworkTypeName() == "muzero") {
        std::shared_ptr<network::MuZeroNetwork> muzero_network = std::static_pointer_cast<network::MuZeroNetwork>(network_);
        int index = muzero_network->pushBackInitialData(actor_->getEnvironment().getFeatures());
        std::shared_ptr<NetworkOutput> network_output = muzero_network->initialInference()[index];
        std::shared_ptr<minizero::network::MuZeroNetworkOutput> zero_output = std::static_pointer_cast<minizero::network::MuZeroNetworkOutput>(network_output);
        output_policy = zero_output->policy_;
        output_value = zero_output->value_;
    } else {
    }
    Environment env_transition = actor_->getEnvironment();
    for (size_t action_id = 0; action_id < output_policy.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        ouput_policy_vector.push_back(make_pair(action.toConsoleString(), output_policy[action_id]));
    }
    std::cerr << "[policy] ";
    std::sort(ouput_policy_vector.begin(), ouput_policy_vector.end(), [](const std::pair<std::string, float>& a, const std::pair<std::string, float>& b) { return (a.second > b.second); });
    for (long unsigned int i = 0; i < ouput_policy_vector.size(); i++) {
        std::cerr << ouput_policy_vector[i].first << ": " << std::fixed << std::setprecision(3) << ouput_policy_vector[i].second << " ";
    }
    std::cerr << std::endl
              << "[value]  " << output_value << std::endl;
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdListCommands(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    std::ostringstream oss;
    for (const auto& command : function_map_) { oss << command.first << std::endl; }
    reply(ConsoleResponse::kSuccess, oss.str());
}

void Console::cmdName(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    reply(ConsoleResponse::kSuccess, "minizero");
}

void Console::cmdVersion(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    reply(ConsoleResponse::kSuccess, "1.0");
}

void Console::cmdProtocalVersion(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    reply(ConsoleResponse::kSuccess, "2");
}

void Console::cmdClearBoard(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    actor_->reset();
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdShowBoard(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    reply(ConsoleResponse::kSuccess, "\n" + actor_->getEnvironment().toString());
}

void Console::cmdPlay(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, INT_MAX)) { return; }
    std::string action_string = args[2];
    std::vector<std::string> act_args;
    for (unsigned int i = 1; i < args.size(); i++) { act_args.push_back(args[i]); }
    if (!actor_->act(act_args)) { return reply(ConsoleResponse::kFail, "Invalid action: \"" + action_string + "\""); }
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdBoardSize(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }
#if GO
    minizero::config::env_go_board_size = stoi(args[1]);
#elif KILLALLGO
    minizero::config::env_go_board_size = stoi(args[1]);
#elif OTHELLO
    minizero::config::env_othello_board_size = stoi(args[1]);
#else
#endif
    initialize();
    reply(ConsoleResponse::kSuccess, "\n" + actor_->getEnvironment().toString());
}

void Console::cmdGenmove(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }
    if (actor_->isEnvTerminal()) {
        reply(ConsoleResponse::kSuccess, "PASS");
        return;
    }
    const Action action = actor_->think((args[0] == "genmove" ? true : false), true);
    if (actor_->isResign()) {
        reply(ConsoleResponse::kSuccess, "Resign");
        return;
    }
    reply(ConsoleResponse::kSuccess, action.toConsoleString());
}

void Console::cmdFinalScore(const std::vector<std::string>& args)
{
    reply(ConsoleResponse::kSuccess, std::to_string(actor_->getEvalScore()));
}

bool Console::checkArgument(const std::vector<std::string>& args, int min_argc, int max_argc)
{
    int size = args.size();
    if (size >= min_argc && size <= max_argc) { return true; }

    std::ostringstream oss;
    oss << "command requires ";
    if (min_argc == max_argc) {
        oss << "exactly " << min_argc << " argument" << (min_argc == 1 ? "" : "s");
    } else {
        oss << min_argc << " to " << max_argc << " arguments";
    }

    reply(ConsoleResponse::kFail, oss.str());
    return false;
}

void Console::reply(ConsoleResponse response, const std::string& reply)
{
    std::cout << static_cast<char>(response) << " " << reply << "\n\n";
}

} // namespace minizero::console
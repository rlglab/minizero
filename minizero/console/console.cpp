#include "console.h"
#include "configuration.h"
#include "create_actor.h"
#include "create_network.h"
#include "sgf_loader.h"
#include "time_system.h"
#include <algorithm>
#include <climits>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace minizero::console {

using namespace network;

Console::Console()
    : network_(nullptr),
      actor_(nullptr)
{
    RegisterFunction("gogui-analyze_commands", this, &Console::cmdGoguiAnalyzeCommands);
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
    RegisterFunction("load_model", this, &Console::cmdLoadModel);
}

void Console::initialize()
{
    if (!network_) { network_ = createNetwork(config::nn_file_name, 0); }
    if (!actor_) {
        uint64_t tree_node_size = static_cast<uint64_t>(config::actor_num_simulation + 1) * network_->getActionSize();
        actor_ = actor::createActor(tree_node_size, network_);
    }
    actor_->setNetwork(network_);

    // forward the network several times to warmup since the first few forwards requires some initialization time
    const int num_warmup_forward = 3;
    if (network_->getNetworkTypeName() == "alphazero") {
        std::shared_ptr<network::AlphaZeroNetwork> alphazero_network = std::static_pointer_cast<network::AlphaZeroNetwork>(network_);
        for (int i = 0; i < num_warmup_forward; ++i) {
            for (int j = 0; j < config::actor_mcts_think_batch_size; ++j) { alphazero_network->pushBack(actor_->getEnvironment().getFeatures()); }
            alphazero_network->forward();
        }
    } else if (network_->getNetworkTypeName() == "muzero") {
        std::shared_ptr<network::MuZeroNetwork> muzero_network = std::static_pointer_cast<network::MuZeroNetwork>(network_);
        for (int i = 0; i < num_warmup_forward; ++i) {
            for (int j = 0; j < config::actor_mcts_think_batch_size; ++j) { muzero_network->pushBackInitialData(actor_->getEnvironment().getFeatures()); }
            muzero_network->initialInference();
        }
    } else {
        assert(false); // should not be here
    }
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

void Console::cmdGoguiAnalyzeCommands(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    std::string registered_cmd = "sboard/policy_value/pv\n";
    reply(console::ConsoleResponse::kSuccess, registered_cmd);
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
    minizero::config::env_board_size = stoi(args[1]);
    initialize();
    reply(ConsoleResponse::kSuccess, "\n" + actor_->getEnvironment().toString());
}

void Console::cmdGenmove(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }

    if (actor_->isEnvTerminal()) { return reply(ConsoleResponse::kSuccess, "PASS"); }
    actor_->getEnvironment().setTurn(minizero::env::charToPlayer(args[1].c_str()[0]));
    boost::posix_time::ptime start_ptime = utils::TimeSystem::getLocalTime();
    const Action action = actor_->think((args[0] == "genmove" ? true : false), true);
    std::cerr << "Spent Time = " << (utils::TimeSystem::getLocalTime() - start_ptime).total_milliseconds() / 1000.0f << " (s)" << std::endl;
    if (actor_->isResign()) { return reply(ConsoleResponse::kSuccess, "Resign"); }

    reply(ConsoleResponse::kSuccess, action.toConsoleString());
}

void Console::cmdFinalScore(const std::vector<std::string>& args)
{
    reply(ConsoleResponse::kSuccess, std::to_string(actor_->getEvalScore()));
}

void Console::cmdPV(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }

    float value;
    std::vector<float> policy;
    utils::Rotation rotation = config::actor_use_random_rotation_features ? static_cast<utils::Rotation>(utils::Random::randInt() % static_cast<int>(utils::Rotation::kRotateSize)) : utils::Rotation::kRotationNone;
    calculatePolicyValue(policy, value, rotation);

    const Environment& env_transition = actor_->getEnvironment();
    std::vector<std::pair<std::string, float>> sorted_policy;
    for (size_t action_id = 0; action_id < policy.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        sorted_policy.push_back(make_pair(action.toConsoleString(), policy[action_id]));
    }

    std::ostringstream oss;
    std::sort(sorted_policy.begin(), sorted_policy.end(), [](const std::pair<std::string, float>& a, const std::pair<std::string, float>& b) { return (a.second > b.second); });
    oss << "[rotation] " << utils::getRotationString(rotation) << std::endl;
    oss << "[policy] ";
    for (size_t i = 0; i < sorted_policy.size(); i++) { oss << sorted_policy[i].first << ": " << std::fixed << std::setprecision(3) << sorted_policy[i].second << " "; }
    oss << std::endl;
    oss << "[value] " << value << std::endl;
    std::cerr << oss.str() << std::endl;

    // for GUI
    oss.str("");
    oss.clear();
    int board_size = minizero::config::env_board_size;
    oss << std::endl;
    for (int row = board_size - 1; row >= 0; row--) {
        for (int col = 0; col < board_size; col++) {
            int action_id = row * board_size + col;
            oss << (env_transition.isLegalAction(Action(action_id, env_transition.getTurn())) ? std::to_string(policy[action_id] * 100).substr(0, 4) + "%" : "\"\"") << " ";
        }
        oss << std::endl;
    }

    reply(ConsoleResponse::kSuccess, oss.str());
}

void Console::cmdLoadModel(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }
    minizero::config::nn_file_name = args[1];
    network_ = nullptr;
    initialize();
    reply(ConsoleResponse::kSuccess, "");
}

void Console::calculatePolicyValue(std::vector<float>& policy, float& value, utils::Rotation rotation /* = utils::Rotation::kRotationNone */)
{
    if (network_->getNetworkTypeName() == "alphazero") {
        std::shared_ptr<network::AlphaZeroNetwork> alphazero_network = std::static_pointer_cast<network::AlphaZeroNetwork>(network_);
        int index = alphazero_network->pushBack(actor_->getEnvironment().getFeatures(rotation), rotation);
        std::shared_ptr<NetworkOutput> network_output = alphazero_network->forward()[index];
        std::shared_ptr<minizero::network::AlphaZeroNetworkOutput> zero_output = std::static_pointer_cast<minizero::network::AlphaZeroNetworkOutput>(network_output);
        policy = zero_output->policy_;
        value = zero_output->value_;
    } else if (network_->getNetworkTypeName() == "muzero") {
        std::shared_ptr<network::MuZeroNetwork> muzero_network = std::static_pointer_cast<network::MuZeroNetwork>(network_);
        int index = muzero_network->pushBackInitialData(actor_->getEnvironment().getFeatures());
        std::shared_ptr<NetworkOutput> network_output = muzero_network->initialInference()[index];
        std::shared_ptr<minizero::network::MuZeroNetworkOutput> zero_output = std::static_pointer_cast<minizero::network::MuZeroNetworkOutput>(network_output);
        policy = zero_output->policy_;
        value = zero_output->value_;
    } else {
        assert(false); // should not be here
    }
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

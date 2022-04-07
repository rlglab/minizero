#include "console.h"
#include "actor.h"
#include "alphazero_network.h"
#include "muzero_network.h"
#include "sgf_loader.h"
#include <climits>
#include <iostream>
#include <sstream>

namespace minizero::console {

Console::Console()
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
    RegisterFunction("reg_genmove", this, &Console::cmdRegGenmove);

    initialize();
}

void Console::executeCommand(std::string command)
{
    if (command.back() == '\r') { command.pop_back(); }
    if (command.empty()) { return; }

    // parse command to args
    std::stringstream ss(command);
    std::string tmp, first_command;
    std::getline(ss, first_command, ' ');
    std::vector<std::string> args;
    while (std::getline(ss, tmp, ' ')) { args.push_back(tmp); }

    // execute function
    if (function_map_.count(first_command) == 0) { return reply(ConsoleResponse::kFail, "Unknown command: " + command); }
    (*function_map_[first_command])(args);
}

void Console::initialize()
{
    network_ = network::createNetwork(config::nn_file_name, 0);
    long long tree_node_size = static_cast<long long>(config::actor_num_simulation) * network_->getActionSize();
    actor_ = actor::createActor(tree_node_size, network_->getNetworkTypeName());
    actor_->reset();
}

void Console::cmdListCommands(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 0, 0)) { return; }
    std::ostringstream oss;
    for (const auto& command : function_map_) { oss << command.first << std::endl; }
    reply(ConsoleResponse::kSuccess, oss.str());
}

void Console::cmdName(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 0, 0)) { return; }
    reply(ConsoleResponse::kSuccess, "minizero");
}

void Console::cmdVersion(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 0, 0)) { return; }
    reply(ConsoleResponse::kSuccess, "1.0");
}

void Console::cmdProtocalVersion(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 0, 0)) { return; }
    reply(ConsoleResponse::kSuccess, "2");
}

void Console::cmdClearBoard(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 0, 0)) { return; }
    actor_->reset();
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdShowBoard(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 0, 0)) { return; }
    reply(ConsoleResponse::kSuccess, "\n" + actor_->getEnvironment().toString());
}

void Console::cmdPlay(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, INT_MAX)) { return; }
    std::string action_string = args[1];
    if (!actor_->act(args)) { return reply(ConsoleResponse::kFail, "Invalid action: \"" + action_string + "\""); }
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdBoardSize(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    reply(ConsoleResponse::kSuccess, "");
}

void Console::cmdGenmove(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    const Action action = actor_->think(network_, true, true);
    reply(ConsoleResponse::kSuccess, action.toConsoleString());
}

void Console::cmdRegGenmove(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    const Action action = actor_->think(network_, false, true);
    reply(ConsoleResponse::kSuccess, action.toConsoleString());
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
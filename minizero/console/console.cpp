#include "console.h"
#include <climits>
#include <iostream>
#include <sstream>

namespace minizero::console {

Console::Console()
{
    RegisterFunction("list_commands", this, &Console::CmdListCommands);
    RegisterFunction("name", this, &Console::CmdName);
    RegisterFunction("version", this, &Console::CmdVersion);
    RegisterFunction("protocol_version", this, &Console::CmdProtocalVersion);
    RegisterFunction("clear_board", this, &Console::CmdClearBoard);
    RegisterFunction("showboard", this, &Console::CmdShowBoard);
    RegisterFunction("play", this, &Console::CmdPlay);
}

void Console::ExecuteCommand(std::string command)
{
    if (command.empty()) { return; }

    // parse command to args
    std::stringstream ss(command);
    std::string tmp, first_command;
    std::getline(ss, first_command, ' ');
    std::vector<std::string> args;
    while (std::getline(ss, tmp, ' ')) { args.push_back(tmp); }

    // execute function
    if (function_map_.count(first_command) == 0) { return Reply(ConsoleResponse::kFail, "Unknown command: " + command); }
    (*function_map_[first_command])(args);
}

void Console::CmdListCommands(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 0, 0)) { return; }
    std::ostringstream oss;
    for (const auto& command : function_map_) { oss << command.first << std::endl; }
    Reply(ConsoleResponse::kSuccess, oss.str());
}

void Console::CmdName(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 0, 0)) { return; }
    Reply(ConsoleResponse::kSuccess, "minizero");
}

void Console::CmdVersion(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 0, 0)) { return; }
    Reply(ConsoleResponse::kSuccess, "1.0");
}

void Console::CmdProtocalVersion(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 0, 0)) { return; }
    Reply(ConsoleResponse::kSuccess, "2");
}

void Console::CmdClearBoard(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 0, 0)) { return; }
    env_.Reset();
    Reply(ConsoleResponse::kSuccess, "");
}

void Console::CmdShowBoard(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 0, 0)) { return; }
    Reply(ConsoleResponse::kSuccess, "\n" + env_.ToString());
}

void Console::CmdPlay(const std::vector<std::string>& args)
{
    if (!CheckArgument(args, 1, INT_MAX)) { return; }
    std::string action_string = args[1];
    if (!env_.Act(args)) { return Reply(ConsoleResponse::kFail, "Invalid action: \"" + action_string + "\""); }
    Reply(ConsoleResponse::kSuccess, "");
}

bool Console::CheckArgument(const std::vector<std::string>& args, int min_argc, int max_argc)
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

    Reply(ConsoleResponse::kFail, oss.str());
    return false;
}

void Console::Reply(ConsoleResponse response, const std::string& reply)
{
    std::cout << static_cast<char>(response) << " " << reply << "\n\n";
}

} // namespace minizero::console
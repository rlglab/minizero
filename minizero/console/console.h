#pragma once

#include "environment.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace minizero::console {

enum class ConsoleResponse : char {
    kFail = '?',
    kSuccess = '='
};

class Console {
public:
    Console();
    virtual ~Console() = default;

    void ExecuteCommand(std::string command);

protected:
    class BaseFunction {
    public:
        virtual ~BaseFunction() = default;
        virtual void operator()(const std::vector<std::string>& args) = 0;
    };

    template <class I, class F>
    class Function : public BaseFunction {
    public:
        Function(I* instance, F function) : instance_(instance), function_(function) {}
        void operator()(const std::vector<std::string>& args) { (*instance_.*function_)(args); }

        I* instance_;
        F function_;
    };

    template <class I, class F>
    void RegisterFunction(const std::string& name, I* instance, F function)
    {
        function_map_.insert(std::make_pair(name, std::make_shared<Function<I, F>>(instance, function)));
    }

    void CmdListCommands(const std::vector<std::string>& args);
    void CmdName(const std::vector<std::string>& args);
    void CmdVersion(const std::vector<std::string>& args);
    void CmdProtocalVersion(const std::vector<std::string>& args);
    void CmdClearBoard(const std::vector<std::string>& args);
    void CmdShowBoard(const std::vector<std::string>& args);
    void CmdPlay(const std::vector<std::string>& args);

    bool CheckArgument(const std::vector<std::string>& args, int min_argc, int max_argc);
    void Reply(ConsoleResponse response, const std::string& reply);

    Environment env_;
    std::map<std::string, std::shared_ptr<BaseFunction>> function_map_;
};

} // namespace minizero::console

#pragma once

#include "base_actor.h"
#include "network.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace minizero::console {

using namespace minizero;

enum class ConsoleResponse : char {
    kFail = '?',
    kSuccess = '='
};

class Console {
public:
    Console();
    virtual ~Console() = default;

    virtual void initialize();
    virtual void executeCommand(std::string command);

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
        function_map_[name] = std::make_shared<Function<I, F>>(instance, function);
    }

    void cmdGoguiAnalyzeCommands(const std::vector<std::string>& args);
    void cmdListCommands(const std::vector<std::string>& args);
    void cmdName(const std::vector<std::string>& args);
    void cmdVersion(const std::vector<std::string>& args);
    void cmdProtocalVersion(const std::vector<std::string>& args);
    void cmdClearBoard(const std::vector<std::string>& args);
    void cmdShowBoard(const std::vector<std::string>& args);
    void cmdPlay(const std::vector<std::string>& args);
    void cmdBoardSize(const std::vector<std::string>& args);
    void cmdGenmove(const std::vector<std::string>& args);
    void cmdFinalScore(const std::vector<std::string>& args);
    void cmdPV(const std::vector<std::string>& args);
    void cmdLoadModel(const std::vector<std::string>& args);

    virtual void calculatePolicyValue(std::vector<float>& policy, float& value, utils::Rotation rotation = utils::Rotation::kRotationNone);
    bool checkArgument(const std::vector<std::string>& args, int min_argc, int max_argc);
    void reply(ConsoleResponse response, const std::string& reply);

    std::shared_ptr<minizero::network::Network> network_;
    std::shared_ptr<actor::BaseActor> actor_;
    std::map<std::string, std::shared_ptr<BaseFunction>> function_map_;
};

} // namespace minizero::console

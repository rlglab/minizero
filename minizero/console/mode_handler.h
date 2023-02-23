#pragma once

#include "configuration.h"
#include <map>
#include <memory>
#include <string>

namespace minizero::console {

class ModeHandler {
public:
    ModeHandler();
    virtual ~ModeHandler() = default;

    void run(int argc, char* argv[]);

protected:
    class BaseFunction {
    public:
        virtual ~BaseFunction() = default;
        virtual void operator()() = 0;
    };

    template <class I, class F>
    class Function : public BaseFunction {
    public:
        Function(I* instance, F function) : instance_(instance), function_(function) {}
        void operator()() { (*instance_.*function_)(); }

        I* instance_;
        F function_;
    };

    template <class I, class F>
    void RegisterFunction(const std::string& name, I* instance, F function)
    {
        function_map_[name] = std::make_shared<Function<I, F>>(instance, function);
    }

    void usage();
    std::string getAllModesString();
    virtual void setDefaultConfiguration(config::ConfigureLoader& cl) { config::setConfiguration(cl); }
    void genConfiguration(config::ConfigureLoader& cl, const std::string& sConfigFile);
    bool readConfiguration(config::ConfigureLoader& cl, const std::string& sConfigFile, const std::string& sConfigString);
    virtual void runConsole();
    virtual void runSelfPlay();
    virtual void runZeroServer();
    virtual void runZeroTrainingName();
    virtual void runEnvTest();

    std::map<std::string, std::shared_ptr<BaseFunction>> function_map_;
};

} // namespace minizero::console

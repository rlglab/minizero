#include "actor_group.h"
#include "configuration.h"
#include "console.h"
#include "environment.h"
#include "zero_server.h"
#include <torch/script.h>
#include <vector>

using namespace std;

void usage()
{
    cout << "./minizero [arguments]" << endl;
    cout << "arguments:" << endl;
    cout << "\t-mode [gtp|sp]" << endl;
    cout << "\t-gen configuration_file" << endl;
    cout << "\t-conf_file configuration_file" << endl;
    cout << "\t-conf_str configuration_string" << endl;
}

void GenConfiguration(minizero::config::ConfigureLoader& cl, string sConfigFile)
{
    // check configure file is exist
    ifstream f(sConfigFile);
    if (f.good()) {
        char ans = ' ';
        while (ans != 'y' && ans != 'n') {
            cerr << sConfigFile << " already exist, do you want to overwrite it? [y/n]" << endl;
            cin >> ans;
        }
        if (ans == 'y') { cerr << "overwrite " << sConfigFile << endl; }
        if (ans == 'n') {
            cerr << "didn't overwrite " << sConfigFile << endl;
            f.close();
            return;
        }
    }
    f.close();

    ofstream fout(sConfigFile);
    fout << cl.ToString();
    fout.close();
}

bool readConfiguration(minizero::config::ConfigureLoader& cl, string sConfigFile, string sConfigString)
{
    if (!sConfigFile.empty() && !cl.LoadFromFile(sConfigFile)) {
        cerr << "Failed to load configuration file." << endl;
        return false;
    }
    if (!sConfigString.empty() && !cl.LoadFromString(sConfigString)) {
        cerr << "Failed to load configuration string." << endl;
        return false;
    }

    cerr << cl.ToString();
    return true;
}

void Console()
{
    minizero::console::Console console;
    string command;
    while (getline(cin, command)) {
        if (command == "quit") { break; }
        console.ExecuteCommand(command);
    }
}

void SelfPlay()
{
    minizero::actor::ActorGroup ag;
    ag.Run();
}

void ZeroServer()
{
    minizero::server::ZeroServer server;
    server.Run();
}

void Test()
{
    Environment env;
    env.Reset();
    while (!env.IsTerminal()) {
        vector<Action> legal_actions = env.GetLegalActions();
        int index = rand() % legal_actions.size();
        env.Act(legal_actions[index]);
    }
    cout << env.ToString() << endl;

    EnvironmentLoader env_loader;
    env_loader.LoadFromEnvironment(env);
    cout << env_loader.ToString() << endl;
}

int main(int argc, char* argv[])
{
    if (argc % 2 != 1) {
        usage();
        return -1;
    }

    minizero::env::SetUpEnv();

    string sMode = "console";
    string sConfigFile = "";
    string sConfigString = "";
    minizero::config::ConfigureLoader cl;
    minizero::config::SetConfiguration(cl);

    for (int i = 1; i < argc; i += 2) {
        string sCommand = string(argv[i]);

        if (sCommand == "-mode") {
            sMode = argv[i + 1];
        } else if (sCommand == "-gen") {
            GenConfiguration(cl, argv[i + 1]);
            return 0;
        } else if (sCommand == "-conf_file") {
            sConfigFile = argv[i + 1];
        } else if (sCommand == "-conf_str") {
            sConfigString = argv[i + 1];
        } else {
            cerr << "unknown argument: " << sCommand << endl;
            usage();
            return -1;
        }
    }

    if (!readConfiguration(cl, sConfigFile, sConfigString)) { return -1; }

    if (sMode == "console") {
        Console();
    } else if (sMode == "sp") {
        SelfPlay();
    } else if (sMode == "zero_server") {
        ZeroServer();
    } else if (sMode == "test") {
        Test();
    } else {
        cerr << "Error mode: " << sMode << endl;
        return -1;
    }

    return 0;
}

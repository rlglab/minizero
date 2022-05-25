#include "configuration.h"
#include "console.h"
#include "environment.h"
#include "killallgo_actor_group.h"
#include "killallgo_configuration.h"
#include "killallgo_mcts_solver.h"
#include "zero_server.h"
#include <torch/script.h>
#include <vector>

using namespace std;
using namespace minizero;
using namespace solver;

void usage()
{
    cout << "./solver [arguments]" << endl;
    cout << "arguments:" << endl;
    cout << "\t-mode [gtp|sp]" << endl;
    cout << "\t-gen configuration_file" << endl;
    cout << "\t-conf_file configuration_file" << endl;
    cout << "\t-conf_str configuration_string" << endl;
}

void genConfiguration(config::ConfigureLoader& cl, string sConfigFile)
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
    fout << cl.toString();
    fout.close();
}

bool readConfiguration(config::ConfigureLoader& cl, string sConfigFile, string sConfigString)
{
    if (!sConfigFile.empty() && !cl.loadFromFile(sConfigFile)) {
        cerr << "Failed to load configuration file." << endl;
        return false;
    }
    if (!sConfigString.empty() && !cl.loadFromString(sConfigString)) {
        cerr << "Failed to load configuration string." << endl;
        return false;
    }

    cerr << cl.toString();
    return true;
}

void runConsole()
{
    console::Console console;
    string command;
    while (getline(cin, command)) {
        if (command == "quit") { break; }
        console.executeCommand(command);
    }
}

void runSelfPlay()
{
    KillallGoActorGroup ag;
    ag.run();
}

void runZeroServer()
{
    minizero::server::ZeroServer server;
    server.run();
}

void runSolver()
{
    /*int gpu_id = 0;
    string nn_file_name = "";
    std::shared_ptr<solver::ProofCostNetwork> network = std::make_shared<solver::ProofCostNetwork>();
    network->loadModel(nn_file_name, gpu_id);
    long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * network->getActionSize();

    solver::KillAllGoMCTSSolver solver(tree_node_size);
    solver.setNetwork(network);
    // vector<int> pos = {21, 49, 48, 14, 22, 15, 23, 8, 16, 9, 10, 2, 3}; // sim=2
    // vector<int> pos = {21, 49, 22, 14, 23, 15, 16, 8, 17, 9, 18, 10, 11}; // sim=76
    vector<int> pos = {21, 49, 22, 14, 23, 15, 16, 8, 17, 9, 18, 10}; // sim=94276
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 6, 9, 5, 12, 13, 26,9, 8}; // test rzone
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 5, 4, 12, 13, 26, 9, 8, 11}; // test capture
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 5, 4, 12, 13, 26, 9, 8, 11, 20}; // test capture2
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 5, 4, 12, 13, 26, 9, 8, 11, 20, 27, 34}; // test eat ko
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 5, 4, 12, 13, 26, 9, 8, 11, 20, 27, 34, 38, 33, 40, 39, 46}; // test eat ko2
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 5, 4, 12, 13, 26, 9, 8, 11, 20, 27, 34, 38, 33, 40, 39, 46, 15, 41, 7, 24}; // test eat ko3
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25}; // sim=?, piggy_back_before
    // vector<int> pos = {31, 49, 17, 10, 16, 18, 25, 19, 26}; // sim=792444(rzone)
    // vector<int> pos = {31, 49, 39, 24, 25, 26, 27, 17, 18, 19, 20, 10, 11, 12, 34, 13, 33, 3, 40, 4, 41, 5,23}; // test bug

    std::string str = "(;FF[4]CA[UTF-8]AP[GoGui:1.5.1]SZ[7]KM[6.5]DT[2022-05-17];B[dc];W[];B[de];W[df];B[ce];W[ee];B[ed])";
    minizero::utils::SGFLoader sgf_loader;
    sgf_loader.loadFromString(str);
    const std::vector<std::pair<std::vector<std::string>, std::string>>& actions = sgf_loader.getActions();
    const Environment& env = solver.getMCTS().getEnvironment();
    if (sgf_loader.getTags().count("SZ") == 0) { return; }
    for (auto act : actions) {
        std::cout << act.first << endl;
        Action action(act.first, stoi(sgf_loader.getTags().at("SZ")));
        solver.act(action);
    }

    // for (auto id : pos) {
    //     Action action(id, env.getTurn());
    //     solver.act(action);
    // }
    cout << env.toString() << endl;
    // cout << env.name() << endl;
    solver::SolveResult result = solver.solve();
    cout << solver.getMCTS().getRootNode()->getCount() << endl;
    cout << static_cast<int>(result) << endl;

    // EnvironmentLoader env_loader;
    // env_loader.loadFromEnvironment(env);
    // cout << solver.getTree().toString(env_loader.toString()) << endl;

    ostringstream oss;
    oss << "(;FF[4]CA[UTF-8]AP[GoGui:1.5.1]SZ[7]";
    const std::vector<Action>& action_list = env.getActionHistory();
    for (unsigned int i = 0; i < action_list.size(); ++i) {
        Action action = action_list[i];
        string sgf_string = minizero::utils::SGFLoader::actionIDToSGFString(action.getActionID(), 7);
        oss << ";";
        oss << playerToChar(action.getPlayer());
        oss << "[" << sgf_string << "]";
    }
    oss << ")";

    std::fstream f;
    f.open("tree.sgf", ios::out);
    f << solver.getMCTS().getTreeString(oss.str()) << endl;
    f.close();*/
}

int main(int argc, char* argv[])
{
    if (argc % 2 != 1) {
        usage();
        return -1;
    }

    env::setUpEnv();

    string sMode = "console";
    string sConfigFile = "";
    string sConfigString = "";
    config::ConfigureLoader cl;
    solver::setConfiguration(cl);

    for (int i = 1; i < argc; i += 2) {
        string sCommand = string(argv[i]);

        if (sCommand == "-mode") {
            sMode = argv[i + 1];
        } else if (sCommand == "-gen") {
            genConfiguration(cl, argv[i + 1]);
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
        runConsole();
    } else if (sMode == "sp") {
        runSelfPlay();
    } else if (sMode == "zero_server") {
        runZeroServer();
    } else if (sMode == "solver") {
        runSolver();
    } else {
        cerr << "Error mode: " << sMode << endl;
        return -1;
    }

    return 0;
}

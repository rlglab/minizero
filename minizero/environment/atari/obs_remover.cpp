#include "obs_remover.h"

namespace minizero::env::atari {

std::string ObsRemoverThreadSharedData::getAvailableSgfPath()
{
    std::lock_guard lock(mutex_);

    if (all_sgf_path_it_ == all_sgf_path_.end()) { return ""; }

    std::string sgf_path = *all_sgf_path_it_;
    all_sgf_path_it_++;

    return sgf_path;
}

void ObsRemoverSlaveThread::runJob()
{
    while (removeSingleObs()) {}
}

bool ObsRemoverSlaveThread::removeSingleObs()
{
    const std::string file_path = getSharedData()->getAvailableSgfPath();
    if (file_path.empty()) { return false; }

    std::cout << "Removing obs: " << file_path << std::endl;
    std::string remove_obs_file_path = file_path.substr(0, file_path.size() - std::string(".sgf").length()) + "_remove_obs.sgf";
    std::ifstream original_file(file_path);
    std::ofstream processed_file(remove_obs_file_path);

    if (!original_file.is_open()) {
        std::cerr << "Cannot open " << file_path << "!" << std::endl;
        exit(-1);
    }

    if (!processed_file.is_open()) {
        std::cerr << "Cannot create " << remove_obs_file_path << "!" << std::endl;
        exit(-1);
    }

    std::string line;
    while (getline(original_file, line)) {
        size_t start = line.find("OBS[");
        size_t end = line.find("]", start);

        if (start == std::string::npos || end == std::string::npos) {
            std::cerr << "Wrong file format in " << file_path << std::endl;
            exit(-1);
        }

        line.replace(start, end - start + 1, "OBS[]");
        processed_file << line << std::endl;
    }

    original_file.close();
    processed_file.close();

    return true;
}

bool ObsRemover::isProperGame(const std::string& path)
{
    std::ifstream file(path);
    std::string sgf;
    getline(file, sgf);

    AtariEnvLoader env_loader;
    env_loader.loadFromString(sgf);
    std::string game = env_loader.getTag("GM");

    return game.find("atari") != std::string::npos; // currently, only support for atari games
}

void ObsRemover::run(std::string& path)
{
    if (path.substr(path.size() - std::string(".sgf").length()) == ".sgf") {
        // path is a sgf file
        getSharedData()->all_sgf_path_.push_back(path);
    } else {
        // path is a directory
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_regular_file()) { continue; }
            const std::string path = entry.path().string();
            if (path.find(".sgf") == std::string::npos) { continue; } // not a sgf file
            getSharedData()->all_sgf_path_.push_back(path);
        }
    }

    const std::string& file_path = getSharedData()->all_sgf_path_[0];
    if (!isProperGame(file_path)) {
        std::cerr << "Currently, only support recover observation for atari games" << std::endl;
        exit(-1);
    }

    getSharedData()->all_sgf_path_it_ = getSharedData()->all_sgf_path_.begin();
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }
}

void ObsRemover::initialize()
{
    std::cout << "Using " << config::zero_num_threads << " threads to remove obs" << std::endl;
    createSlaveThreads(config::zero_num_threads);
}

} // namespace minizero::env::atari

#include "obs_recover.h"

namespace minizero::env::atari {

EnvInfo* ObsRecoverThreadSharedData::getAvailableEnvInfoPtr()
{
    std::lock_guard lock(mutex_);

    if (seed_env_info_it_ == seed_env_info_.end()) { return nullptr; }

    if (env_info_it_ == seed_env_info_it_->second.end()) {
        seed_env_info_it_++;
        if (seed_env_info_it_ == seed_env_info_.end()) { return nullptr; }
        env_info_it_ = seed_env_info_it_->second.begin();
    }

    EnvInfo* env_info_ptr = *env_info_it_;
    env_info_it_++;

    return env_info_ptr;
}

int ObsRecoverThreadSharedData::getEnvIndex(const AtariEnvLoader& env_loader)
{
    // where this env is in the vector
    const int seed = std::stoi(env_loader.getTag("SD"));
    if (seed_env_info_[seed].empty()) { return getInitEnvInfoPtrIndex(seed); }

    const auto& action_pairs = env_loader.getActionPairs();
    const std::vector<EnvInfo*>& env_info_ptrs = seed_env_info_[seed];
    for (int i = 0; i < static_cast<int>(env_info_ptrs.size()); i++) {
        const auto& actions = env_info_ptrs[i]->env_.getActionHistory();
        if (action_pairs.size() <= actions.size()) { continue; } // action_pairs should be longer than action history in env, otherwise, this action_pairs is from a new env
        bool is_same = true;
        for (int pos = 0; pos < static_cast<int>(actions.size()); pos++) {
            if (action_pairs[pos].first.getActionID() != actions[pos].getActionID()) {
                is_same = false;
                break;
            }
        }
        if (is_same) { return i; }
    }
    return getInitEnvInfoPtrIndex(seed);
}

EnvInfo* ObsRecoverThreadSharedData::getInitEnvInfoPtr(const int seed)
{
    EnvInfo* env_info_ptr = new EnvInfo;
    env_info_ptr->seed_ = seed;
    env_info_ptr->env_.reset(seed);
    return env_info_ptr;
}

int ObsRecoverThreadSharedData::getInitEnvInfoPtrIndex(const int seed)
{
    EnvInfo* env_info_ptr = getInitEnvInfoPtr(seed);
    std::lock_guard lock(mutex_);
    seed_env_info_[seed].push_back(env_info_ptr);
    return seed_env_info_[seed].size() - 1;
}

void ObsRecoverThreadSharedData::setSgf(std::string file_path)
{
    resetMember();
    std::cout << "Recovering obs: " << file_path << std::endl;

    size_t pos = file_path.find("_remove_obs.sgf"); // find the position of the first '_'
    std::string recover_obs_file_path = file_path.substr(0, pos) + ".sgf";
    if (original_file_.is_open()) { original_file_.close(); }
    if (processed_file_.is_open()) { processed_file_.close(); }
    original_file_.open(file_path);
    processed_file_.open(recover_obs_file_path);

    std::string sgf;
    while (getline(original_file_, sgf)) { sgfs_.push_back(sgf); }
}

void ObsRecoverThreadSharedData::addEnvInfo(AtariEnvLoader env_loader, int sgf_id)
{
    const int seed = std::stoi(env_loader.getTag("SD"));
    const int env_index = getEnvIndex(env_loader);
    EnvInfo* env_info_ptr = seed_env_info_[seed][env_index];

    env_info_ptr->sgf_ids_.push_back(sgf_id);
    env_info_ptr->end_positions_.push_back(env_loader.getActionPairs().size());

    std::lock_guard lock(mutex_);
    if (env_loader.getActionPairs().size() > env_info_ptr->env_loader_.getActionPairs().size()) { env_info_ptr->env_loader_ = env_loader; }
}

void ObsRecoverThreadSharedData::addEnvInfoToRemove(EnvInfo* env_info_ptr)
{
    // handle env end
    std::lock_guard lock(mutex_);
    env_info_to_remove_.push_back(env_info_ptr);
}

void ObsRecoverThreadSharedData::resetMember()
{
    sgfs_.clear();
    env_info_to_remove_.clear();

    // remove all sgf_ids_, end_positions_ in the previous sgf file
    for (auto& pair : seed_env_info_) {
        std::vector<EnvInfo*>& env_info_ptrs = pair.second;
        for (EnvInfo* env_info_ptr : env_info_ptrs) {
            env_info_ptr->sgf_ids_.clear();
            env_info_ptr->end_positions_.clear();
            env_info_ptr->env_loader_.reset();
        }
    }
}

std::pair<std::string, int> ObsRecoverThreadSharedData::getNextEnvPair()
{
    std::lock_guard lock(mutex_);
    if (sgfs_it_ == sgfs_.end()) { return std::make_pair("", -1); }

    std::string env_string = *sgfs_it_;
    int sgf_id = sgfs_it_ - sgfs_.begin();
    sgfs_it_++;
    return std::make_pair(env_string, sgf_id);
}

void ObsRecoverSlaveThread::runJob()
{
    if (getSharedData()->sgfs_it_ != getSharedData()->sgfs_.end()) {
        while (addEnvironmentLoader()) {}
    } else {
        recover();
    }
}

bool ObsRecoverSlaveThread::addEnvironmentLoader()
{
    std::string env_string;
    int sgf_id;
    std::tie(env_string, sgf_id) = getSharedData()->getNextEnvPair();

    if (sgf_id == -1) { return false; }

    AtariEnvLoader env_loader;
    if (env_loader.loadFromString(env_string)) { getSharedData()->addEnvInfo(env_loader, sgf_id); }

    return true;
}

void ObsRecoverSlaveThread::handleEndPosition(std::string& sgf, EnvInfo* env_info_ptr)
{
    size_t begin = sgf.find("OBS[");
    size_t end = sgf.find("]", begin);

    std::string observation;
    for (const auto& obs : env_info_ptr->env_.getObservationHistory()) { observation += obs; }
    sgf.replace(begin, end - begin + 1, "OBS[" + utils::compressString(observation) + "]");

    if (sgf.find("#") != std::string::npos) { getSharedData()->addEnvInfoToRemove(env_info_ptr); }
}

void ObsRecoverSlaveThread::recover()
{
    EnvInfo* env_info_ptr = getSharedData()->getAvailableEnvInfoPtr();
    while (env_info_ptr) {
        std::sort(env_info_ptr->sgf_ids_.begin(), env_info_ptr->sgf_ids_.end());
        std::sort(env_info_ptr->end_positions_.begin(), env_info_ptr->end_positions_.end());

        const auto& action_pairs = env_info_ptr->env_loader_.getActionPairs();
        size_t pos = env_info_ptr->env_.getActionHistory().size();
        for (size_t i = 0; i < env_info_ptr->end_positions_.size(); i++) {
            for (; pos < env_info_ptr->end_positions_[i]; pos++) { env_info_ptr->env_.act(action_pairs[pos].first); }
            handleEndPosition(getSharedData()->sgfs_[env_info_ptr->sgf_ids_[i]], env_info_ptr);
        }

        env_info_ptr = getSharedData()->getAvailableEnvInfoPtr();
    }
}

void ObsRecover::removeEnvInfo(EnvInfo* env_info_ptr)
{
    const int seed = env_info_ptr->seed_;
    std::vector<EnvInfo*>& env_info_ptrs = getSharedData()->seed_env_info_[seed];
    const size_t env_index = std::find(env_info_ptrs.begin(), env_info_ptrs.end(), env_info_ptr) - env_info_ptrs.begin();

    delete env_info_ptrs[env_index];
    env_info_ptrs.erase(env_info_ptrs.begin() + env_index);

    if (env_info_ptrs.empty()) { getSharedData()->seed_env_info_.erase(seed); }
}

std::vector<std::string> ObsRecover::getAllSgfPath(const std::string& dir_path)
{
    std::vector<std::string> all_sgf_path;

    for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) { continue; }
        std::string path = entry.path().string();
        size_t pos = path.find("_remove_obs.sgf");
        if (pos == std::string::npos) { continue; } // not a {id}_remove_obs.sgf format
        all_sgf_path.push_back(path);
    }

    std::sort(all_sgf_path.begin(), all_sgf_path.end(), [](const std::string& a, const std::string& b) {
        auto it_a = std::find_if(a.begin() + a.find("/sgf"), a.end(), [](char c) { // find the first number from "/sgf"
            return std::isdigit(c);
        });
        auto it_b = std::find_if(b.begin() + b.find("/sgf"), b.end(), [](char c) { // find the first number from "/sgf"
            return std::isdigit(c);
        });

        const int pos_a = std::distance(a.begin(), it_a);
        const int pos_b = std::distance(b.begin(), it_b);

        int id_a = std::stoi(a.substr(pos_a, a.find("_remove_obs.sgf")));
        int id_b = std::stoi(b.substr(pos_b, b.find("_remove_obs.sgf")));
        return id_a < id_b;
    });

    return all_sgf_path;
}

void ObsRecover::runSingleSgf(const std::string& path)
{
    getSharedData()->setSgf(path);
    getSharedData()->sgfs_it_ = getSharedData()->sgfs_.begin();

    // run addEnvironmentLoader
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }

    getSharedData()->seed_env_info_it_ = getSharedData()->seed_env_info_.begin();
    std::vector<EnvInfo*>& env_info_ptrs = getSharedData()->seed_env_info_it_->second;
    getSharedData()->env_info_it_ = env_info_ptrs.begin();

    // run recover
    for (auto& t : slave_threads_) { t->start(); }
    for (auto& t : slave_threads_) { t->finish(); }

    for (std::string& sgf : getSharedData()->sgfs_) { getSharedData()->processed_file_ << sgf << std::endl; }

    for (EnvInfo*& env_info : getSharedData()->env_info_to_remove_) { removeEnvInfo(env_info); }
}

void ObsRecover::run(std::string& obs_file_path)
{
    if (obs_file_path.substr(obs_file_path.size() - std::string(".sgf").size()) != ".sgf") {
        // obs_file_path is a directory
        std::vector<std::string> all_sgf_path = getAllSgfPath(obs_file_path);
        for (const std::string& sgf_path : all_sgf_path) { runSingleSgf(sgf_path); }
    } else {
        if (obs_file_path.find("_remove_obs.sgf") == std::string::npos) {
            std::cerr << "Wrong file name" << std::endl;
            std::cerr << "Basic format: 1_remove_obs.sgf, 2_remove_obs.sgf, ..." << std::endl;
            exit(-1);
        }
        runSingleSgf(obs_file_path);
    }
}

void ObsRecover::initialize()
{
    std::cout << "Using " << config::zero_num_threads << " threads to recover obs" << std::endl;
    createSlaveThreads(config::zero_num_threads);
}

} // namespace minizero::env::atari

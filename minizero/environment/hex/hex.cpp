#include "hex.h"

namespace minizero::env::hex {

HexAction::HexAction(const std::vector<std::string>& action_string_args)
{
    // TODO
}

void HexEnv::reset()
{
    // TODO
    turn_ = Player::kPlayer1;
    actions_.clear();
}

bool HexEnv::act(const HexAction& action)
{
    // TODO
    return true;
}

bool HexEnv::act(const std::vector<std::string>& action_string_args)
{
    return act(HexAction(action_string_args));
}

std::vector<HexAction> HexEnv::getLegalActions() const
{
    // TODO
    return {};
}

bool HexEnv::isLegalAction(const HexAction& action) const
{
    // TODO
    return true;
}

bool HexEnv::isTerminal() const
{
    // TODO
    return false;
}

float HexEnv::getEvalScore(bool is_resign /* = false */) const
{
    // TODO
    return 0.0f;
}

std::vector<float> HexEnv::getFeatures(utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    // TODO
    return {};
}

std::vector<float> HexEnv::getActionFeatures(const HexAction& action, utils::Rotation rotation /* = utils::Rotation::kRotationNone */) const
{
    // TODO
    return {};
}

std::string HexEnv::toString() const
{
    // TODO
    return "";
}

} // namespace minizero::env::hex
import torch
import torch.nn as nn
import torch.nn.functional as F
from .network_unit import ResidualBlock, PolicyNetwork, ValueNetwork, DiscreteValueNetwork


class AlphaZeroNetwork(nn.Module):
    def __init__(self,
                 game_name,
                 num_input_channels,
                 input_channel_height,
                 input_channel_width,
                 num_hidden_channels,
                 hidden_channel_height,
                 hidden_channel_width,
                 num_blocks,
                 action_size,
                 num_value_hidden_channels,
                 discrete_value_size):
        super(AlphaZeroNetwork, self).__init__()
        self.game_name = game_name
        self.num_input_channels = num_input_channels
        self.input_channel_height = input_channel_height
        self.input_channel_width = input_channel_width
        self.num_hidden_channels = num_hidden_channels
        self.hidden_channel_height = hidden_channel_height
        self.hidden_channel_width = hidden_channel_width
        self.num_blocks = num_blocks
        self.action_size = action_size
        self.num_value_hidden_channels = num_value_hidden_channels
        self.discrete_value_size = discrete_value_size

        self.conv = nn.Conv2d(num_input_channels, num_hidden_channels, kernel_size=3, padding=1)
        self.bn = nn.BatchNorm2d(num_hidden_channels)
        self.residual_blocks = nn.ModuleList([ResidualBlock(num_hidden_channels) for _ in range(num_blocks)])
        self.policy = PolicyNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, action_size)
        if self.discrete_value_size == 1:
            self.value = ValueNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, num_value_hidden_channels)
        else:
            self.value = DiscreteValueNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, num_value_hidden_channels, discrete_value_size)

    @torch.jit.export
    def get_type_name(self):
        return "alphazero"

    @torch.jit.export
    def get_game_name(self):
        return self.game_name

    @torch.jit.export
    def get_num_input_channels(self):
        return self.num_input_channels

    @torch.jit.export
    def get_input_channel_height(self):
        return self.input_channel_height

    @torch.jit.export
    def get_input_channel_width(self):
        return self.input_channel_width

    @torch.jit.export
    def get_num_hidden_channels(self):
        return self.num_hidden_channels

    @torch.jit.export
    def get_hidden_channel_height(self):
        return self.hidden_channel_height

    @torch.jit.export
    def get_hidden_channel_width(self):
        return self.hidden_channel_width

    @torch.jit.export
    def get_num_blocks(self):
        return self.num_blocks

    @torch.jit.export
    def get_action_size(self):
        return self.action_size

    @torch.jit.export
    def get_num_value_hidden_channels(self):
        return self.num_value_hidden_channels

    @torch.jit.export
    def get_discrete_value_size(self):
        return self.discrete_value_size

    def forward(self, state):
        x = self.conv(state)
        x = self.bn(x)
        x = F.relu(x)
        for residual_block in self.residual_blocks:
            x = residual_block(x)

        # policy
        policy_logit = self.policy(x)
        policy = torch.softmax(policy_logit, dim=1)

        # value
        if self.discrete_value_size == 1:
            value = self.value(x)
            return {"policy_logit": policy_logit,
                    "policy": policy,
                    "value": value}
        else:
            value_logit = self.value(x)
            value = torch.softmax(value_logit, dim=1)
            return {"policy_logit": policy_logit,
                    "policy": policy,
                    "value_logit": value_logit,
                    "value": value}

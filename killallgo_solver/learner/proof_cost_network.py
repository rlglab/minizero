import torch
import torch.nn as nn
import torch.nn.functional as F


class ResidualBlock(nn.Module):
    def __init__(self, num_channels):
        super(ResidualBlock, self).__init__()
        self.conv1 = nn.Conv2d(num_channels, num_channels, 3, padding=1)
        self.bn1 = nn.BatchNorm2d(num_channels)
        self.conv2 = nn.Conv2d(num_channels, num_channels, 3, padding=1)
        self.bn2 = nn.BatchNorm2d(num_channels)

    def forward(self, x):
        input = x

        x = self.conv1(x)
        x = self.bn1(x)
        x = F.relu(x)
        x = self.conv2(x)
        x = self.bn2(x)
        x = F.relu(input + x)
        return x


class PolicyNetwork(nn.Module):
    def __init__(self, num_channels, channel_height, channel_width, num_output_channels, action_size):
        super(PolicyNetwork, self).__init__()
        self.channel_height = channel_height
        self.channel_width = channel_width
        self.num_output_channels = num_output_channels
        self.conv = nn.Conv2d(num_channels, num_output_channels, 1)
        self.bn = nn.BatchNorm2d(num_output_channels)
        self.fc = nn.Linear(num_output_channels*channel_height*channel_width, action_size)

    def forward(self, x):
        x = self.conv(x)
        x = self.bn(x)
        x = F.relu(x)
        x = x.view(-1, self.num_output_channels*self.channel_height*self.channel_width)
        x = self.fc(x)
        return x


class CostNetwork(nn.Module):
    def __init__(self, num_channels, channel_height, channel_width, num_output_channels, value_size):
        super(CostNetwork, self).__init__()

        self.channel_height = channel_height
        self.channel_width = channel_width
        self.num_output_channels = num_output_channels
        self.conv = nn.Conv2d(num_channels, num_output_channels, 1)
        self.bn = nn.BatchNorm2d(num_output_channels)
        self.fc = nn.Linear(num_output_channels*channel_height*channel_width, value_size)

    def forward(self, x):
        x = self.conv(x)
        x = self.bn(x)
        x = F.relu(x)
        x = x.view(-1, self.num_output_channels*self.channel_height*self.channel_width)
        x = self.fc(x)
        return x


class ProofCostNetwork(nn.Module):
    def __init__(self,
                 game_name,
                 num_input_channels,
                 input_channel_height,
                 input_channel_width,
                 num_hidden_channels,
                 hidden_channel_height,
                 hidden_channel_width,
                 num_blocks,
                 num_action_channels,
                 action_size,
                 num_value_hidden_channels,
                 value_size):
        super(ProofCostNetwork, self).__init__()
        self.game_name = game_name
        self.num_input_channels = num_input_channels
        self.input_channel_height = input_channel_height
        self.input_channel_width = input_channel_width
        self.num_hidden_channels = num_hidden_channels
        self.hidden_channel_height = hidden_channel_height
        self.hidden_channel_width = hidden_channel_width
        self.num_blocks = num_blocks
        self.num_action_channels = num_action_channels
        self.action_size = action_size
        self.num_value_hidden_channels = num_value_hidden_channels
        self.value_size = value_size

        self.conv = nn.Conv2d(num_input_channels, num_hidden_channels, 3, padding=1)
        self.bn = nn.BatchNorm2d(num_hidden_channels)
        self.residual_blocks = nn.ModuleList([ResidualBlock(num_hidden_channels) for _ in range(num_blocks)])
        self.policy = PolicyNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, num_action_channels, action_size)
        self.value_n = CostNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, num_value_hidden_channels, value_size)
        self.value_m = CostNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, num_value_hidden_channels, value_size)

    @torch.jit.export
    def get_type_name(self):
        return "pcn"

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
    def get_num_action_channels(self):
        return self.num_action_channels

    @torch.jit.export
    def get_action_size(self):
        return self.action_size

    @torch.jit.export
    def get_num_value_hidden_channels(self):
        return self.num_value_hidden_channels

    @torch.jit.export
    def get_value_size(self):
        return self.value_size

    def forward(self, state):
        x = self.conv(state)
        x = self.bn(x)
        x = F.relu(x)
        for residual_block in self.residual_blocks:
            x = residual_block(x)

        policy = self.policy(x)
        value_n = self.value_n(x)
        value_m = self.value_m(x)
        return {"policy": policy, "value_n": value_n, "value_m": value_m}


def create_proof_cost_network(game_name="killallgo",
                              num_input_channels=18,
                              input_channel_height=7,
                              input_channel_width=7,
                              num_hidden_channels=256,
                              hidden_channel_height=7,
                              hidden_channel_width=7,
                              num_blocks=3,
                              num_action_channels=2,
                              action_size=50,
                              num_value_hidden_channels=256,
                              value_size=200):
    network = ProofCostNetwork(game_name,
                               num_input_channels,
                               input_channel_height,
                               input_channel_width,
                               num_hidden_channels,
                               hidden_channel_height,
                               hidden_channel_width,
                               num_blocks,
                               num_action_channels,
                               action_size,
                               num_value_hidden_channels,
                               value_size)
    return network

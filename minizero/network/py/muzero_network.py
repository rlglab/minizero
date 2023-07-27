import torch
import torch.nn as nn
import torch.nn.functional as F
from .network_unit import ResidualBlock, PolicyNetwork, ValueNetwork


class MuZeroRepresentationNetwork(nn.Module):
    def __init__(self, num_input_channels, num_output_channels, num_blocks):
        super(MuZeroRepresentationNetwork, self).__init__()
        self.conv = nn.Conv2d(num_input_channels, num_output_channels, kernel_size=3, padding=1)
        self.bn = nn.BatchNorm2d(num_output_channels)
        self.residual_blocks = nn.ModuleList([ResidualBlock(num_output_channels) for _ in range(num_blocks)])

    def forward(self, state):
        x = self.conv(state)
        x = self.bn(x)
        x = F.relu(x)
        for residual_block in self.residual_blocks:
            x = residual_block(x)
        return x


class MuZeroDynamicsNetwork(nn.Module):
    def __init__(self, num_channels, num_action_feature_channels, num_blocks):
        super(MuZeroDynamicsNetwork, self).__init__()
        self.conv = nn.Conv2d(num_channels + num_action_feature_channels, num_channels, kernel_size=3, padding=1)
        self.bn = nn.BatchNorm2d(num_channels)
        self.residual_blocks = nn.ModuleList([ResidualBlock(num_channels) for _ in range(num_blocks)])

    def forward(self, hidden_state, action_plane):
        x = torch.cat((hidden_state, action_plane), dim=1)
        x = self.conv(x)
        x = self.bn(x)
        x = F.relu(x)
        for residual_block in self.residual_blocks:
            x = residual_block(x)
        return x


class MuZeroPredictionNetwork(nn.Module):
    def __init__(self, num_channels, channel_height, channel_width, action_size, num_value_hidden_channels):
        super(MuZeroPredictionNetwork, self).__init__()
        self.policy = PolicyNetwork(num_channels, channel_height, channel_width, action_size)
        self.value = ValueNetwork(num_channels, channel_height, channel_width, num_value_hidden_channels)

    def forward(self, hidden_state):
        policy_logit = self.policy(hidden_state)
        value = self.value(hidden_state)
        return policy_logit, value


class MuZeroNetwork(nn.Module):
    def __init__(self,
                 game_name,
                 num_input_channels,
                 input_channel_height,
                 input_channel_width,
                 num_hidden_channels,
                 hidden_channel_height,
                 hidden_channel_width,
                 num_action_feature_channels,
                 num_blocks,
                 action_size,
                 num_value_hidden_channels,
                 discrete_value_size):
        super(MuZeroNetwork, self).__init__()
        self.game_name = game_name
        self.num_input_channels = num_input_channels
        self.input_channel_height = input_channel_height
        self.input_channel_width = input_channel_width
        self.num_hidden_channels = num_hidden_channels
        self.hidden_channel_height = hidden_channel_height
        self.hidden_channel_width = hidden_channel_width
        self.num_action_feature_channels = num_action_feature_channels
        self.num_blocks = num_blocks
        self.action_size = action_size
        self.num_value_hidden_channels = num_value_hidden_channels
        self.discrete_value_size = discrete_value_size

        self.representation_network = MuZeroRepresentationNetwork(num_input_channels, num_hidden_channels, num_blocks)
        self.dynamics_network = MuZeroDynamicsNetwork(num_hidden_channels, num_action_feature_channels, num_blocks)
        self.prediction_network = MuZeroPredictionNetwork(num_hidden_channels, hidden_channel_height, hidden_channel_width, action_size, num_value_hidden_channels)

    @torch.jit.export
    def get_type_name(self):
        return "muzero"

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
    def get_num_action_feature_channels(self):
        return self.num_action_feature_channels

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

    @torch.jit.export
    def initial_inference(self, state):
        # representation + prediction
        hidden_state = self.representation_network(state)
        hidden_state = self.scale_hidden_state(hidden_state)
        policy_logit, value = self.prediction_network(hidden_state)
        policy = torch.softmax(policy_logit, dim=1)
        return {"policy_logit": policy_logit, "policy": policy, "value": value, "hidden_state": hidden_state}

    @torch.jit.export
    def recurrent_inference(self, hidden_state, action_plane):
        # dynamics + prediction
        next_hidden_state = self.dynamics_network(hidden_state, action_plane)
        next_hidden_state = self.scale_hidden_state(next_hidden_state)
        policy_logit, value = self.prediction_network(next_hidden_state)
        policy = torch.softmax(policy_logit, dim=1)
        return {"policy_logit": policy_logit, "policy": policy, "value": value, "hidden_state": next_hidden_state}

    def scale_hidden_state(self, hidden_state):
        # scale hidden state to range [0, 1] for each feature plane
        batch_size, channel, w, h = hidden_state.shape
        hidden_state = hidden_state.view(batch_size, -1)
        min_val = hidden_state.min(-1, keepdim=True).values
        max_val = hidden_state.max(-1, keepdim=True).values
        scale = (max_val - min_val)
        scale[scale < 1e-5] += 1e-5
        hidden_state = (hidden_state - min_val) / scale
        hidden_state = hidden_state.view(batch_size, channel, w, h)
        return hidden_state

    def forward(self, state, action_plane=torch.empty(0)):
        if action_plane.numel() == 0:
            return self.initial_inference(state)
        else:
            return self.recurrent_inference(state, action_plane)

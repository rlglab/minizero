import math
import torch.nn as nn
import torch.nn.functional as F


class ResidualBlock(nn.Module):
    def __init__(self, num_channels):
        super(ResidualBlock, self).__init__()
        self.conv1 = nn.Conv2d(num_channels, num_channels, kernel_size=3, padding=1)
        self.bn1 = nn.BatchNorm2d(num_channels)
        self.conv2 = nn.Conv2d(num_channels, num_channels, kernel_size=3, padding=1)
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
    def __init__(self, num_channels, channel_height, channel_width, action_size):
        super(PolicyNetwork, self).__init__()
        self.channel_height = channel_height
        self.channel_width = channel_width
        self.num_output_channels = math.ceil(action_size / (channel_height * channel_width))
        self.conv = nn.Conv2d(num_channels, self.num_output_channels, kernel_size=1)
        self.bn = nn.BatchNorm2d(self.num_output_channels)
        self.fc = nn.Linear(self.num_output_channels * channel_height * channel_width, action_size)

    def forward(self, x):
        x = self.conv(x)
        x = self.bn(x)
        x = F.relu(x)
        x = x.view(-1, self.num_output_channels * self.channel_height * self.channel_width)
        x = self.fc(x)
        return x


class ValueNetwork(nn.Module):
    def __init__(self, num_channels, channel_height, channel_width, num_value_hidden_channels):
        super(ValueNetwork, self).__init__()
        self.channel_height = channel_height
        self.channel_width = channel_width
        self.conv = nn.Conv2d(num_channels, 1, kernel_size=1)
        self.bn = nn.BatchNorm2d(1)
        self.fc1 = nn.Linear(channel_height * channel_width, num_value_hidden_channels)
        self.fc2 = nn.Linear(num_value_hidden_channels, 1)
        self.tanh = nn.Tanh()

    def forward(self, x):
        x = self.conv(x)
        x = self.bn(x)
        x = F.relu(x)
        x = x.view(-1, self.channel_height * self.channel_width)
        x = self.fc1(x)
        x = F.relu(x)
        x = self.fc2(x)
        x = self.tanh(x)
        return x


class DiscreteValueNetwork(nn.Module):
    def __init__(self, num_channels, channel_height, channel_width, num_value_hidden_channels, value_size):
        super(DiscreteValueNetwork, self).__init__()
        self.channel_height = channel_height
        self.channel_width = channel_width
        self.hidden_channels = math.ceil(value_size / (channel_height * channel_width))
        self.conv = nn.Conv2d(num_channels, self.hidden_channels, kernel_size=1)
        self.bn = nn.BatchNorm2d(self.hidden_channels)
        self.fc1 = nn.Linear(channel_height * channel_width * self.hidden_channels, num_value_hidden_channels)
        self.fc2 = nn.Linear(num_value_hidden_channels, value_size)

    def forward(self, x):
        x = self.conv(x)
        x = self.bn(x)
        x = F.relu(x)
        x = x.view(-1, self.channel_height * self.channel_width * self.hidden_channels)
        x = self.fc1(x)
        x = F.relu(x)
        x = self.fc2(x)
        return x

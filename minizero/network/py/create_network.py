from .alphazero_network import AlphaZeroNetwork
from .muzero_network import MuZeroNetwork
from .muzero_atari_network import MuZeroAtariNetwork


def create_network(game_name="tietactoe",
                   num_input_channels=4,
                   input_channel_height=3,
                   input_channel_width=3,
                   num_hidden_channels=16,
                   hidden_channel_height=3,
                   hidden_channel_width=3,
                   num_action_feature_channels=1,
                   num_blocks=1,
                   num_action_channels=1,
                   action_size=9,
                   num_value_hidden_channels=256,
                   discrete_value_size=601,
                   network_type_name="alphazero"):

    network = None
    if network_type_name == "alphazero":
        network = AlphaZeroNetwork(game_name,
                                   num_input_channels,
                                   input_channel_height,
                                   input_channel_width,
                                   num_hidden_channels,
                                   hidden_channel_height,
                                   hidden_channel_width,
                                   num_blocks,
                                   num_action_channels,
                                   action_size,
                                   num_value_hidden_channels)
    elif network_type_name == "muzero":
        if game_name == "atari":
            network = MuZeroAtariNetwork(game_name,
                                         num_input_channels,
                                         input_channel_height,
                                         input_channel_width,
                                         num_hidden_channels,
                                         hidden_channel_height,
                                         hidden_channel_width,
                                         num_action_feature_channels,
                                         num_blocks,
                                         num_action_channels,
                                         action_size,
                                         num_value_hidden_channels,
                                         discrete_value_size)
        else:
            network = MuZeroNetwork(game_name,
                                    num_input_channels,
                                    input_channel_height,
                                    input_channel_width,
                                    num_hidden_channels,
                                    hidden_channel_height,
                                    hidden_channel_width,
                                    num_action_feature_channels,
                                    num_blocks,
                                    num_action_channels,
                                    action_size,
                                    num_value_hidden_channels)
    else:
        assert False

    return network

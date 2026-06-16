#!/usr/bin/env python3

import argparse
import filecmp
import inspect
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

torch = None
create_network = None


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


def import_pybind(game_type):
    module = __import__(f"build.{game_type}", globals(), locals(), ["minizero_py"], 0)
    return module.minizero_py


def default_conf_file(training_dir):
    cfg_files = sorted(training_dir.glob("*.cfg"))
    if len(cfg_files) != 1:
        raise RuntimeError(
            f"Please specify --conf. Found {len(cfg_files)} cfg files in {training_dir}."
        )
    return cfg_files[0]


def load_checkpoint(path):
    kwargs = {"map_location": torch.device("cpu")}
    if "weights_only" in inspect.signature(torch.load).parameters:
        kwargs["weights_only"] = False
    return torch.load(path, **kwargs)


def atomic_save(obj, path, legacy_pickle=False):
    tmp_path = path.with_suffix(path.suffix + ".tmp")
    torch.save(obj, tmp_path, _use_new_zipfile_serialization=not legacy_pickle)
    tmp_path.replace(path)


def copy_conf_file(conf_file, output_dir):
    dst_conf = output_dir / f"{output_dir.name}.cfg"
    if conf_file.resolve() != dst_conf.resolve():
        shutil.copy2(conf_file, dst_conf)

    stale_conf = output_dir / conf_file.name
    if stale_conf != dst_conf and stale_conf.exists() and filecmp.cmp(conf_file, stale_conf, shallow=False):
        stale_conf.unlink()

    return dst_conf


def copy_training_data(training_dir, output_dir):
    src_sgf_dir = training_dir / "sgf"
    if src_sgf_dir.is_dir():
        shutil.copytree(src_sgf_dir, output_dir / "sgf", dirs_exist_ok=True)

    for log_name in ("op.log", "Training.log", "Worker.log"):
        src_log = training_dir / log_name
        if src_log.exists():
            shutil.copy2(src_log, output_dir / log_name)


def create_configured_network(py):
    network = create_network(
        py.get_game_name(),
        py.get_nn_num_input_channels(),
        py.get_nn_input_channel_height(),
        py.get_nn_input_channel_width(),
        py.get_nn_num_hidden_channels(),
        py.get_nn_hidden_channel_height(),
        py.get_nn_hidden_channel_width(),
        py.get_nn_num_action_feature_channels(),
        py.get_nn_num_blocks(),
        py.get_nn_action_size(),
        py.get_nn_num_value_hidden_channels(),
        py.get_nn_discrete_value_size(),
        py.get_nn_type_name(),
    )
    network.eval()
    return network


def convert_one(pkl_path, src_model_dir, dst_model_dir, py, legacy_pickle=False):
    snapshot = load_checkpoint(pkl_path)
    if "network" not in snapshot:
        raise RuntimeError(f"{pkl_path} does not contain a 'network' state_dict.")

    dst_pkl = dst_model_dir / pkl_path.name
    atomic_save(snapshot, dst_pkl, legacy_pickle)

    network = create_configured_network(py)
    network.load_state_dict(snapshot["network"])
    scripted = torch.jit.script(network)

    dst_pt = dst_model_dir / pkl_path.with_suffix(".pt").name
    tmp_pt = dst_pt.with_suffix(dst_pt.suffix + ".tmp")
    scripted.save(str(tmp_pt))
    tmp_pt.replace(dst_pt)

    src_pt = src_model_dir / pkl_path.with_suffix(".pt").name
    return dst_pkl, dst_pt, src_pt.exists()


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Re-serialize MiniZero weight_iter_*.pkl checkpoints and regenerate "
            "matching TorchScript .pt files with the currently installed PyTorch."
        )
    )
    parser.add_argument("training_dir", type=Path, help="Folder like baseline_gaz_1")
    parser.add_argument(
        "-g",
        "--game-type",
        required=True,
        help="Build/game directory used by train.py, e.g. go, atari, tictactoe",
    )
    parser.add_argument(
        "-c",
        "--conf",
        type=Path,
        help="Config file. Defaults to the only *.cfg under training_dir.",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        help="Destination training folder. Defaults to <training_dir>_torch<version>.",
    )
    parser.add_argument(
        "--in-place",
        action="store_true",
        help="Overwrite files under training_dir/model after creating *.bak files.",
    )
    parser.add_argument(
        "--pattern",
        default="weight_iter_*.pkl",
        help="Checkpoint glob under model/. Default: weight_iter_*.pkl",
    )
    parser.add_argument(
        "--legacy-pickle",
        action="store_true",
        help="Save .pkl with PyTorch's legacy serialization format.",
    )
    parser.add_argument(
        "--no-copy-data",
        action="store_true",
        help="Do not copy sgf/ and logs when writing to a new output folder.",
    )
    return parser.parse_args()


def main():
    global create_network, torch

    args = parse_args()
    import torch as torch_module
    from minizero.network.py.create_network import create_network as create_network_func

    torch = torch_module
    create_network = create_network_func

    training_dir = args.training_dir.resolve()
    src_model_dir = training_dir / "model"
    if not src_model_dir.is_dir():
        raise RuntimeError(f"Model directory not found: {src_model_dir}")

    conf_file = (args.conf.resolve() if args.conf else default_conf_file(training_dir))
    if args.output_dir and args.in_place:
        raise RuntimeError("Use either --output-dir or --in-place, not both.")

    torch_version_tag = torch.__version__.replace("+", "_").replace("/", "_")
    output_dir = (
        training_dir
        if args.in_place
        else (args.output_dir.resolve() if args.output_dir else training_dir.with_name(f"{training_dir.name}_torch{torch_version_tag}"))
    )
    dst_model_dir = output_dir / "model"

    if output_dir != training_dir:
        output_dir.mkdir(parents=True, exist_ok=True)
        dst_model_dir.mkdir(parents=True, exist_ok=True)
        copy_conf_file(conf_file, output_dir)
        if not args.no_copy_data:
            copy_training_data(training_dir, output_dir)
    else:
        dst_model_dir.mkdir(parents=True, exist_ok=True)

    py = import_pybind(args.game_type)
    if not py.load_config_file(str(conf_file)):
        raise RuntimeError(f"Failed to load config file: {conf_file}")

    pkl_files = sorted(src_model_dir.glob(args.pattern))
    if not pkl_files:
        raise RuntimeError(f"No checkpoints matched {src_model_dir / args.pattern}")

    eprint(f"PyTorch: {torch.__version__}")
    eprint(f"Config: {conf_file}")
    eprint(f"Source model dir: {src_model_dir}")
    eprint(f"Destination model dir: {dst_model_dir}")

    converted = 0
    for pkl_path in pkl_files:
        if args.in_place:
            for path in (pkl_path, src_model_dir / pkl_path.with_suffix(".pt").name):
                if path.exists():
                    backup = path.with_suffix(path.suffix + ".bak")
                    if not backup.exists():
                        shutil.copy2(path, backup)

        dst_pkl, dst_pt, had_src_pt = convert_one(
            pkl_path, src_model_dir, dst_model_dir, py, args.legacy_pickle
        )
        converted += 1
        missing_note = "" if had_src_pt else " (source .pt was missing; regenerated)"
        eprint(f"Converted {pkl_path.name} -> {dst_pkl.name}, {dst_pt.name}{missing_note}")

    eprint(f"Done. Converted {converted} checkpoint(s).")


if __name__ == "__main__":
    try:
        main()
    except Exception as err:
        eprint(f"error: {err}")
        sys.exit(1)

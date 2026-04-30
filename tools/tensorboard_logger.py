#!/usr/bin/env python

import sys
import os
import re
import json
from torch.utils.tensorboard import SummaryWriter


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs, flush=True)


class TensorBoardLogger:
    def __init__(self, training_dir):
        self.writer = SummaryWriter(os.path.join(training_dir, "tensorboard"))
        self.training_dir = training_dir

    def log_training_info(self, training_info, global_step, display_step):
        grouped, singles = self._collect_grouped_training_metrics(training_info, display_step)

        for key, value in singles.items():
            self.writer.add_scalar(f"train/{key}", value, global_step)

        for group_name, curves in grouped.items():
            if curves:
                self.writer.add_scalars(f"train/{group_name}", curves, global_step)

    def log_selfplay_metrics(self, iteration):
        metrics_path = os.path.join(self.training_dir, "tensorboard", "selfplay_metrics.jsonl")
        if not os.path.exists(metrics_path):
            return

        target = None
        with open(metrics_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue

                try:
                    record = json.loads(line)
                except json.JSONDecodeError:
                    continue

                if record.get("iteration") == iteration:
                    target = record

        if target is None:
            eprint(f"[Warning] selfplay metrics not found for iteration {iteration}")
            return

        tb_step = target.get("iteration", iteration)
        target = dict(target)
        target.pop("iteration", None)

        self._write_selfplay_dict("selfplay", target, tb_step)

    def flush(self):
        self.writer.flush()

    def close(self):
        self.writer.close()

    def _collect_grouped_training_metrics(self, training_info, display_step):
        grouped = {}
        singles = {}

        for key, value in training_info.items():
            avg_value = value / display_step
            m = re.match(r"^(.*)_(\d+)$", key)
            if m:
                group_name = m.group(1)
                step_idx = m.group(2)
                grouped.setdefault(group_name, {})[step_idx] = avg_value
            else:
                singles[key] = avg_value

        return grouped, singles

    def _write_selfplay_dict(self, prefix, data, step):
        grouped_stat_keys = {"min", "max", "avg", "std"}

        scalar_items = {}
        dict_items = {}

        for key, value in data.items():
            if isinstance(value, dict):
                dict_items[key] = value
            elif isinstance(value, (int, float)):
                scalar_items[key] = value

        if scalar_items and set(scalar_items.keys()).issubset(grouped_stat_keys):
            self.writer.add_scalars(prefix, scalar_items, step)
            return

        for key, value in scalar_items.items():
            tag = f"{prefix}/{key}" if prefix else key
            self.writer.add_scalar(tag, value, step)

        for key, value in dict_items.items():
            tag = f"{prefix}/{key}" if prefix else key
            self._write_selfplay_dict(tag, value, step)

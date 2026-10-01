#!/usr/bin/env python3

import sys
from pathlib import Path
import yaml

def targets_yaml(file_path: Path, target: str, target_args: list = []):
    with open(file_path, "r", encoding="utf-8") as f:
        runners = yaml.safe_load(f)

    if target not in runners["runners"]:
        runners["runners"].append(target)

    runners["flash-runner"] = target
    runners["debug-runner"] = target

    if target not in runners["args"]:
        runners["args"][target] = []

    for arg in target_args:
        if arg not in runners["args"][target]:
            runners["args"][target].append(arg)

    with open(file_path, "w", encoding="utf-8") as f:
        yaml.dump(runners, f, sort_keys=False)

if __name__ == "__main__":
    target_file = Path(sys.argv[1])
    target_runner = sys.argv[2]
    target_runner_args = sys.argv[3:]

    targets_yaml(target_file, target_runner, target_runner_args)
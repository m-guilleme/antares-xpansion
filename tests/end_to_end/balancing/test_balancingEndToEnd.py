import shutil
import subprocess
from itertools import zip_longest
from pathlib import Path
from typing import List

import pytest

from tests.end_to_end.utils_functions import get_conf

from .studies import study_parameters, study_values

# Output file produced by the run -> reference file stored with the study
OUTPUT_TO_REF = {
    "final_capacities.csv": "final_capacities_ref.csv",  # cluster results
    "final_criteria.csv": "final_criteria_ref.csv",  # criterion and area results
    "iterations_values_log.csv": "iterations_values_log_ref.csv",  # iterative logs
}

study_cases = pytest.mark.parametrize(study_parameters, study_values)


def prepare_study_data(study_path: Path, rel_std_indicator: str, tmp_path: Path) -> Path:
    """Copy the study into tmp_path and select the input file of the indicator."""
    shutil.copytree(study_path, tmp_path, dirs_exist_ok=True)
    shutil.copyfile(
        tmp_path / rel_std_indicator / "input_balancing.yml",
        tmp_path / "user" / "balancing" / "input_balancing.yml",
    )
    return tmp_path


def run_balancing(install_dir, study_path: Path) -> None:
    executable = Path(install_dir) / get_conf("BALANCING")
    command = [str(executable), "--study", str(study_path), "--threads", "1"]
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    assert result.returncode == 0, (
        f"Command failed: {' '.join(command)}\n"
        f"--- stdout ---\n{result.stdout}\n"
        f"--- stderr ---\n{result.stderr}"
    )


def find_run_directory(output_dir: Path) -> Path:
    run_dirs = [p for p in output_dir.iterdir() if p.is_dir()]
    assert len(run_dirs) == 1, (
        f"Expected exactly one run directory in {output_dir}, found {len(run_dirs)}"
    )
    return run_dirs[0]


def read_lines(file_path: Path) -> List[str]:
    # Raises FileNotFoundError with the path if the file is missing
    with open(file_path, "r", encoding="utf-8") as f:
        return [line.rstrip("\n") for line in f]


def compare_csv_to_ref(output_file: Path, ref_file: Path) -> None:
    """Compare two CSV files line by line, reporting every difference."""
    prefix = f"File {output_file}: "
    errors = []

    lines = zip_longest(read_lines(ref_file), read_lines(output_file))
    for line_no, (ref_line, out_line) in enumerate(lines, start=1):
        if ref_line == out_line:
            continue
        if ref_line is None:
            errors.append(f"{prefix}Line {line_no}: unexpected extra line '{out_line}'")
            continue
        if out_line is None:
            errors.append(f"{prefix}Line {line_no}: missing in output file")
            continue

        errors.append(f"{prefix}Line {line_no}: expected '{ref_line}', got '{out_line}'")

        # Detail the difference column by column
        tokens = zip_longest(ref_line.split(","), out_line.split(","), fillvalue="<missing>")
        for col, (ref_token, out_token) in enumerate(tokens, start=1):
            if ref_token != out_token:
                errors.append(
                    f"{prefix}Line {line_no} col {col}: "
                    f"expected '{ref_token}', got '{out_token}'"
                )

    assert not errors, "\n".join(errors)


def run_balancing_test(install_dir, study, rel_std_indicator: str, tmp_path: Path) -> None:
    study_dir = prepare_study_data(Path(study["path"]), rel_std_indicator, tmp_path)
    run_balancing(install_dir, study_dir)

    run_dir = find_run_directory(study_dir / "output")
    ref_dir = study_dir / rel_std_indicator
    for output_name, ref_name in OUTPUT_TO_REF.items():
        compare_csv_to_ref(run_dir / output_name, ref_dir / ref_name)


@study_cases
@pytest.mark.unspenerg
def test_balancing_unspenerg(install_dir, study, tmp_path):
    run_balancing_test(install_dir, study, "UNSP_ENERG", tmp_path)


@study_cases
@pytest.mark.lole
def test_balancing_lole(install_dir, study, tmp_path):
    run_balancing_test(install_dir, study, "LOLE", tmp_path)
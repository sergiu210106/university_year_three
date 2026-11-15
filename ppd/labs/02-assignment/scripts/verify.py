#!/usr/bin/env python3
"""Small independent oracle and boundary tests; no third-party dependencies."""
import random
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"


def commands():
    return [
        [str(BUILD / "seq")], ["java", "-cp", str(BUILD), "Seq"],
        [str(BUILD / "par")], ["java", "-cp", str(BUILD), "Par"],
    ]


def reference(image, kernel):
    # Intentionally out of place, only in this small test oracle.
    rows, cols = len(image), len(image[0])
    return [[sum(image[max(0, min(rows - 1, i + a - 1))]
                      [max(0, min(cols - 1, j + b - 1))] * kernel[a][b]
                 for a in range(3) for b in range(3))
             for j in range(cols)] for i in range(rows)]


def write_input(path, image, kernel):
    with path.open("w") as output:
        output.write(f"{len(image)} {len(image[0])}\n")
        for row in image:
            output.write(" ".join(map(str, row)) + "\n")
        output.write("3\n")
        for row in kernel:
            output.write(" ".join(map(str, row)) + "\n")


def main():
    rng = random.Random(42)
    asymmetric = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
    identity = [[0, 0, 0], [0, 1, 0], [0, 0, 0]]
    cases = [
        ([[1, 2], [3, 4]], asymmetric),
        ([[7]], asymmetric),
        ([[1, 2, 3, 4]], asymmetric),
        ([[1], [2], [3], [4]], asymmetric),
        ([[2, -1], [7, 3]], identity),
        ([[2, -1], [7, 3]], [[0] * 3 for _ in range(3)]),
        ([[-2147483648, 2147483647]], identity),
    ]
    for rows, cols in [(2, 7), (3, 2), (7, 13), (17, 9), (33, 5), (25, 31)]:
        image = [[rng.randrange(256) for _ in range(cols)] for _ in range(rows)]
        kernel = [[rng.randint(-5, 5) for _ in range(3)] for _ in range(3)]
        cases.append((image, kernel))
    input_path, output_path = BUILD / "verify_input.txt", BUILD / "verify_output.txt"
    checks = 0
    for image, kernel in cases:
        expected = reference(image, kernel)
        # Fixes padding, orientation and weights independently of all implementations.
        if image == [[1, 2], [3, 4]]:
            assert expected == [[111, 126], [141, 156]]
        write_input(input_path, image, kernel)
        for index, command in enumerate(commands()):
            for threads in ([None] if index < 2 else [1, 2, 3, 16, len(image) + 2]):
                args = command + [str(input_path), str(output_path)]
                if threads is not None:
                    args.append(str(threads))
                result = subprocess.run(args, check=True, capture_output=True, text=True)
                assert len(result.stdout.split()) == 2, result.stdout
                values = list(map(int, output_path.read_text().split()))
                assert values[:2] == [len(image), len(image[0])]
                assert values[2:] == [value for row in expected for value in row], args
                checks += 1
    # Repeated scheduling at one-row and uneven worker boundaries.
    image, kernel = cases[-1]
    write_input(input_path, image, kernel)
    expected = [value for row in reference(image, kernel) for value in row]
    for _ in range(10):
        for command in commands()[2:]:
            subprocess.run(command + [str(input_path), str(output_path), "16"],
                           check=True, capture_output=True)
            assert list(map(int, output_path.read_text().split()))[2:] == expected
            checks += 1
    invalid_inputs = ["0 1", "1 -1", "1 1\n7\n5", "1 1\n7\n3\n1 2", "x 1", "1 1\n2147483648", "1 1\n-"]
    for text in invalid_inputs:
        input_path.write_text(text)
        for index, command in enumerate(commands()):
            args = command + [str(input_path), str(output_path)] + (["2"] if index >= 2 else [])
            result = subprocess.run(args, capture_output=True)
            assert result.returncode != 0 and result.stderr, args
            checks += 1
    write_input(input_path, [[7]], identity)
    for command in commands()[2:]:
        for count in ["0", "-1", "abc", "2x"]:
            result = subprocess.run(command + [str(input_path), str(output_path), count],
                                    capture_output=True)
            assert result.returncode != 0 and result.stderr
            checks += 1
    print(f"PASS: {checks} checks (independent oracle, repeated boundaries, invalid inputs).")


if __name__ == "__main__":
    main()

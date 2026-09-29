#!/usr/bin/env bash
# Differential test: run every fixture through the Python prototype and the
# C++ port, then compare stdout, stderr and the exit code.
#
# Usage: tools/differential_test.sh [fixture.resy ...]
# Without arguments every tests/differential/*.resy file is used.

set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(dirname "$script_dir")"
prototype_dir="$(dirname "$project_dir")/PyResy"
cpp_cli="$project_dir/build/resiris"
fixture_dir="$project_dir/tests/differential"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

if [ ! -x "$cpp_cli" ]; then
    echo "missing $cpp_cli - build first: cmake --build build" >&2
    exit 2
fi

if [ "$#" -gt 0 ]; then
    fixtures=("$@")
else
    fixtures=("$fixture_dir"/*.resy)
fi

passed=0
failed=0

for fixture in "${fixtures[@]}"; do
    name="$(basename "$fixture" .resy)"

    (cd "$prototype_dir" && python3 -m resiris.cli "$fixture") > "$work_dir/$name.py" 2>&1
    python_status=$?

    "$cpp_cli" "$fixture" > "$work_dir/$name.cpp" 2>&1
    cpp_status=$?

    if [ "$python_status" -eq "$cpp_status" ] && diff -q "$work_dir/$name.py" "$work_dir/$name.cpp" > /dev/null; then
        passed=$((passed + 1))
        echo "ok   $name (exit $python_status, $(wc -l < "$work_dir/$name.py") lines)"
    else
        failed=$((failed + 1))
        echo "FAIL $name (python exit $python_status, c++ exit $cpp_status)"
        diff "$work_dir/$name.py" "$work_dir/$name.cpp" | head -20
    fi
done

echo
echo "differential: $passed matched, $failed differed"
[ "$failed" -eq 0 ]

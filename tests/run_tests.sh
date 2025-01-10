#!/bin/bash

run_dir=$(readlink -f $(pwd))
param_path=$1

for dir in "$run_dir"/*; do
    if [ -d "$dir" ]; then
        for test_script in "$dir"/*.sh; do
            if [ -x "$test_script" ]; then
                echo "$dir"
                "$test_script" "$dir" "$param_path";
            fi
        done
        echo ""
    fi
done

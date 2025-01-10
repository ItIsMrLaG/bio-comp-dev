#!/bin/bash

run_dir=$(readlink -f $1);
include_fl=$(readlink -f $run_dir/include.cfg);
param_path=$(readlink -f $2);
PREFIX="|--->"
SET_PREFIX="|-> "

function run_file() {
    local fio_fl=$1;
    local setup_fl=$2;
    local ret_s="";

    while IFS= read -r line
    do

        echo -n "$line" > "$param_path/parameters/bcomp_mapper";
        sleep 0.2;

        fio $fio_fl > /dev/null;

        if [ $? -eq 0 ]
        then
            ret_s="\e[32mSUCCESS\e[0m"
        else
            ret_s="\e[31mFAILED\e[0m"
        fi

        echo -e "$PREFIX$ret_s: \e[36m$line\e[0m"

        echo -n "UNMAP_DEV" > "$param_path/parameters/bcomp_unmapper";
        sleep 0.2;

    done < "$setup_fl"

}

function run_all_tests() {

    while IFS= read -r included_dir; do
        local _path="$run_dir/$included_dir"

        for fio_fl in $_path/*.fio; do

            for setup_fl in $_path/*.cfg; do

                if [ -e "$fio_fl" ] && [ -e "$setup_fl" ];
                then
                    echo -e "$SET_PREFIX\e[34mTEST-SET:\e[0m$included_dir";
                    run_file "$fio_fl" "$setup_fl";
                fi
            done
        done

    done < "$include_fl"
}

echo -e "\e[34mLZ4\e[0m";
run_all_tests;

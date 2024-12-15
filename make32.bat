@echo off
python runtime_env_generator.py
call g++32 bin_runtime.cpp -o bin_runtime -s -O2 -Wall
call g++32 bin_dk.cpp -o bin_dk -s -O2 -Wall & bin_dk
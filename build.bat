@echo off
python runtime_env_generator.py
g++ bin_runtime.cpp -o bin_runtime -ldbghelp -s -O2 -Wall
g++ bin_dk.cpp -o bin_dk -s -O2 -Wall & bin_dk
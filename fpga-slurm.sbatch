#!/bin/bash
#SBATCH --job-name=fpga-test
#SBATCH --gres=gpu:1   # request 1 FPGA
#SBATCH --output=fpga_out_%j.txt

source /opt/Vitis/2024.1/settings64.sh
vitis_run sample.py

echo "Job finished."
#!/bin/bash

CPUID=7
ORIG_ASLR=$(cat /proc/sys/kernel/randomize_va_space)
ORIG_GOV=$(cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor)
ORIG_MIN_FREQ=$(cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_min_freq)
ORIG_MAX_FREQ=$(cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_max_freq)
ORIG_IDLESTATE1=$(cat /sys/devices/system/cpu/cpu$CPUID/cpuidle/state1/disable)

sudo bash -c "echo 0 > /proc/sys/kernel/randomize_va_space"
sudo bash -c "echo performance > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo bash -c "echo $ORIG_MAX_FREQ > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_min_freq"
sudo bash -c "echo 1 > /sys/devices/system/cpu/cpu$CPUID/cpuidle/state1/disable"

make client_stat
make load
rm -f plot_input
sudo taskset -c $CPUID ./client_stat >plot_input
gnuplot scripts/plot.gp
make unload

sudo bash -c "echo $ORIG_ASLR >  /proc/sys/kernel/randomize_va_space"
sudo bash -c "echo $ORIG_GOV > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo bash -c "echo $ORIG_MIN_FREQ > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_min_freq"
sudo bash -c "echo $ORIG_IDLESTATE1 > /sys/devices/system/cpu/cpu$CPUID/cpuidle/state1/disable"

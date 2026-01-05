#!/bin/bash
cd /home/daniel/energy-monitor
python3 generate_day.py --date $(date -d yesterday +%F)

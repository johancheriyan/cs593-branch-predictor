#!/usr/bin/bash
# =============================================================================
# Parameter Exploration for Two-Level Local Branch Predictor
# =============================================================================
# Template: two_level_local<PHT_LOG_SIZE, HIST_LEN>
# BHT size is always 2^HIST_LEN (tied to history length)
#
# Three sweeps:
#   1. PHT size sweep (vary PHT, fix HIST_LEN=10)
#   2. History length sweep (vary HIST_LEN, fix PHT=12)
#   3. Combined sweet-spot verification
# =============================================================================

echo "===== Sweep 1: PHT Size (HIST_LEN=14, BHT WIDTH=2) ====="
echo "PHT entries: 256, 512, 1024, 2048, 4096"
./compile cbp -DPREDICTOR="two_level_local<8,14,2>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<9,14,2>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<10,14,2>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<11,14,2>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,14,2>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

echo ""
echo "===== Sweep 2: History Length (PHT=4096, BHT WIDTH=2) ====="
echo "History: 4, 6, 8, 10, 12, 14 bits"
./compile cbp -DPREDICTOR="two_level_local<12,4>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,6>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,8>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,12>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,14>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

echo ""
echo "===== Sweep 3: Vary BHT Width (PHT=4096, HIST_LEN=14) ====="
echo "BHT width: 1, 2, 4 bits"
./compile cbp -DPREDICTOR="two_level_local<12,14,1>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000 
./compile cbp -DPREDICTOR="two_level_local<12,14,2>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,14,3>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,14,4>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

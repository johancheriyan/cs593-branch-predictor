#!/usr/bin/bash
# =============================================================================
# Parameter Exploration for Two-Level Local Branch Predictor
# =============================================================================
# This script compiles and runs the gshare tournament predictor with various parameter sweeps
# Parameters to explore:
# - Global History Length (GLOG): 8, 10, 12, 14, 16
# - Pattern History Table (PHT) entries: 256, 512, 1024, 2048, 4096
# - Global Table entries (CLOG): 8, 10, 12, 14, 16
# Note: For simplicity, we will keep GLOG and CLOG the same in this exploration

echo "=========================================================== G-Share Predictor Sweep Complete ==========================================================="

./compile cbp -DPREDICTOR="global_predictor<6,6,6,1>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<8,8,8,1>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<10,10,10,1>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<12,12,12,1>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<14,14,14,1>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human


echo "=========================================================== Regular Global Predictor Sweep Complete ==========================================================="

./compile cbp -DPREDICTOR="global_predictor<6,6,6,0>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<8,8,8,0>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<10,10,10,0>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<12,12,12,0>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="global_predictor<14,14,14,0>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human


echo "=========================================================== Local Predictor Sweep Complete ==========================================================="
echo "====================================="
echo "6, 8, 10, 12, 14"
echo "====================================="


./compile cbp -DPREDICTOR="two_level_local<6,6,3>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="two_level_local<8,8,3>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="two_level_local<10,10,3>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="two_level_local<12,12,3>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human
./compile cbp -DPREDICTOR="two_level_local<14,14,3>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000 --format human



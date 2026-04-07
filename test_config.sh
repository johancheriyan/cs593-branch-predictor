#!/usr/bin/bash
# Vary history length
./compile cbp -DPREDICTOR="two_level_local<12,4,4>"   && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,8,8>"   && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,14,14>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

# Vary PHT size
./compile cbp -DPREDICTOR="two_level_local<8,10,10>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<14,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

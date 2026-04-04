#!/usr/bin/bash
# PHT size sweep (history=10, BHT=10)
./compile cbp -DPREDICTOR="two_level_local<8,10,10>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<10,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<14,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<16,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

# History length sweep (PHT=12, BHT=history)
./compile cbp -DPREDICTOR="two_level_local<12,4,4>"   && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,6,6>"   && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,8,8>"   && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,12,12>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,14,14>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

# BHT aliasing sweep (PHT=12, history=10, vary BHT)
./compile cbp -DPREDICTOR="two_level_local<12,10,8>"  && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10,10>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10,12>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
./compile cbp -DPREDICTOR="two_level_local<12,10,14>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000

#!/usr/bin/bash
# PHT size sweep (history=10, BHT=10)
# Sweep global history length
for G in 8 10 12 14 16; do
  ./compile cbp -DPREDICTOR="tournament<8,10,10,14,$G,12>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
done

# Sweep local history length and BHT size together
for H in 8 10 12 14; do
  ./compile cbp -DPREDICTOR="tournament<12,$H,$H>" && ./cbp ./gcc_test_trace.gz test 1000000 40000000
done
``` 
 
 
 


 
 
 
 
 
 


 
 
 


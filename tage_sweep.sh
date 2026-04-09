#!/bin/bash
# Task 4: TAGE Parameter Optimization
# Sweeps key parameters and logs results to CSV
# Usage: bash tage_sweep.sh <trace_path> <trace_name> <warmup> <measurement>
#   e.g. bash tage_sweep.sh ./gcc_test_trace.gz test 1000000 40000000

TRACE=${1:?Usage: $0 <trace> <name> <warmup> <measure>}
NAME=${2:?}
WARM=${3:?}
MEAS=${4:?}

OUTFILE="tage_results.csv"
echo "config,NUMG,LOGG,LOGB,TAGW,GHIST,LOGP1,GHIST1,instructions,branches,condbr,predictions,extra_cycles,short_misp,block_short_misp,mispredictions,p1_lat,p2_lat,epi" > "$OUTFILE"

run_config() {
    local NUMG=$1 LOGG=$2 LOGB=$3 TAGW=$4 GHIST=$5 LOGP1=$6 GHIST1=$7
    local TAG="N${NUMG}_G${LOGG}_B${LOGB}_T${TAGW}_H${GHIST}_P${LOGP1}_PH${GHIST1}"
    local PRED="tage<6,${NUMG},${LOGG},${LOGB},${TAGW},${GHIST},${LOGP1},${GHIST1}>"

    echo ">>> $TAG"
    if ./compile cbp -DPREDICTOR="$PRED" 2>/dev/null; then
        local OUT
        OUT=$(./cbp "$TRACE" "$NAME" "$WARM" "$MEAS" 2>/dev/null)
        if [ -n "$OUT" ]; then
            echo "${TAG},${NUMG},${LOGG},${LOGB},${TAGW},${GHIST},${LOGP1},${GHIST1},${OUT#*,}" >> "$OUTFILE"
            # Extract mispredictions for quick display
            local MISP=$(echo "$OUT" | cut -d, -f9)
            local COND=$(echo "$OUT" | cut -d, -f4)
            echo "    misp=$MISP / cond=$COND"
        else
            echo "    FAILED (runtime)"
        fi
    else
        echo "    FAILED (compile)"
    fi
}

echo "============================================"
echo "Phase 1: Baseline"
echo "============================================"
run_config 8 11 12 11 100 14 6

echo "============================================"
echo "Phase 2: Sweep NUMG (number of tagged tables)"
echo "============================================"
for N in 4 6 8 10 12 14 16; do
    run_config $N 11 12 11 100 14 6
done

echo "============================================"
echo "Phase 3: Sweep LOGG (tagged table size)"
echo "============================================"
for G in 9 10 11 12 13 14; do
    run_config 8 $G 12 11 100 14 6
done

echo "============================================"
echo "Phase 4: Sweep GHIST (max global history)"
echo "============================================"
for H in 50 75 100 150 200 300 500; do
    run_config 8 11 12 11 $H 14 6
done

echo "============================================"
echo "Phase 5: Sweep TAGW (tag width)"
echo "============================================"
for T in 8 10 11 12 14 16; do
    run_config 8 11 12 $T 100 14 6
done

echo "============================================"
echo "Phase 6: Sweep LOGB (bimodal table size)"
echo "============================================"
for B in 10 12 14 16; do
    run_config 8 11 $B 11 100 14 6
done

echo "============================================"
echo "Phase 7: Sweep LOGP1 and GHIST1 (P1 predictor)"
echo "============================================"
for P in 12 14 16; do
    for PH in 4 6 8 12; do
        run_config 8 11 12 11 100 $P $PH
    done
done

echo "============================================"
echo "Phase 8: Combined best candidates"
echo "============================================"
# Try larger configs targeting accuracy (unrestricted track)
for N in 10 12; do
    for G in 12 13; do
        for H in 200 300; do
            for T in 12 14; do
                run_config $N $G 14 $T $H 14 6
            done
        done
    done
done

echo ""
echo "Results written to $OUTFILE"
echo "Sort by mispredictions: sort -t, -k16 -n $OUTFILE | head -20"

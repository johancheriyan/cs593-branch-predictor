// =============================================================================
// Two-Level Local Branch Predictor for CBP-NG
// =============================================================================
// "Two-level" refers to the two levels of table lookup:
//
//   Level 1: PHT (Pattern History Table)
//     - Indexed by lower bits of branch PC (skip 2 alignment bits)
//     - Each entry: HIST_LEN-bit shift register of local taken/not-taken history
//
//   Level 2: BHT (Branch History Table)
//     - Indexed by the local history value read from the PHT
//     - Each entry: 2-bit saturating counter
//     - Predict TAKEN when counter >= 2 (MSB == 1)
//
// Flow:  PC --> PHT[PC] --> history --> BHT[history] --> 2-bit counter --> prediction
//
// BHT size is tied to 2^HIST_LEN (no separate parameter) to avoid:
//   - Wasted entries when BHT > 2^HIST_LEN (unreachable entries)
//   - Destructive aliasing when BHT < 2^HIST_LEN (truncated history)
//
// Template parameters for design space exploration:
//   PHT_LOG_SIZE: log2 of PHT entries (varies capacity vs aliasing)
//   HIST_LEN:     bits of history per branch (varies pattern depth vs training)
//
// HARCOM constraint: one RAM access per cycle per table.
//   predict2 reads PHT and BHT in cycle N.
//   update_cycle calls need_extra_cycle() to advance to cycle N+1,
//   then writes PHT and BHT in that new cycle.
// =============================================================================

#pragma once
#include "../cbp.hpp"
#include "common.hpp"

using namespace hcm;

template <u64 PHT_LOG_SIZE = 12, u64 HIST_LEN = 10>
struct two_level_local : predictor {

    // =====================================================================
    // Hardware structures
    // =====================================================================

    // PHT: one HIST_LEN-bit local history shift register per branch
    //   - Indexed by lower PHT_LOG_SIZE bits of PC (after skipping 2 LSBs)
    //   - Stores the pattern of recent taken/not-taken outcomes for that branch
    //   - Example: history 110110 means T,T,NT,T,T,NT (oldest to newest)
    ram<val<HIST_LEN>, (1 << PHT_LOG_SIZE)> pht{"pht"};

    // BHT: 2-bit saturating counters indexed by local history
    //   - Size = 2^HIST_LEN so every possible history pattern has its own counter
    //   - States: 00=strongly NT, 01=weakly NT, 10=weakly T, 11=strongly T
    ram<val<2>, (1 << HIST_LEN)> bht{"bht"};

    // Saved state from predict2 for use in update_cycle
    //   (HARCOM enforces single RAM access per cycle per table,
    //    so we save values read during prediction and write in a new cycle)
    reg<PHT_LOG_SIZE> saved_pht_idx;
    reg<HIST_LEN>     saved_bht_idx;
    reg<HIST_LEN>     saved_hist;
    reg<2>            saved_ctr;
    reg<1>            saved_taken;
    u64 had_branch = 0;

    // =====================================================================
    // Level 1 prediction — unused (return static value)
    // =====================================================================
    // We use only level 2 for prediction. Level 1 returns 0.

    val<1> predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }

    val<1> reuse_predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }

    // =====================================================================
    // Level 2 prediction — two-level local (PHT + BHT)
    // =====================================================================
    // Called at the start of each prediction block (cycle N — reads RAMs).
    //
    //   Step 1: PC[2+PHT_LOG_SIZE-1 : 2] --> PHT index
    //   Step 2: PHT[index] --> local history (HIST_LEN bits)
    //   Step 3: local history --> BHT index
    //   Step 4: BHT[index] --> 2-bit counter
    //   Step 5: counter MSB --> prediction (1 = taken, 0 = not taken)

    val<1> predict2(val<64> inst_pc) override {
        // Step 1: Compute PHT index from PC (skip 2 LSBs for instruction alignment)
        val<PHT_LOG_SIZE> pht_idx = inst_pc.fo1() >> 2;
        pht_idx.fanout(hard<2>{});
        saved_pht_idx = pht_idx;

        // Step 2: Read local history from PHT
        val<HIST_LEN> history = pht.read(pht_idx);
        history.fanout(hard<2>{});
        saved_hist = history;

        // Step 3: Use full history as BHT index (no truncation)
        val<HIST_LEN> bht_idx = history;
        bht_idx.fanout(hard<2>{});
        saved_bht_idx = bht_idx;

        // Step 4: Read 2-bit saturating counter from BHT
        val<2> counter = bht.read(bht_idx);
        counter.fanout(hard<2>{});
        saved_ctr = counter;

        // Step 5: Predict taken if MSB == 1 (counter >= 2)
        return val<1>{counter >> 1};
    }

    val<1> reuse_predict2(val<64> inst_pc) override {
        return predict2(inst_pc);
    }

    // =====================================================================
    // Update — buffer branch outcome
    // =====================================================================
    // Called after each conditional branch with the actual direction.

    void update_condbr([[maybe_unused]] val<64> branch_pc,
                       val<1> taken,
                       [[maybe_unused]] val<64> next_pc) override {
        saved_taken = taken.fo1();
        had_branch = 1;
    }

    // =====================================================================
    // Update — write tables in extra cycle (cycle N+1)
    // =====================================================================
    // Called at end of prediction block. Uses saved registers to avoid
    // reading RAMs again (HARCOM: one access per cycle per table).
    //
    //   1. Update BHT counter: increment if taken, decrement if not-taken
    //   2. Update PHT history: shift left, insert new outcome as bit 0
    //   3. need_extra_cycle() advances to cycle N+1 before writing

    void update_cycle([[maybe_unused]] instruction_info &block_end_info) override {
        if (!had_branch) return;
        had_branch = 0;

        val<1> taken = saved_taken;
        taken.fanout(hard<2>{});

        // --- Update BHT counter (saturating increment/decrement) ---
        val<2> old_ctr = saved_ctr;
        val<2> new_ctr = update_ctr(old_ctr, taken);

        // --- Update PHT history (shift left, insert taken as new LSB) ---
        // Decompose history into individual bits, shift, reassemble
        val<HIST_LEN> old_hist = saved_hist;
        arr<val<1>, HIST_LEN> bits = old_hist.fo1().make_array(val<1>{});
        arr<val<1>, HIST_LEN> shifted = [&](u64 i) -> val<1> {
            if (i == 0) return taken;          // new outcome enters at position 0
            return bits[i - 1].fo1();          // all other bits shift up by 1
        };
        val<HIST_LEN> new_hist = shifted.fo1().concat();

        // --- Advance to cycle N+1 (avoids read/write conflict on RAMs) ---
        need_extra_cycle(val<1>(1));

        // --- Write updated values back to RAMs ---
        bht.write(saved_bht_idx, new_ctr.fo1());
        pht.write(saved_pht_idx, new_hist.fo1());
    }
};

// =============================================================================
// Two-Level Local Branch Predictor for CBP-NG
// =============================================================================
// PHT (Pattern History Table): per-branch local history shift registers
// BHT (Branch History Table): 2-bit saturating counters
//
// HARCOM constraint: each ram allows only 1 access per cycle (read OR write).
// predict2 reads pht and bht in cycle N.
// update_cycle calls need_extra_cycle() to advance to cycle N+1,
// then writes pht and bht in that new cycle.
// =============================================================================

#pragma once
#include "../cbp.hpp"
#include "common.hpp"

using namespace hcm;

template <u64 PHT_LOG_SIZE = 12, u64 HIST_LEN = 10, u64 BHT_LOG_SIZE = 10>
struct two_level_local : predictor {

    // PHT: one HIST_LEN-bit local history per branch
    ram<val<HIST_LEN>, (1 << PHT_LOG_SIZE)> pht;

    // BHT: 2-bit saturating counters
    ram<val<2>, (1 << BHT_LOG_SIZE)> bht;

    // Saved state from predict2, reused in update_cycle
    reg<PHT_LOG_SIZE> saved_pht_idx;
    reg<BHT_LOG_SIZE> saved_bht_idx;
    reg<HIST_LEN> saved_hist;
    reg<2> saved_ctr;
    reg<1> saved_taken;
    u64 had_branch = 0;

    // ----- Level 1: unused -----

    val<1> predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }

    val<1> reuse_predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }

    // ----- Level 2: main prediction (cycle N — reads RAMs) -----

    val<1> predict2(val<64> inst_pc) override {
        // PHT index from PC (skip 2 LSBs)
        val<PHT_LOG_SIZE> pht_idx = inst_pc.fo1() >> 2;
        pht_idx.fanout(hard<2>{});
        saved_pht_idx = pht_idx;

        // Read local history — save for update
        val<HIST_LEN> history = pht.read(pht_idx);
        history.fanout(hard<2>{});
        saved_hist = history;

        // BHT index from history
        val<BHT_LOG_SIZE> bht_idx = history;
        bht_idx.fanout(hard<2>{});
        saved_bht_idx = bht_idx;

        // Read counter — save for update
        val<2> counter = bht.read(bht_idx);
        counter.fanout(hard<2>{});
        saved_ctr = counter;

        // Predict taken if MSB == 1 (counter >= 2)
        return val<1>{counter >> 1};
    }

    val<1> reuse_predict2(val<64> inst_pc) override {
        return predict2(inst_pc);
    }

    // ----- Update: buffer branch info -----

    void update_condbr([[maybe_unused]] val<64> branch_pc,
                       val<1> taken,
                       [[maybe_unused]] val<64> next_pc) override {
        saved_taken = taken.fo1();
        had_branch = 1;
    }

    // ----- Update: write tables in extra cycle (cycle N+1) -----

    void update_cycle([[maybe_unused]] instruction_info &block_end_info) override {
        if (!had_branch) return;
        had_branch = 0;

        val<1> taken = saved_taken;
        taken.fanout(hard<2>{});

        // Compute new counter (no RAM access, uses saved register)
        val<2> old_ctr = saved_ctr;
        val<2> new_ctr = update_ctr(old_ctr, taken);

        // Compute new history (no RAM access, uses saved register)
        val<HIST_LEN> old_hist = saved_hist;
        arr<val<1>, HIST_LEN> bits = old_hist.fo1().make_array(val<1>{});
        arr<val<1>, HIST_LEN> shifted = [&](u64 i) -> val<1> {
            if (i == 0) return taken;
            return bits[i - 1].fo1();
        };
        val<HIST_LEN> new_hist = shifted.fo1().concat();

        // Advance to a new cycle so RAM writes don't conflict with
        // the RAM reads that happened in predict2
        need_extra_cycle(val<1>(1));

        // Now in cycle N+1: safe to write RAMs
        bht.write(saved_bht_idx, new_ctr.fo1());
        pht.write(saved_pht_idx, new_hist.fo1());
    }
};

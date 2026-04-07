// =============================================================================
// Tournament Branch Predictor for CBP-NG (Task 3)
// =============================================================================
// Combines:
//   1. Local predictor  – two-level (PHT of local histories → BHT of 2-bit ctrs)
//   2. Global predictor – gshare (global history XOR PC → 2-bit ctrs)
//   3. Chooser table    – 2-bit saturating counters selecting local vs global
//
// Chooser policy: counter >= 2 → use global, else use local.
// Updated toward whichever predictor was correct (only when they disagree).
//
// HARCOM rules observed:
//   - fo1() may only be called ONCE per value (marks it consumed).
//   - After fanout(hard<N>{}), use N normal reads — NOT fo1().
//   - Each RAM: at most 1 read + 1 write per cycle; use need_extra_cycle().
// =============================================================================

#pragma once
#include "../cbp.hpp"
#include "common.hpp"

using namespace hcm;

template <
    u64 PHT_LOG   = 12,   // log2 entries in local PHT
    u64 HIST_LEN  = 12,   // local history length (bits)
    u64 BHT_LOG   = 12,   // log2 entries in local BHT
    u64 GLOG      = 14,   // log2 entries in global table
    u64 GHIST     = 14,   // global history length (bits)
    u64 CLOG      = 12    // log2 entries in chooser table
>
struct tournament : predictor {

    // ===================== Local predictor =====================
    ram<val<HIST_LEN>, (1 << PHT_LOG)> local_pht {"L_PHT"};
    ram<val<2>,        (1 << BHT_LOG)> local_bht {"L_BHT"};

    // ===================== Global predictor ====================
    reg<GHIST> global_history;
    ram<val<2>, (1 << GLOG)> global_table {"G_TBL"};

    // ===================== Chooser =============================
    ram<val<2>, (1 << CLOG)> chooser {"CHOOSE"};

    // ===================== Saved state =========================
    reg<PHT_LOG>  sv_pht_idx;
    reg<BHT_LOG>  sv_bht_idx;
    reg<HIST_LEN> sv_local_hist;
    reg<2>        sv_local_ctr;
    reg<1>        sv_local_pred;

    reg<GLOG>     sv_global_idx;
    reg<2>        sv_global_ctr;
    reg<1>        sv_global_pred;

    reg<CLOG>     sv_chooser_idx;
    reg<2>        sv_chooser_ctr;

    reg<1>        sv_taken;
    u64           had_branch = 0;

    // ===================== Predict 1 (unused) ==================
    val<1> predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }
    val<1> reuse_predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }

    // ===================== Predict 2 (main) ====================
    val<1> predict2(val<64> inst_pc) override {
        // inst_pc is read 3 times: local idx, global idx, chooser idx
        inst_pc.fanout(hard<3>{});

        // --- Local predictor ---
        val<PHT_LOG> pht_idx = inst_pc >> 2;    // normal read 1
        pht_idx.fanout(hard<2>{});
        sv_pht_idx = pht_idx;

        val<HIST_LEN> history = local_pht.read(pht_idx);
        history.fanout(hard<2>{});
        sv_local_hist = history;

        val<BHT_LOG> bht_idx = history;
        bht_idx.fanout(hard<2>{});
        sv_bht_idx = bht_idx;

        val<2> local_ctr = local_bht.read(bht_idx);
        local_ctr.fanout(hard<2>{});
        sv_local_ctr = local_ctr;

        val<1> local_pred = val<1>{local_ctr >> 1};
        local_pred.fanout(hard<2>{});
        sv_local_pred = local_pred;

        // --- Global predictor ---
        auto compute_gidx = [&]() -> val<GLOG> {
            if constexpr (GHIST <= GLOG) {
                return (inst_pc >> 2) ^ (val<GLOG>{global_history} << (GLOG - GHIST));  // normal read 2
            } else {
                return global_history.make_array(val<GLOG>{}).append(val<GLOG>{inst_pc >> 2}).fold_xor();
            }
        };
        val<GLOG> global_idx = compute_gidx();
        global_idx.fanout(hard<2>{});
        sv_global_idx = global_idx;

        val<2> global_ctr = global_table.read(global_idx);
        global_ctr.fanout(hard<2>{});
        sv_global_ctr = global_ctr;

        val<1> global_pred = val<1>{global_ctr >> 1};
        global_pred.fanout(hard<2>{});
        sv_global_pred = global_pred;

        // --- Chooser ---
        val<CLOG> chooser_idx = inst_pc >> 2;   // normal read 3
        chooser_idx.fanout(hard<2>{});
        sv_chooser_idx = chooser_idx;

        val<2> chooser_ctr = chooser.read(chooser_idx);
        chooser_ctr.fanout(hard<2>{});
        sv_chooser_ctr = chooser_ctr;

        // chooser >= 2 → global, else local
        val<1> use_global = val<1>{chooser_ctr >> 1};
        return select(use_global, global_pred, local_pred);
    }

    val<1> reuse_predict2(val<64> inst_pc) override {
        return predict2(inst_pc);
    }

    // ===================== Update per branch ===================
    void update_condbr([[maybe_unused]] val<64> branch_pc,
                       val<1> taken,
                       [[maybe_unused]] val<64> next_pc) override {
        sv_taken = taken.fo1();
        had_branch = 1;
    }

    // ===================== Update per cycle =====================
    void update_cycle([[maybe_unused]] instruction_info &block_end_info) override {
        if (!had_branch) return;
        had_branch = 0;

        // taken is read 5 times: local ctr, history shift, global ctr, chooser cmp, ghist update
        val<1> taken = sv_taken;
        taken.fanout(hard<5>{});

        val<1> local_pred  = sv_local_pred;
        val<1> global_pred = sv_global_pred;
        local_pred.fanout(hard<3>{});
        global_pred.fanout(hard<3>{});

        // --- Update local BHT counter ---
        val<2> new_local_ctr = update_ctr(sv_local_ctr, taken);  // taken read 1

        // --- Update local PHT history ---
        val<HIST_LEN> old_hist = sv_local_hist;
        arr<val<1>, HIST_LEN> bits = old_hist.fo1().make_array(val<1>{});
        arr<val<1>, HIST_LEN> shifted = [&](u64 i) -> val<1> {
            if (i == 0) return taken;       // taken read 2
            return bits[i - 1].fo1();
        };
        val<HIST_LEN> new_hist = shifted.fo1().concat();

        // --- Update global counter ---
        val<2> new_global_ctr = update_ctr(sv_global_ctr, taken);  // taken read 3

        // --- Update chooser (only when predictors disagree) ---
        val<1> disagree = local_pred != global_pred;
        disagree.fanout(hard<2>{});
        val<1> global_correct = (global_pred == taken);  // taken read 4
        val<2> new_chooser_ctr = update_ctr(sv_chooser_ctr, global_correct.fo1());

        // --- Update global history ---
        global_history = (global_history << 1) | val<GHIST>{taken};  // taken read 5

        // --- Write RAMs (need extra cycle) ---
        need_extra_cycle(val<1>(1));

        local_bht.write(sv_bht_idx, new_local_ctr.fo1());
        local_pht.write(sv_pht_idx, new_hist.fo1());
        global_table.write(sv_global_idx, new_global_ctr.fo1());

        // Only update chooser when predictors disagreed
        execute_if(disagree, [&]() {
            chooser.write(sv_chooser_idx, new_chooser_ctr.fo1());
        });
    }
};

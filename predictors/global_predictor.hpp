// =============================================================================
// Tournament Branch Predictor
// =============================================================================
// Defaults to a modified version of Alpha 21264 Tournament Predictor 
// (with gshare instead of regular global predictor, and some parameter tweaks).
// =============================================================================

#pragma once
#include "../cbp.hpp"
#include "common.hpp"

using namespace hcm;

template <
    u64 GLOG      = 12,   // log2 entries in global table
    u64 GHIST_LEN = 12,   // global history length (bits)
    u64 CLOG      = 12,    // log2 entries in chooser table
    u64 GSHARE    = 1
>
struct global_predictor : predictor {

    static_assert(CLOG == GLOG, "Chooser and global predictor must be indexed by the same bits for simplicity");
    static_assert(GSHARE != 0 || (GLOG == CLOG && GHIST_LEN == GLOG), 
                  "Global history constraints failed: when GSHARE is 0, GHIST_LEN and GLOG must match.");
    // ===================== Local predictor =====================


    // ===================== Global predictor ====================
    reg<GHIST_LEN> global_history;
    ram<val<2>, (1 << GLOG)> global_table;

    // ===================== Chooser =============================

    // ===================== Saved state =========================



    reg<GLOG>     sv_global_idx;
    reg<2>        sv_global_ctr;
    reg<1>        sv_global_pred;


    reg<1>        sv_taken;
    u64           had_branch = 0;

    // ===================== Predict 1 (unused) ==================
    val<1> predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }
    val<1> reuse_predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }


    // ===================== GShare index computation (for global predictor) ==================
    auto compute_gidx(val<64> inst_pc) -> val<GLOG> {
        // if constexpr (GHIST_LEN <= GLOG) { // gshare idx
        //     // simply pad with zeros and xor with (PC >> 5), as every one in five is a branch we xor with multiples of 5
        //     return (inst_pc.fo1() >> 5).template slice<GLOG>() ^ (val<GLOG>{global_history});  
        // } else { // gshare
        //     return (inst_pc.fo1() >> 5).template slice<GLOG>() ^ (val<GLOG>{global_history});  ;
        // }
        return val<GLOG>(inst_pc >> 5) ^ (val<GLOG>{global_history}); 
    }

    // ===================== Predict 2 (main) ====================
    val<1> predict2(val<64> inst_pc) override {
        // inst_pc is read 3 times: local idx, global idx, chooser idx
            inst_pc.fanout(hard<2>{});

        // ======================================== Global Predictor ========================================
 
            val<GLOG> global_idx = (GSHARE != 0) ? compute_gidx(inst_pc) 
                                     : val<GLOG>{global_history};
            global_idx.fanout(hard<2>{});
            sv_global_idx = global_idx;

            val<2> global_ctr = global_table.read(global_idx);
            global_ctr.fanout(hard<2>{});
            sv_global_ctr = global_ctr;

            val<1> global_pred = val<1>{global_ctr >> 1};
            global_pred.fanout(hard<2>{});
            sv_global_pred = global_pred;
    
            return global_pred; // select local_pred if use_local=1 else global_pred
    }

    val<1> reuse_predict2(val<64> inst_pc) override {
        return predict2(inst_pc);
    }

    // ===================== Update per branch ===================
    void update_condbr([[maybe_unused]] val<64> branch_pc,
                       val<1> taken,
                       [[maybe_unused]] val<64> next_pc) override {
        taken.fanout(hard<2>{});   // local ctr, local hist, global ctr, chooser cmp, GHIST_LEN

        // --- Update global counter ---
        val<2> new_global_ctr = update_ctr(sv_global_ctr, taken); // taken read 3

        // Update global history (shift left, insert taken as LSB)
        val<GHIST_LEN> new_global_hist = (global_history << 1) | val<GHIST_LEN>{taken}; // taken read 4

        // --- Write RAMs (need extra cycle) ---
        need_extra_cycle(val<1>(1));


        global_table.write(sv_global_idx, new_global_ctr.fo1());

        global_history = new_global_hist.fo1();

    }

    // ===================== Update per cycle =====================
    void update_cycle([[maybe_unused]] instruction_info &block_end_info) {
 
    }
};

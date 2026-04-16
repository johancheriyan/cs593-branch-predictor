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
    u64 PHT_IDX_BITS   = 10,   // log2 entries in local PHT
    u64 LHIST_LEN  = 10,   // local history length (bits)
    u64 GLOG      = 12,   // log2 entries in global table
    u64 GHIST_LEN = 12,   // global history length (bits)
    u64 CLOG      = 12,    // log2 entries in chooser table
    u64 GSHARE    = 1
>
struct gshare_tournament : predictor {

    static_assert(CLOG == GLOG, "Chooser and global predictor must be indexed by the same bits for simplicity");
    static_assert(GSHARE != 0 || (GLOG == CLOG && GHIST_LEN == GLOG), 
                  "Global history constraints failed: when GSHARE is 0, GHIST_LEN and GLOG must match.");
    // ===================== Local predictor =====================
    ram<val<LHIST_LEN>, (1 << PHT_IDX_BITS)>  local_pht;
    ram<val<3>, (1 << LHIST_LEN)> local_bht;

    // ===================== Global predictor ====================
    reg<GHIST_LEN> global_history;
    ram<val<2>, (1 << GLOG)> global_table;

    // ===================== Chooser =============================
    ram<val<2>, (1 << CLOG)> chooser;

    // ===================== Saved state =========================
    reg<PHT_IDX_BITS>  sv_pht_idx;
    reg<LHIST_LEN> sv_bht_idx;
    reg<LHIST_LEN> sv_local_hist;
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
            inst_pc.fanout(hard<3>{});
        // ======================================== Local Predictor ========================================
            // Calculating the PHT idx (inst_pc >> 5) (one in every 5 instructions is a branch)
            val<PHT_IDX_BITS> pht_idx = val<PHT_IDX_BITS>(inst_pc >> 5);

            // --- Local predictor ---
            pht_idx.fanout(hard<2>{});
            sv_pht_idx = pht_idx;

            val<LHIST_LEN> history = local_pht.read(pht_idx);
            history.fanout(hard<2>{});
            sv_local_hist = history;

            // val<LHIST_LEN> bht_idx = history;
            // bht_idx.fanout(hard<2>{});
            // sv_bht_idx = bht_idx;

            val<3> local_ctr = local_bht.read(history);
            local_ctr.fanout(hard<2>{});
            sv_local_ctr = local_ctr;

            // We want the MSB (to tell whether its taken or not)
            val<1> local_pred = val<1>{local_ctr >> 2};
            local_pred.fanout(hard<2>{});
            sv_local_pred = local_pred;



        // ======================================== Global Predictor ========================================
            // auto compute_gidx = [&]() -> val<GLOG> {
            //     if constexpr (GHIST_LEN <= GLOG) {
            //         return (inst_pc >> 2) ^ (val<GLOG>{global_history} << (GLOG - GHIST_LEN));  // read 2 of 3
            //     } else {
            //         return global_history.make_array(val<GLOG>{}).append(val<GLOG>{inst_pc >> 2}).fold_xor();
            //     }
            // };
            // if gshare then compute idx
            // Initialize directly so no assignment operator is needed
            val<GLOG> global_idx = (GSHARE != 0) ? compute_gidx(inst_pc) 
                                     : val<GLOG>{global_history};
            global_idx.fanout(hard<4>{});
            sv_global_idx = global_idx;

            val<2> global_ctr = global_table.read(global_idx);
            global_ctr.fanout(hard<2>{});
            sv_global_ctr = global_ctr;

            val<1> global_pred = val<1>{global_ctr >> 1};
            global_pred.fanout(hard<2>{});
            sv_global_pred = global_pred;

        // ======================================== Chooser ========================================
    
            sv_chooser_idx = global_idx;

            val<2> chooser_ctr = chooser.read(global_idx);
            chooser_ctr.fanout(hard<2>{});
            sv_chooser_ctr = chooser_ctr;

            // chooser MSB=1 -> local, MSB=0 -> global
            val<1> use_local = val<1>{chooser_ctr >> 1};
            return select(use_local, local_pred, global_pred);
    }

    val<1> reuse_predict2(val<64> inst_pc) override {
        return predict2(inst_pc);
    }

    // ===================== Update per branch ===================
    void update_condbr([[maybe_unused]] val<64> branch_pc,
                       val<1> taken,
                       [[maybe_unused]] val<64> next_pc) override {
        taken.fanout(hard<5>{});   // local ctr, local hist, global ctr, chooser cmp, GHIST_LEN

        val<1> local_pred  = sv_local_pred;
        val<1> global_pred = sv_global_pred;
        local_pred.fanout(hard<2>{});   // disagree, local_correct
        global_pred.fanout(hard<2>{});  // disagree, (spare)

        // --- Update local BHT counter ---
        val<2> new_local_ctr = update_ctr(sv_local_ctr, taken);   // taken read 1

        // --- Update local PHT history (shift left, insert taken as LSB) ---
        val<LHIST_LEN> new_local_hist = sv_local_hist << 1 |  val<GHIST_LEN>{taken};
        

        // --- Update global counter ---
        val<2> new_global_ctr = update_ctr(sv_global_ctr, taken); // taken read 3

        // Update global history (shift left, insert taken as LSB)
        val<GHIST_LEN> new_global_hist = (global_history << 1) | val<GHIST_LEN>{taken}; // taken read 4

        // --- Update chooser (only when predictors disagree) ---
        val<1> disagree = local_pred != global_pred;               // local read 1, global read 1
        disagree.fanout(hard<2>{});
        val<1> local_correct = (local_pred == taken);              // local read 2, taken read 4
        val<2> new_chooser_ctr = update_ctr(sv_chooser_ctr, local_correct);

        // --- Write RAMs (need extra cycle) ---
        need_extra_cycle(val<1>(1));

        local_bht.write(sv_bht_idx, new_local_ctr.fo1());
        local_pht.write(sv_pht_idx, new_local_hist.fo1());

        global_table.write(sv_global_idx, new_global_ctr.fo1());
        global_history = new_global_hist;
        
        // Only update chooser when predictors disagreed
        execute_if(disagree, [&]() {
            chooser.write(sv_chooser_idx, new_chooser_ctr.fo1());
        }); 
    }

    // ===================== Update per cycle =====================
    void update_cycle([[maybe_unused]] instruction_info &block_end_info) {
 
    }
};

#include "../cbp.hpp"
#include "./common.hpp"

using namespace hcm;

template<u64 PHT_IDX_BITS = 10, u64 PHT_HISTORY_BITS = 6, u64 BHT_COUNTER_BITS = 2>
struct two_level_local : predictor { 

    ram<val<PHT_HISTORY_BITS>, (1ULL << PHT_IDX_BITS)> pht; // pattern history table
    ram<val<BHT_COUNTER_BITS>, (1ULL << PHT_HISTORY_BITS)> bht; // branch history table

    // Saved Registers
    reg<PHT_IDX_BITS>     saved_pht_idx;
    reg<PHT_HISTORY_BITS> saved_local_history;
    reg<BHT_COUNTER_BITS> saved_bht_counter;



    // Level 1: Always predict not taken (0)
    val<1> predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }
    val<1> reuse_predict1([[maybe_unused]] val<64> inst_pc) override {
        return val<1>(0);
    }

    // Level 2: Two level predictor 
    //  - PHT: local history shift register indexed by PC bits
    //  - BHT: xxx-bit saturating counters indexed by local history
    val<1> predict2(val<64> inst_pc) override {
        val <PHT_IDX_BITS> pht_idx = (inst_pc.fo1() >> 7);
        pht_idx.fanout(hard<2>{});
        saved_pht_idx = pht_idx; // Save for update
        
        val <PHT_HISTORY_BITS> local_history = pht.read(pht_idx);
        local_history.fanout(hard<2>{});
        saved_local_history = local_history; // Save for update

        val <BHT_COUNTER_BITS> bht_counter = bht.read(local_history);
        bht_counter.fanout(hard<2>{});
        saved_bht_counter = bht_counter; // Save for update

        return bht_counter.fo1() >> (BHT_COUNTER_BITS - 1); // Predict taken if MSB of counter is 1
    }

    val<1> reuse_predict2(val<64> inst_pc) override {
        return predict2(inst_pc);
    }

    void update_condbr([[maybe_unused]] val<64> branch_pc,
                       [[maybe_unused]] val<1> taken,
                       [[maybe_unused]] val<64> next_pc) override {
        need_extra_cycle(val<1>(1));
        // update bht 
        val<BHT_COUNTER_BITS> new_bht_counter = update_ctr(saved_bht_counter, taken);
        new_bht_counter.fanout(hard<2>{});
        bht.write(saved_local_history, new_bht_counter);
        // update pht
        val<PHT_HISTORY_BITS> new_local_history = (saved_local_history << 1) | taken;
        new_local_history.fanout(hard<2>{});
        pht.write(saved_pht_idx, new_local_history);        
    }

    void update_cycle([[maybe_unused]] instruction_info &block_end_info) { }
        
};



#include "cryptoTools/Common/Log.h"
#include <functional>
#include "UnitTests.h"

#include "Paxos_Tests.h"
#include "RsOprf_Tests.h"
#include "RsOpprf_Tests.h"
#include "RsPsi_Tests.h"
#include "RsCpsi_Tests.h"
#include "GMW_Tests.h"
#include "volePSI/GMW/Circuit.h"
#include "FileBase_Tests.h"

namespace volePSI_Tests
{
    using namespace volePSI;
    oc::TestCollection Tests([](oc::TestCollection& t) {
        
        t.add("Paxos_buildRow_Test         ", Paxos_buildRow_Test);
        t.add("Paxos_solve_Test            ", Paxos_solve_Test);
        t.add("Paxos_solve_u8_Test         ", Paxos_solve_u8_Test);
        t.add("Paxos_solve_mtx_Test        ", Paxos_solve_mtx_Test);
                                           
        t.add("Paxos_invE_Test             ", Paxos_invE_Test);
        t.add("Paxos_invE_g3_Test          ", Paxos_invE_g3_Test);
        t.add("Paxos_solve_gap_Test        ", Paxos_solve_gap_Test);
        t.add("Paxos_solve_rand_Test       ", Paxos_solve_rand_Test);
        t.add("Paxos_solve_rand_gap_Test   ", Paxos_solve_rand_gap_Test);
                                           
        t.add("Baxos_solve_Test            ", Baxos_solve_Test);
        t.add("Baxos_solve_mtx_Test        ", Baxos_solve_mtx_Test);
        t.add("Baxos_solve_par_Test        ", Baxos_solve_par_Test);
        t.add("Baxos_solve_rand_Test       ", Baxos_solve_rand_Test);
        
#ifdef VOLE_PSI_ENABLE_GMW
        t.add("SilentTripleGen_test        ", SilentTripleGen_test);
        //t.add("IknpTripleGen_test          ", IknpTripleGen_test);
        t.add("generateBase_test           ", generateBase_test);

        t.add("isZeroCircuit_Test          ", isZeroCircuit_Test);
        t.add("Gmw_half_test               ", Gmw_half_test);
        t.add("Gmw_basic_test              ", Gmw_basic_test);
        t.add("Gmw_inOut_test              ", Gmw_inOut_test);
        t.add("Gmw_xor_test                ", Gmw_xor_test);
        t.add("Gmw_and_test                ", Gmw_and_test);
        t.add("Gmw_or_test                 ", Gmw_or_test);
        t.add("Gmw_na_and_test             ", Gmw_na_and_test);
        t.add("Gmw_xor_and_test            ", Gmw_xor_and_test);
        t.add("Gmw_aa_na_and_test          ", Gmw_aa_na_and_test);
        t.add("Gmw_noLevelize_test         ", Gmw_noLevelize_test);
        t.add("Gmw_add_test                ", Gmw_add_test);
#endif

        t.add("RsOprf_eval_test            ", RsOprf_eval_test);
        t.add("RsOprf_mal_test             ", RsOprf_mal_test);
        t.add("RsOprf_reduced_test         ", RsOprf_reduced_test);
                   
#ifdef VOLE_PSI_ENABLE_OPPRF
        t.add("RsOpprf_eval_blk_test       ", RsOpprf_eval_blk_test);
        t.add("RsOpprf_eval_u8_test        ", RsOpprf_eval_u8_test);

        t.add("RsOpprf_eval_blk_mtx_test   ", RsOpprf_eval_blk_mtx_test);
        t.add("RsOpprf_eval_u8_mtx_test    ", RsOpprf_eval_u8_mtx_test);
#endif

        t.add("Psi_Rs_empty_test           ", Psi_Rs_empty_test);
        t.add("Psi_Rs_partial_test         ", Psi_Rs_partial_test);
        t.add("Psi_Rs_full_test            ", Psi_Rs_full_test);
        t.add("Psi_Rs_reduced_test         ", Psi_Rs_reduced_test);
        t.add("Psi_Rs_multiThrd_test       ", Psi_Rs_multiThrd_test);
        t.add("Psi_Rs_mal_test             ", Psi_Rs_mal_test);
                                           
#ifdef VOLE_PSI_ENABLE_CPSI
        t.add("Cpsi_Rs_empty_test          ", Cpsi_Rs_empty_test);
        t.add("Cpsi_Rs_partial_test        ", Cpsi_Rs_partial_test);
        t.add("Cpsi_Rs_full_test           ", Cpsi_Rs_full_test);
        t.add("Cpsi_Rs_full_asym_test      ", Cpsi_Rs_full_asym_test);
        t.add("Cpsi_Rs_full_add32_test     ", Cpsi_Rs_full_add32_test);
#endif

        t.add("filebase_readSet_Test       ", filebase_readSet_Test);
        t.add("filebase_psi_bin_Test       ", filebase_psi_bin_Test);
        t.add("filebase_psi_csv_Test       ", filebase_psi_csv_Test);
        t.add("filebase_psi_csvh_Test      ", filebase_psi_csvh_Test);

    });
}

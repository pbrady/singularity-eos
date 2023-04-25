//------------------------------------------------------------------------------
// © 2021-2023. Triad National Security, LLC. All rights reserved.  This
// program was produced under U.S. Government contract 89233218CNA000001
// for Los Alamos National Laboratory (LANL), which is operated by Triad
// National Security, LLC for the U.S.  Department of Energy/National
// Nuclear Security Administration. All rights in the program are
// reserved by Triad National Security, LLC, and the U.S. Department of
// Energy/National Nuclear Security Administration. The Government is
// granted for itself and others acting on its behalf a nonexclusive,
// paid-up, irrevocable worldwide license in this material to reproduce,
// prepare derivative works, distribute copies to the public, perform
// publicly and display publicly, and to permit others to do so.
//------------------------------------------------------------------------------

#include <ports-of-call/portability.hpp>
#include <singularity-eos/closure/mixed_cell_models.hpp>
#include <singularity-eos/eos/eos.hpp>
#include <singularity-eos/eos/get_sg_eos.hpp>
#include <singularity-eos/eos/get_sg_eos_lambdas.hpp>

namespace singularity {
void get_sg_eos_rho_e(const char *name, int ncell, int nmat, indirection_v &offsets_v,
                      indirection_v &eos_offsets_v, Kokkos::View<EOS *, Llft> &eos_v,
                      dev_v &press_v, dev_v &pmax_v, dev_v &vol_v, dev_v &spvol_v,
                      dev_v &sie_v, dev_v &temp_v, dev_v &bmod_v, dev_v &dpde_v,
                      dev_v &cv_v, dev_frac_v &frac_mass_v, dev_frac_v &frac_vol_v,
                      dev_frac_v &frac_ie_v, dev_frac_v &frac_bmod_v,
                      dev_frac_v &frac_dpde_v, dev_frac_v &frac_cv_v,
                      ScratchV<int> &pte_idxs, ScratchV<int> &pte_mats,
                      ScratchV<double> &press_pte, ScratchV<double> &vfrac_pte,
                      ScratchV<double> &rho_pte, ScratchV<double> &sie_pte,
                      ScratchV<double> &temp_pte, ScratchV<double> &solver_scratch,
                      Kokkos::Experimental::UniqueToken<DES, KGlobal> &tokens,
                      bool small_loop, bool do_frac_bmod, bool do_frac_dpde,
                      bool do_frac_cv) {
  const auto init_lambda = SG_GET_SG_EOS_INIT_LAMBDA_DECL;
  const auto final_lambda = SG_GET_SG_EOS_FINAL_LAMBDA_DECL;
  portableFor(
      name, 0, ncell, PORTABLE_LAMBDA(const int &iloop) {
        // cell offset
        const int i{offsets_v(iloop) - 1};
        // get "thread-id" like thing with optimization
        // for small loops
        const int32_t token{tokens.acquire()};
        const int32_t tid{small_loop ? iloop : token};
        double mass_sum{0.0};
        int npte{0};
        // initialize values for solver / lookup
        init_lambda(i, tid, mass_sum, npte, 0.0, 1.0, 0.0);
        // get cache from offsets into scratch
        const int neq = npte + 1;
        singularity::mix_impl::CacheAccessor cache(&solver_scratch(tid, 0) +
                                                   neq * (neq + 4) + 2 * npte);
        if (npte > 1) {
          // create solver lambda
          // eos accessor
          singularity::EOSAccessor_ eos_inx(eos_v, &pte_idxs(tid, 0));
          // reset inputs
          PTESolverRhoT<singularity::EOSAccessor_, Real *, Real **> method(
              npte, eos_inx, 1.0, sie_v(i), &rho_pte(tid, 0), &vfrac_pte(tid, 0),
              &sie_pte(tid, 0), &temp_pte(tid, 0), &press_pte(tid, 0), cache,
              &solver_scratch(tid, 0));
          const bool res_{PTESolver(method)};
        } else {
          // pure cell (nmat = 1)
          // calculate sie from single eos
          Real dpdr_m, dtdr_m, dpde_m, dtde_m;
          eos_v(pte_idxs(tid, 0))
              .PTofRE(rho_pte(tid, 0), sie_pte(tid, 0), cache[0], press_pte(tid, 0),
                      temp_pte(tid, 0), dpdr_m, dpde_m, dtdr_m, dtde_m);
        }
        // assign outputs
        final_lambda(i, tid, npte, mass_sum, 1.0, 0.0, 1.0, cache);
        // assign max pressure
        pmax_v(i) = press_v(i) > pmax_v(i) ? press_v(i) : pmax_v(i);
        // release the token used for scratch arrays
        tokens.release(token);
      });
  return;
}
} // namespace singularity

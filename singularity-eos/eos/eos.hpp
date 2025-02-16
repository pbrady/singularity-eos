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

#ifndef _SINGULARITY_EOS_EOS_EOS_HPP_
#define _SINGULARITY_EOS_EOS_EOS_HPP_

#include "stdio.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

#include <ports-of-call/portability.hpp>
#include <singularity-eos/eos/eos_base.hpp>
#include <singularity-eos/eos/eos_variant.hpp>

// Base stuff
#include <singularity-eos/base/constants.hpp>
#include <singularity-eos/base/eos_error.hpp>
#include <singularity-eos/base/variadic_utils.hpp>

// EOS models
#include <singularity-eos/eos/eos_davis.hpp>
#include <singularity-eos/eos/eos_eospac.hpp>
#include <singularity-eos/eos/eos_gruneisen.hpp>
#include <singularity-eos/eos/eos_helmholtz.hpp>
#include <singularity-eos/eos/eos_ideal.hpp>
#include <singularity-eos/eos/eos_jwl.hpp>
#include <singularity-eos/eos/eos_noble_abel.hpp>
#include <singularity-eos/eos/eos_sap_polynomial.hpp>
#include <singularity-eos/eos/eos_spiner.hpp>
#include <singularity-eos/eos/eos_stellar_collapse.hpp>
#include <singularity-eos/eos/eos_stiff.hpp>
#include <singularity-eos/eos/eos_vinet.hpp>

// Modifiers
#include <singularity-eos/eos/modifiers/eos_unitsystem.hpp>
#include <singularity-eos/eos/modifiers/ramps_eos.hpp>
#include <singularity-eos/eos/modifiers/relativistic_eos.hpp>
#include <singularity-eos/eos/modifiers/scaled_eos.hpp>
#include <singularity-eos/eos/modifiers/shifted_eos.hpp>

namespace singularity {

// recreate variadic list
template <typename... Ts>
using tl = singularity::detail::type_list<Ts...>;

template <template <typename> class... Ts>
using al = singularity::detail::adapt_list<Ts...>;

// transform variadic list: applies modifiers to eos's
using singularity::detail::transform_variadic_list;

// all eos's
static constexpr const auto full_eos_list =
    tl<IdealGas, Gruneisen, Vinet, JWL, DavisReactants, DavisProducts, StiffGas,
       SAP_Polynomial, NobleAbel
#ifdef SINGULARITY_USE_SPINER_WITH_HDF5
#ifdef SINGULARITY_USE_HELMHOLTZ
       ,
       Helmholtz
#endif // SINGULARITY_USE_HELMHOLTZ
       ,
       SpinerEOSDependsRhoT, SpinerEOSDependsRhoSie, StellarCollapse
#endif // SINGULARITY_USE_SPINER_WITH_HDF5
#ifdef SINGULARITY_USE_EOSPAC
       ,
       EOSPAC
#endif // SINGULARITY_USE_EOSPAC
       >{};
// eos's that get relativistic modifier
static constexpr const auto relativistic_eos_list =
    tl<IdealGas
#ifdef SINGULARITY_USE_SPINER_WITH_HDF5
       ,
       SpinerEOSDependsRhoT, SpinerEOSDependsRhoSie, StellarCollapse
#endif // SINGULAIRTY_USE_SPINER_WITH_HDF5
       >{};
// eos's that get unit system modifier
static constexpr const auto unit_system_eos_list =
    tl<IdealGas
#ifdef SPINER_USE_HDF
       ,
       SpinerEOSDependsRhoT, SpinerEOSDependsRhoSie, StellarCollapse
#endif // SPINER_USE_HDF
#ifdef SINGULARITY_USE_EOSPAC
       ,
       EOSPAC
#endif // SINGULARITY_USE_EOSPAC
       >{};
// modifiers that get applied to all eos's
static constexpr const auto apply_to_all = al<ScaledEOS, ShiftedEOS>{};
// variadic list of UnitSystem<T>'s
static constexpr const auto unit_system =
    transform_variadic_list(unit_system_eos_list, al<UnitSystem>{});
// variadic list of Relativistic<T>'s
static constexpr const auto relativistic =
    transform_variadic_list(relativistic_eos_list, al<RelativisticEOS>{});
// variadic list of eos's with shifted or scaled modifiers
static constexpr const auto shifted_1 =
    transform_variadic_list(full_eos_list, al<ShiftedEOS>{});
static constexpr const auto scaled_1 =
    transform_variadic_list(full_eos_list, al<ScaledEOS>{});
// relativistic and unit system modifiers
static constexpr const auto unit_or_rel =
    singularity::detail::concat(unit_system, relativistic);
// variadic list of eos with shifted, relativistic or unit system modifiers
static constexpr const auto shifted_of_unit_or_rel =
    transform_variadic_list(unit_or_rel, al<ShiftedEOS>{});
// combined list of all shifted EOS
static constexpr const auto shifted =
    singularity::detail::concat(shifted_1, shifted_of_unit_or_rel);
// variadic list of eos with scaled, relativistic or unit system modifiers
static constexpr const auto scaled_of_unit_or_rel =
    transform_variadic_list(unit_or_rel, al<ScaledEOS>{});
// variadic list of Scaled<Shifted<T>>'s
static constexpr const auto scaled_of_shifted =
    transform_variadic_list(shifted, al<ScaledEOS>{});
// combined list of all scaled EOS
static constexpr const auto scaled =
    singularity::detail::concat(scaled_1, scaled_of_unit_or_rel, scaled_of_shifted);
// create combined list
static constexpr const auto combined_list_1 =
    singularity::detail::concat(full_eos_list, shifted, scaled, unit_or_rel);
// make a ramped eos of everything
static constexpr const auto ramped_all =
    transform_variadic_list(combined_list_1, al<BilinearRampEOS>{});
// final combined list
static constexpr const auto combined_list =
    singularity::detail::concat(combined_list_1, ramped_all);
// a function that returns a Variant from a typelist
template <typename... Ts>
struct tl_to_Variant_struct {
  using vt = Variant<Ts...>;
};

template <typename... Ts>
constexpr auto tl_to_Variant(tl<Ts...>) {
  return tl_to_Variant_struct<Ts...>{};
}

// create the alias
using EOS = typename decltype(tl_to_Variant(combined_list))::vt;
} // namespace singularity

#endif // _SINGULARITY_EOS_EOS_EOS_HPP_

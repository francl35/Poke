#pragma once

#include "psprecomp/runtime.hpp"

namespace mhp1 {

void install_overlay_support(psprecomp::Runtime &runtime);
bool try_install_overlay(psprecomp::Runtime &runtime, std::uint32_t pc);
void revalidate_overlays(psprecomp::Runtime &runtime);
void forget_unmatched_overlays();

} // namespace mhp1

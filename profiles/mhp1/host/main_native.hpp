#pragma once

#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"

namespace mhp1 {
void prepare_main_native(
    psprecomp::Runtime &runtime, const psprecomp::Elf32Image &elf, std::span<const psprecomp::PspImport> imports);
bool main_native_active(const psprecomp::Runtime &runtime) noexcept;
void bind_main_native_import_bridge(psprecomp::Runtime &runtime, psprecomp::Runtime::RecompiledFunction bridge);
void revalidate_main_native(psprecomp::Runtime &runtime);
void reset_main_native(psprecomp::Runtime &runtime) noexcept;
} // namespace mhp1

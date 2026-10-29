#pragma once
#include "psprecomp/runtime.hpp"
#include "gpu/ge_state.hpp"
#include <filesystem>
#include <memory>
namespace mh {
class HleRegistrar;
}
namespace mhp1 {
struct Pad {
    std::uint32_t buttons{};
    std::uint8_t x{128}, y{128};
};
class VideoBackend {
public:
    virtual ~VideoBackend() = default;
    // Backends opt in only when the renderer supports and enables raw vertices.
    virtual bool gpu_decode() const { return false; }
    virtual bool check_gpu_decode() const { return false; }
    virtual void begin_list() = 0;
    virtual void submit(const mh::gpu::DrawCall &, psprecomp::GuestMemory &) = 0;
    virtual void read_back(std::uint32_t, psprecomp::GuestMemory &) = 0;
    virtual void present(std::uint32_t, std::uint32_t, std::uint32_t, psprecomp::GuestMemory &) = 0;
    virtual bool poll() = 0;
    virtual Pad pad() = 0;
    virtual bool capture(const std::filesystem::path &) = 0;
};
std::unique_ptr<VideoBackend> make_video_backend(const std::filesystem::path &data);
void register_controller(mh::HleRegistrar &hle, std::function<Pad(psprecomp::Runtime &)> sample);
}

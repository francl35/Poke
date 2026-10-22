// PSP audio/SAS service group adapted from this project's MIT-licensed hle_media.cpp.
// MHP2G frontend, camera, loading acceleration and game hooks are not linked.
#include "hle/hle_common.hpp"
#include "audio/audio_sink.hpp"
#include "audio/guest_pcm.hpp"
#include "audio/sas_core.hpp"
#include "kernel/load_trace.hpp"
#include <algorithm>
#include <array>
#include <vector>
namespace mhp1 {
namespace {
using namespace mhp2g;
constexpr std::uint32_t kAudioSampleRate = 44100;
struct AudioChannel {
    bool reserved{};
    std::uint32_t samples{}, format{};
    std::uint64_t queued_until_us{}, cursor{};
};
struct AudioState {
    std::array<AudioChannel, 8> audio;
};
AudioState &media() {
    static AudioState state;
    return state;
}
void audio_output(Runtime &rt, AllegrexContext &ctx) {
    const std::uint32_t channel = arg(ctx, 0);
    if (channel >= media().audio.size()) {
        kernel().finish(ctx, 0x80260002u);
        return;
    }
    AudioChannel &state = media().audio[channel];
    const std::uint32_t left = arg(ctx, 1);
    const std::uint32_t right = arg(ctx, 2);
    const std::uint32_t buffer = arg(ctx, 3);
    const std::uint32_t frames = state.samples;
    // Format 0x10 is mono: one sample per frame instead of a stereo pair.
    const bool mono = (state.format & 0x10u) != 0u;

    if (buffer != 0u && frames != 0u) {
        static std::vector<std::int16_t> staging;
        if (audio::stage_guest_pcm(rt.memory(), buffer, frames, mono, staging)) {
            // How loud the buffer is after the channel's volume, worked out
            // as the sink mixes it (0x8000 is full volume): 0 means the sink
            // would add nothing but zeros.
            const std::int32_t gains[2] = {static_cast<std::int32_t>(std::min<std::uint32_t>(left, 0x8000u)),
                static_cast<std::int32_t>(std::min<std::uint32_t>(right, 0x8000u))};
            int peak = 0;
            for (std::size_t i = 0; i < staging.size(); ++i)
                peak = std::max(peak, std::abs((static_cast<std::int32_t>(staging[i]) * gains[i & 1u]) >> 15));
            load_trace::note_audio_peak(peak);
            // While a load runs faster than real time its silence is dropped:
            // played, it would pile up faster than the device plays it. So is
            // everything while the player fast-forwards: the sound is muted
            // rather than sped up. The channel's cursor stays where it was
            // and catches up with the device when sound comes back.
            const bool silent_load = false;
            if (!silent_load) audio::AudioSink::instance().mix(state.cursor, staging.data(), frames, left, right);
        } else {
            log_once("audio-buffer", "[audio] output buffer is not a single mapped range; dropping it");
        }
    }

    const std::uint64_t now = kernel().now_us();
    if (state.queued_until_us < now) state.queued_until_us = now;
    const std::uint64_t previous_end = state.queued_until_us;
    state.queued_until_us += static_cast<std::uint64_t>(frames) * 1'000'000u / kAudioSampleRate;
    if (previous_end > now)
        kernel().delay_current(ctx, previous_end - now, frames);
    else
        kernel().finish(ctx, frames);
}

// __sceSasCore renders one grain into a guest buffer; the guest then hands that
// buffer to a sceAudio channel itself, so nothing here reaches the sink.
void sas_render(Runtime &rt, std::uint32_t core, std::uint32_t output, bool mix, std::uint32_t left_volume,
    std::uint32_t right_volume) {
    audio::SasCore &sas = audio::sas_core(core);
    const std::size_t frames = sas.grain();
    static std::vector<std::int16_t> staging;
    staging.resize(frames * 2u);
    sas.render(rt.memory(), staging.data(), frames);

    std::uint8_t *destination = rt.memory().raw_pointer(output, frames * 4u);
    if (destination == nullptr) {
        log_once("sas-output", "[sas] output buffer is not a single mapped range; dropping the grain");
        return;
    }
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (std::size_t side = 0; side < 2u; ++side) {
            const std::size_t index = frame * 2u + side;
            std::int32_t value = staging[index];
            if (mix) {
                const std::uint32_t gain = side == 0u ? left_volume : right_volume;
                value = (value * static_cast<std::int32_t>(std::min(gain, 0x1000u))) >> 12;
                value += static_cast<std::int16_t>(destination[index * 2u] | (destination[index * 2u + 1u] << 8));
                value = std::clamp(value, -32768, 32767);
            }
            destination[index * 2u] = static_cast<std::uint8_t>(value);
            destination[index * 2u + 1u] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> 8u);
        }
    }
}

void register_audio_impl(HleRegistrar &hle) {
    audio::AudioSink::instance().initialize();

    hle.add("sceAudio", "sceAudioChReserve", [](Runtime &, AllegrexContext &ctx) {
        auto channel = static_cast<std::int32_t>(arg(ctx, 0));
        auto &channels = media().audio;
        if (channel < 0) {
            channel = -1;
            for (std::size_t i = 0; i < channels.size(); ++i) {
                if (!channels[i].reserved) {
                    channel = static_cast<std::int32_t>(i);
                    break;
                }
            }
        }
        if (channel < 0 || channel >= static_cast<std::int32_t>(channels.size()) ||
            channels[static_cast<std::size_t>(channel)].reserved) {
            kernel().finish(ctx, 0x80260002u);
            return;
        }
        channels[static_cast<std::size_t>(channel)] = AudioChannel{true, arg(ctx, 1), arg(ctx, 2), 0u, 0u};
        kernel().finish(ctx, static_cast<std::uint32_t>(channel));
    });
    hle.add("sceAudio", "sceAudioChRelease", [](Runtime &, AllegrexContext &ctx) {
        if (arg(ctx, 0) < media().audio.size()) media().audio[arg(ctx, 0)].reserved = false;
        kernel().finish(ctx, 0u);
    });
    hle.add("sceAudio", "sceAudioSetChannelDataLen", [](Runtime &, AllegrexContext &ctx) {
        if (arg(ctx, 0) < media().audio.size()) media().audio[arg(ctx, 0)].samples = arg(ctx, 1);
        kernel().finish(ctx, 0u);
    });
    hle.add("sceAudio", "sceAudioChangeChannelConfig", [](Runtime &, AllegrexContext &ctx) {
        if (arg(ctx, 0) < media().audio.size()) media().audio[arg(ctx, 0)].format = arg(ctx, 1);
        kernel().finish(ctx, 0u);
    });
    hle.add(
        "sceAudio", "sceAudioChangeChannelVolume", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0u); });
    // How much of what the channel was given is still to play, in samples.
    hle.add("sceAudio", "sceAudioGetChannelRestLength", [](Runtime &, AllegrexContext &ctx) {
        const std::uint32_t channel = arg(ctx, 0);
        if (channel >= media().audio.size()) {
            kernel().finish(ctx, 0x80260002u);
            return;
        }
        const std::uint64_t now = kernel().now_us();
        const AudioChannel &state = media().audio[channel];
        const std::uint64_t remaining = state.queued_until_us > now ? state.queued_until_us - now : 0u;
        kernel().finish(ctx, static_cast<std::uint32_t>(remaining * kAudioSampleRate / 1'000'000u));
    });
    hle.add("sceAudio", "sceAudioOutputPannedBlocking", audio_output);
    hle.try_add("sceAudio", "sceAudioOutputPanned", audio_output);

    hle.add("sceSasCore", "__sceSasInit", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).init(arg(ctx, 1), arg(ctx, 2), arg(ctx, 3));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceSasCore", "__sceSasSetVoice", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).set_voice(arg(ctx, 1), arg(ctx, 2), arg(ctx, 3), arg(ctx, 4) != 0u);
        kernel().finish(ctx, 0u);
    });
    hle.try_add("sceSasCore", "__sceSasSetVoicePCM", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0))
            .set_voice_pcm(arg(ctx, 1), arg(ctx, 2), arg(ctx, 3), static_cast<std::int32_t>(arg(ctx, 4)));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceSasCore", "__sceSasSetPitch", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).set_pitch(arg(ctx, 1), arg(ctx, 2));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceSasCore", "__sceSasSetVolume", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0))
            .set_volume(arg(ctx, 1), static_cast<std::int32_t>(arg(ctx, 2)), static_cast<std::int32_t>(arg(ctx, 3)));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceSasCore", "__sceSasSetSimpleADSR", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).set_simple_adsr(arg(ctx, 1), arg(ctx, 2), arg(ctx, 3));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceSasCore", "__sceSasSetKeyOn", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).key_on(arg(ctx, 1));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceSasCore", "__sceSasSetKeyOff", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).key_off(arg(ctx, 1));
        kernel().finish(ctx, 0u);
    });
    hle.try_add("sceSasCore", "__sceSasSetPause", [](Runtime &, AllegrexContext &ctx) {
        audio::sas_core(arg(ctx, 0)).set_pause(arg(ctx, 1), arg(ctx, 2) != 0u);
        kernel().finish(ctx, 0u);
    });
    hle.try_add("sceSasCore", "__sceSasGetEnvelopeHeight", [](Runtime &, AllegrexContext &ctx) {
        kernel().finish(ctx, static_cast<std::uint32_t>(audio::sas_core(arg(ctx, 0)).envelope_height(arg(ctx, 1))));
    });
    // Reverb is not modelled, so the sends are accepted and dropped.
    for (const char *name : {"__sceSasRevType", "__sceSasRevParam", "__sceSasRevEVOL", "__sceSasRevVON"})
        hle.add("sceSasCore", name, [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0u); });
    hle.add("sceSasCore", "__sceSasGetOutputmode",
        [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, audio::sas_core(arg(ctx, 0)).output_mode()); });
    hle.add("sceSasCore", "__sceSasGetEndFlag",
        [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, audio::sas_core(arg(ctx, 0)).end_flag()); });
    hle.add("sceSasCore", "__sceSasCore", [](Runtime &rt, AllegrexContext &ctx) {
        sas_render(rt, arg(ctx, 0), arg(ctx, 1), false, 0u, 0u);
        kernel().finish(ctx, 0u);
    });
    hle.try_add("sceSasCore", "__sceSasCoreWithMix", [](Runtime &rt, AllegrexContext &ctx) {
        sas_render(rt, arg(ctx, 0), arg(ctx, 1), true, arg(ctx, 2), arg(ctx, 3));
        kernel().finish(ctx, 0u);
    });
}

}
void register_audio_services(mhp2g::HleRegistrar &hle) {
    register_audio_impl(hle);
}
}

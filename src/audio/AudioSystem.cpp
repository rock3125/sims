#include "audio/AudioSystem.hpp"

#include <SDL2/SDL.h>

#include <cmath>
#include <cstdio>

namespace sims {
namespace audio {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Synthesize `seconds` of a tone into interleaved stereo float32 samples.
// `freq_fn(t)` returns the instantaneous frequency at time t (allows chirps).
// `env_fn(t)` returns amplitude [0,1] (envelope).
template <typename FreqFn, typename EnvFn>
void synth(std::vector<float>& out, const SDL_AudioSpec& spec, double seconds,
           FreqFn freq_fn, EnvFn env_fn) {
    const std::size_t frames = static_cast<std::size_t>(seconds * spec.freq);
    out.clear();
    out.reserve(frames * 2);
    const double dt = 1.0 / static_cast<double>(spec.freq);
    double phase = 0.0;
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) * dt;
        const double f = freq_fn(t);
        phase += 2.0 * kPi * f * dt;
        const double a = env_fn(t);
        const float sample = static_cast<float>(std::sin(phase) * a);
        out.push_back(sample);
        out.push_back(sample);
    }
}

} // namespace

AudioSystem::~AudioSystem() { shutdown(); }

bool AudioSystem::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "[audio] SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = nullptr; // use SDL_QueueAudio

    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0);
    if (!device_) {
        std::fprintf(stderr, "[audio] SDL_OpenAudioDevice failed: %s (audio disabled)\n",
                     SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(device_, 0); // start playback

    buffers_.resize(static_cast<std::size_t>(Sfx::kCount));
    for (std::size_t i = 0; i < buffers_.size(); ++i) {
        generate(static_cast<Sfx>(i), buffers_[i]);
    }
    return true;
}

void AudioSystem::shutdown() {
    if (device_) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    buffers_.clear();
}

void AudioSystem::generate(Sfx s, Buffer& out) {
    switch (s) {
        case Sfx::Click: {
            // 880Hz sine, 60ms, fast decay.
            synth(out.samples, spec_, 0.06,
                  [](double) { return 880.0; },
                  [](double t) {
                      double e = 1.0 - t / 0.06;
                      return e < 0.0 ? 0.0 : e * 0.4;
                  });
            break;
        }
        case Sfx::Complete: {
            // 660Hz -> 990Hz chirp over 160ms.
            synth(out.samples, spec_, 0.16,
                  [](double t) { return 660.0 + (990.0 - 660.0) * (t / 0.16); },
                  [](double t) {
                      double e = 1.0 - t / 0.16;
                      return (e < 0.0 ? 0.0 : e) * 0.5;
                  });
            break;
        }
        case Sfx::Cancel: {
            // 220Hz buzzy tone, 140ms.
            synth(out.samples, spec_, 0.14,
                  [](double) { return 220.0; },
                  [](double t) {
                      double e = 1.0 - t / 0.14;
                      return (e < 0.0 ? 0.0 : e) * 0.45;
                  });
            break;
        }
        default: break;
    }
}

void AudioSystem::queue(SDL_AudioDeviceID dev, const Buffer& b,
                        const SDL_AudioSpec& spec) {
    const std::uint32_t bytes = static_cast<std::uint32_t>(
        b.samples.size() * sizeof(float));
    if (bytes == 0) return;
    // Avoid unbounded queue growth: skip if >1s of audio pending.
    std::uint32_t queued = SDL_GetQueuedAudioSize(dev);
    std::uint32_t one_sec = static_cast<std::uint32_t>(
        static_cast<double>(spec.freq) * spec.channels * sizeof(float));
    if (queued > one_sec) return;
    SDL_QueueAudio(dev, b.samples.data(), bytes);
}

void AudioSystem::play(Sfx s) {
    if (!enabled_ || !device_) return;
    const auto idx = static_cast<std::size_t>(s);
    if (idx >= buffers_.size()) return;
    queue(device_, buffers_[idx], spec_);
}

} // namespace audio
} // namespace sims

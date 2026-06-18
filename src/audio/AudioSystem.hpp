#pragma once

#include <SDL2/SDL.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sims {
namespace audio {

// Phase 8: lightweight procedural sound effects via raw SDL2 audio. No
// external audio files needed — SFX are synthesized at init time as float32
// PCM and queued to a single shared output device on demand.
//
// One device is opened (44.1kHz stereo float32). `play` queues a pre-rendered
// buffer; if the device is saturated the call is a no-op (audio is best-
// effort and never blocks the sim thread).
class AudioSystem {
public:
    enum class Sfx : std::uint8_t {
        Click,     // short UI blip (pie-menu pick, autonomy decision)
        Complete,  // rising two-tone (action finished)
        Cancel,    // low buzz (user cancel / abort)
        kCount
    };

    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // Initializes the SDL audio subsystem + opens the output device. On
    // failure (no audio device / headless) the system silently degrades to
    // a no-op so the app still runs. Returns false only on hard SDL errors.
    bool init();

    void shutdown();

    void play(Sfx s);

    bool enabled() const { return enabled_; }
    void set_enabled(bool e) { enabled_ = e; }

private:
    struct Buffer {
        std::vector<float> samples; // interleaved stereo float32
    };

    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec spec_{};
    bool enabled_ = true;
    std::vector<Buffer> buffers_; // indexed by Sfx

    void generate(Sfx s, Buffer& out);
    static void queue(SDL_AudioDeviceID dev, const Buffer& b,
                      const SDL_AudioSpec& spec);
};

} // namespace audio
} // namespace sims

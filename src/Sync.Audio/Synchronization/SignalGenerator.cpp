#include "SignalGenerator.h"

namespace Sync::Audio {

    void SignalGenerator::Initialize(uint32_t sampleRate, uint32_t channels) {
        m_sampleRate = (sampleRate > 0) ? sampleRate : 48000;
        m_channels = (channels > 0) ? channels : 2;
        m_samplePhase = 0;
    }

    void SignalGenerator::SetSignalType(TestSignalType type) {
        m_signalType = type;
        m_samplePhase = 0;
    }

    bool SignalGenerator::Generate(float* buffer, size_t frames) {
        if (!buffer || frames == 0 || m_signalType == TestSignalType::None) {
            return false;
        }

        const double twoPi = 6.28318530717958647692;

        for (size_t f = 0; f < frames; ++f) {
            float sample = 0.0f;
            const uint64_t phase = m_samplePhase + f;

            switch (m_signalType) {
                case TestSignalType::PeriodicClick: {
                    // Click every 500ms (repeat period = 0.5 * sampleRate)
                    const uint64_t period = m_sampleRate / 2;
                    const uint64_t subPhase = phase % period;
                    const uint64_t clickLength = (m_sampleRate * 2) / 1000; // 2ms click
                    if (subPhase < clickLength) {
                        // Windowed 2 kHz sine burst
                        const double t = static_cast<double>(subPhase) / static_cast<double>(clickLength);
                        const double window = 0.5 * (1.0 - std::cos(twoPi * t));
                        sample = static_cast<float>(0.8 * window * std::sin(twoPi * 2000.0 * (static_cast<double>(subPhase) / m_sampleRate)));
                    }
                    break;
                }

                case TestSignalType::LogarithmicChirp: {
                    // Chirp every 1000ms
                    const uint64_t period = m_sampleRate;
                    const uint64_t subPhase = phase % period;
                    const uint64_t chirpLength = (m_sampleRate * 50) / 1000; // 50ms chirp
                    if (subPhase < chirpLength) {
                        const double t = static_cast<double>(subPhase) / m_sampleRate;
                        const double T = static_cast<double>(chirpLength) / m_sampleRate;
                        const double f0 = 200.0;
                        const double f1 = 8000.0;
                        // Logarithmic sweep
                        const double k = (T * f0) / std::log(f1 / f0);
                        const double instantaneousPhase = twoPi * k * (std::pow(f1 / f0, t / T) - 1.0);
                        // Hann window
                        const double window = 0.5 * (1.0 - std::cos(twoPi * (subPhase / static_cast<double>(chirpLength))));
                        sample = static_cast<float>(0.75 * window * std::sin(instantaneousPhase));
                    }
                    break;
                }

                case TestSignalType::DualPulse: {
                    // Two pulses separated by 100ms, repeating every 1000ms
                    const uint64_t period = m_sampleRate;
                    const uint64_t subPhase = phase % period;
                    const uint64_t pulseLen = (m_sampleRate * 1) / 1000; // 1ms
                    const uint64_t pulse2Start = (m_sampleRate * 100) / 1000; // 100ms
                    if (subPhase < pulseLen) {
                        const double t = static_cast<double>(subPhase) / pulseLen;
                        sample = static_cast<float>(0.85 * (1.0 - t));
                    } else if (subPhase >= pulse2Start && subPhase < (pulse2Start + pulseLen)) {
                        const double t = static_cast<double>(subPhase - pulse2Start) / pulseLen;
                        sample = static_cast<float>(0.85 * (1.0 - t));
                    }
                    break;
                }

                case TestSignalType::SineTone: {
                    // Continuous 1000 Hz reference tone (-6 dBFS = 0.5 amplitude)
                    const double t = static_cast<double>(phase) / m_sampleRate;
                    sample = static_cast<float>(0.5 * std::sin(twoPi * 1000.0 * t));
                    break;
                }

                default:
                    sample = 0.0f;
                    break;
            }

            for (uint32_t ch = 0; ch < m_channels; ++ch) {
                buffer[f * m_channels + ch] = sample;
            }
        }

        m_samplePhase += frames;
        return true;
    }

    std::vector<float> SignalGenerator::GenerateReferenceChirp(uint32_t sampleRate, double durationSec, double f0, double f1) {
        const size_t totalFrames = static_cast<size_t>(sampleRate * durationSec);
        std::vector<float> chirp(totalFrames);
        const double twoPi = 6.28318530717958647692;
        const double k = (durationSec * f0) / std::log(f1 / f0);

        for (size_t i = 0; i < totalFrames; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const double instantaneousPhase = twoPi * k * (std::pow(f1 / f0, t / durationSec) - 1.0);
            const double window = 0.5 * (1.0 - std::cos(twoPi * (static_cast<double>(i) / totalFrames)));
            chirp[i] = static_cast<float>(window * std::sin(instantaneousPhase));
        }

        return chirp;
    }

} // namespace Sync::Audio

#pragma once

// Library initialization is NOT proof of an authenticated, ready region circuit.
enum class TasiaTransportStatus { Disconnected, SecondLife, Disabled, Unavailable, NotRouted, Active };

inline TasiaTransportStatus tasiaTransportStatus(bool connected, bool second_life,
    bool enabled, bool initialized, bool circuit_ready = false)
{
    if (!connected) return TasiaTransportStatus::Disconnected;
    if (second_life) return TasiaTransportStatus::SecondLife;
    if (!enabled) return TasiaTransportStatus::Disabled;
    if (!initialized) return TasiaTransportStatus::Unavailable;
    if (circuit_ready) return TasiaTransportStatus::Active;
    return TasiaTransportStatus::NotRouted;
}

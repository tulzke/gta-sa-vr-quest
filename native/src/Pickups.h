#pragma once
#include <cstdint>
namespace savr::xr { struct HandPose; }
namespace savr::vehicledoors { struct MarkerFrame; }

// World pickups retain the retail lifecycle and rise near the player. New
// weapons require a fresh one-second grip at the shared nearest target;
// identical owned weapon types remain walk-over ammunition sources.
namespace savr::pickups {

// Resolve CPickup::Update and install the rise/contact hooks. Call once,
// after ResolveGameSymbols has succeeded (pass the same dlopen handle).
void Install(void* handle);

// Per-gameplay-frame follow-up: retries putting a just-collected weapon into
// the hand that physically grabbed its pickup (the game grants weapons a few
// frames later via GiveDelayedWeapon).
void Tick();
// Probe current retail pool objects, before any subsystem acquires a hand.
void ProbeGrabs(const xr::HandPose hands[2]);
void FinishProbes();
void Append(vehicledoors::MarkerFrame& frame);
void Reset();
unsigned ClaimedHands();

}  // namespace savr::pickups

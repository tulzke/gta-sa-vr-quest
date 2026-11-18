#include "CameraPresence.h"

using savr::camerapresence::Update;
using savr::camerapresence::View;
using savr::camerapresence::ViewOf;

constexpr auto starting = Update(0, View::Player, false);
static_assert(ViewOf(starting) == View::Unavailable);

constexpr auto playing = Update(starting, View::Player, true);
static_assert(ViewOf(playing) == View::Player);
static_assert(Update(playing, View::Player, true) == playing);

// A takeover invalidates the last pair immediately, even without a new copy.
constexpr auto scene = Update(playing, View::Scene, false);
static_assert(ViewOf(scene) == View::Scene);
static_assert(Update(scene, View::Player, false) == scene);

// A fast return cannot re-admit a stereo pair from before the takeover.
constexpr auto resumed = Update(scene, View::Player, true);
static_assert(ViewOf(resumed) == View::Player);
static_assert(resumed != playing);
static_assert(Update(resumed, View::Scene, true) != scene);

constexpr auto paused = Update(resumed, View::Unavailable, false);
static_assert(ViewOf(paused) == View::Unavailable);
static_assert(Update(paused, View::Player, false) == paused);

int main() {}

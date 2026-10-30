#pragma once

namespace savr::boatentry {
void Install(void* gameHandle);
// Called once on a permitted ENTER edge. Returns true when a close boat owns
// the press, including a locked/occupied boat; false preserves normal entry.
bool TryEnter();
// Instant front-seat placement: -1 driver, 0 front passenger, 1 rear left, 2 rear right.
bool SeatPlayer(void* vehicle,int passenger);
}

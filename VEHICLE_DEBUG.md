# Vehicle setup and debugging

The fork provides two related tools:

- **VEHICLE DEBUG** adds in-world editing handles to supported vehicle elements.
  It can be used in the normal game world as well as on the test area.
- **VEHICLE TEST AREA** is the vehicle testing area. It supplies different car
  models and automatically
  enables VEHICLE DEBUG while the area is active.

This fork is WIP. Steering wheels, seating positions, and other interactive
elements still need calibration on most cars. A setting belongs to a vehicle
model, so adjusting one car does not configure every other model.

## Enabling the editor

Open the VR menu and set **VEHICLE DEBUG** to **ON** in the main menu. Close the
menu to interact with the editing handles. Stop the vehicle before adjusting
its cabin controls; those edits require the player to occupy the driver's seat.
Use first-person view with the virtual control geometry available.

## Basic controls

1. Bring an empty controller close to the element or its editing marker.
2. Press and hold the **trigger** to start editing. Either hand can be used.
   Release the trigger before starting another edit; approaching with it already
   held does not start a new grab.
3. Move the controller to reposition the selected element. For elements with
   orientation editing, rotate the controller as well.
4. Release the trigger to finish and save the calibration.

This is a nearby hand interaction, rather than a distant pointing ray. Small
controls such as the ignition key and radio knob require the hand to be closer
than larger markers. A short vibration confirms an editing grab.

**Grip** retains its normal gameplay purpose: opening doors, holding the wheel,
turning the key, operating the radio, or holding a climbing pipe. Use the
**trigger** to change their placement. Release any normal grab before editing.

## Editable elements

| Element | How to adjust it |
| --- | --- |
| Steering-wheel position | From the driver's seat, grab the marker at the wheel center with the trigger and move it sideways, vertically, or forward/backward. |
| Steering-wheel size | Keep the first editing trigger held, bring the other hand near the rim, and press its trigger. Move the hands apart or together to change the radius. |
| Steering-wheel tilt | Begin the two-hand edit with the hands arranged mainly above and below one another. Tilt the line between them forward/backward to change wheel pitch. Release the second trigger to return to moving the wheel. |
| Driver seating/view position | Grab the neutral eye-position marker and move it in three axes. The marker previews the change; the camera position is applied when the trigger is released. This adjusts the driving viewpoint, rather than moving the vehicle's seat mesh. |
| Boarding and seat-switch markers | Grab a seat/steering-wheel icon used for boarding or switching seats and move it. These interaction targets are separate from the seated viewpoint; moving one does not adjust the other. |
| Door grab points | Grab the door-handle marker and move it. Outside and inside grab points have separate calibration. |
| Interior door-panel grab areas | Move the corner handles to adjust the panel's bounds and depth placement. The side handles adjust its thickness. These change the interaction volume, rather than the door mesh. |
| Ignition key | Grab the key itself with the trigger, then move and rotate the controller to set its position and orientation. Grip operates the ignition instead. |
| Radio control | Grab the radio knob with the trigger to move and rotate its assembly. Grip turns the knob to switch stations instead. |
| Rear-view mirror | Grab the mirror editing handle to move and rotate its surface. Mirror quality and field of view are separate settings under VEHICLE SETTINGS → CARS. |
| Rear camera | Inside the car, use the camera proxy handle beside the mirror: 40 cm to its right and 20 cm below it. Moving or rotating it changes the rear camera. Outside a stationary car, the camera can also be adjusted using the arrow at its actual position. |
| Climbing pipes | On supported high vehicles, move the center handle to reposition a whole pipe. Move its upper or lower endpoint to change its length and angle. Each side is configured independently; there is no standing-platform editor. |
| Motorcycle handlebar grip points | On supported bikes, move the individual grip targets with the trigger. They can be adjusted from the rider's position, or outside an unoccupied bike. |

Available handles depend on vehicle type, model, view, and player position.
The cabin viewpoint editor currently requires the driver's seat; independent
passenger-viewpoint editing is not provided by this mode.

Door grab-point and panel edits are mirrored to the matching door on the other
side of the same row. Check both sides after adjusting either one.

## Using VEHICLE TEST AREA

1. Stand on foot outdoors in free roam, outside a mission.
2. Open **VR MENU → VEHICLE SETTINGS**, select **CARS**, and enable
   **VEHICLE TEST AREA**.
3. Close the menu to let the area load and move the player there. VEHICLE DEBUG
   is enabled automatically.
4. Approach a vehicle and use the editor as described above. Board it when
   adjusting the steering wheel, driving viewpoint, key, radio, or mirror.
5. To load another group of vehicles, leave the vehicle and release all editing
   and interaction grabs. Change **TEST PAGE** in the same menu, then close it.
   Alternatively, bring a free hand to one of the navigation arrows beside the
   player and press the trigger to select the previous or next page.
6. To leave, get out of the vehicle, return to the area if necessary, toggle
   **VEHICLE TEST AREA** off, and close the menu. The normal exit returns the
   player near the original location and restores the previous editor state.

The area contains up to **10 vehicles per page**, drawn from the game's
automobile and monster-truck model types. It is not a motorcycle, boat, or
aircraft catalogue. Pages wrap around; unavailable models can be skipped.
**TEST PAGE** shows the page, model-ID range, and skipped-model count.

Page changes replace the test vehicles. They require the player to be on foot,
the area vehicles to be unoccupied, and active interactions to be released.
Requests are processed with the VR menu closed. Labels such as **CLOSE MENU**,
**LEAVE VEHICLE FIRST**, **RELEASE EDITOR FIRST**, and **LOADING PAGE** explain
why a request is pending or unavailable.

Game saving is blocked while the area is active. Leave the area before making
a normal game save; calibration settings are saved separately.

## Saving and checking calibration

Calibration is saved to `vr_driving.ini` in the game's files directory:

```text
/sdcard/Android/data/com.rockstargames.gtasa/files/vr_driving.ini
```

Release the trigger to finish each edit. Valid changes are also saved when an
edit is interrupted; interruption is not an undo command. Model-specific
settings can additionally distinguish driving mode, seat, door, or side,
depending on the element. Restarting the game should retain saved calibration.

After adjustment, turn VEHICLE DEBUG off and check normal interactions: reach
the control with each hand, open and close the door, board and leave, and drive
briefly. Confirm placement and comfort in Quest, including whether nearby
controls interfere with one another. Calibrating one model does not confirm
the placement on other models.

If the test area fails to load, its diagnostic log is
`vr_vehicle_test_area.log` in the same game files directory. The menu also
reports loading, floor, entity-pool, and return-position errors.

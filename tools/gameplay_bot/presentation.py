"""Reject stale compositor pixels when native presentation visibly changes."""
import hashlib
import math
import time

from .core import BotFault


def pixels(path):
    from PIL import Image
    with Image.open(path) as image:
        return hashlib.sha256(image.convert("RGB").tobytes()).hexdigest()


class PresentationGuard:
    def __init__(self, clock=time.monotonic, max_frozen_seconds=2.):
        self.previous = None
        self.clock = clock
        self.max_frozen_seconds = max_frozen_seconds
        self.eye_changed_at = None
        self.head_loss = None

    def expect_head_tracking_loss(self, mask, milliseconds):
        if (type(mask) is not int or not 1 <= mask <= 3
                or type(milliseconds) is not int or not 100 <= milliseconds <= 8000):
            raise BotFault("Expected head loss requires the bounded native diagnostic lease")
        self.head_loss = mask, self.clock() + milliseconds / 1000

    def expected_head_loss(self, state, now):
        controls = state.get("controls") or {}
        physical, sticks = controls.get("physical"), controls.get("sticks")
        if not isinstance(physical, list) or not isinstance(sticks, list):
            return False
        neutral = physical + sticks
        return (self.head_loss is not None and now < self.head_loss[1]
                and type(state.get("head_tracking_fault_mask")) is int
                and state["head_tracking_fault_mask"] == self.head_loss[0]
                and state.get("camera_active") is True and state.get("camera_suspended") is True
                and type(state.get("camera_reason")) is int and state["camera_reason"] == 2
                and controls.get("native_buttons") == 0 and len(neutral) == 15
                and all(type(value) in (int, float) and math.isfinite(value) and abs(value) <= .08
                        for value in neutral))

    def awaiting_head_recovery(self, predicate, state):
        # A deliberately armed seven-second loss is not an unexpected menu
        # transition after two seconds. Wait only inside that exact lease,
        # with the same already-reviewed owner and released controls.
        return (predicate.get("camera_active") is True
                and predicate.get("camera_suspended") is False
                and type(predicate.get("head_tracking_fault_mask")) is int
                and predicate["head_tracking_fault_mask"] == 0
                and self.previous is not None
                and not any(state.get(key) for key in ("title", "loading", "demo", "pause", "gamepad"))
                and not (state.get("native") or {}).get("popup")
                and self.context(state) == self.previous[1]
                and self.expected_head_loss(state, self.clock()))

    @staticmethod
    def context(state):
        values = tuple(state.get(name) for name in
                       ("title", "loading", "demo", "menu", "idroid", "camera_active", "activation"))
        return values + ((state.get("rendered") or {}).get("binocular_held"),
                         (state.get("controls") or {}).get("context"),
                         (state.get("rendered") or {}).get("presentation_epoch"),
                         (state.get("rendered") or {}).get("player_owner"),
                         (state.get("native") or {}).get("mission"),
                         (state.get("native") or {}).get("sequence"))

    @staticmethod
    def palms(state):
        rendered = state.get("rendered") or {}
        return {hand: rendered.get(hand + "_palm") for hand in ("left", "right")
                if rendered.get(hand + "_palm_tracked", True)}

    @staticmethod
    def moved(palm, previous):
        if not palm or not previous:
            return False
        translation = math.dist(palm["position"], previous["position"])
        rotation = abs(sum(a*b for a,b in zip(palm["orientation"], previous["orientation"])))
        return translation > .02 or rotation < .999

    def admit(self, captures, state):
        if len(captures) != 2:
            raise BotFault("Both compositor eyes are required")
        hashes = tuple(pixels(path) for path in captures)
        context = self.context(state)
        palms = self.palms(state)
        native = state.get("native", {})
        location = [native.get("player_" + axis) for axis in "xyz"]
        if any(type(value) not in (int, float) or not math.isfinite(value) for value in location):
            location = None
        changed = False
        now = self.clock()
        expected_loss = self.expected_head_loss(state, now)
        if self.previous:
            old_hashes, old_context, old_palms, old_location = self.previous
            changed = context != old_context
            if location and old_location and not state.get("menu") and not expected_loss:
                changed = changed or math.dist(location, old_location) > .025
            if state.get("camera_active"):
                changed = changed or any(self.moved(palm, old_palms.get(hand)) for hand,palm in palms.items())
            if changed and any(a == b for a,b in zip(hashes, old_hashes)):
                return False
            changed_at = [now if a != b else previous for a,b,previous in
                          zip(hashes, old_hashes, self.eye_changed_at)]
            # Only an explicitly armed, still-active native negative fixture
            # permits frozen surroundings. Context/ownership and hand changes
            # above still require new pixels. This is continuity evidence,
            # never a claim that gameplay produced fresh images during loss.
            if expected_loss:
                changed_at = [now, now]
            # Stationary gameplay still animates the hands/world. The reproduced
            # high-resolution failure kept both final eyes byte-identical while
            # native stereo continued at 90 Hz, without a menu/context change.
            gameplay = state.get("camera_active") and not any(state.get(k) for k in
                           ("title", "loading", "demo", "menu"))
            if gameplay and any(now - previous >= self.max_frozen_seconds for previous in changed_at):
                return False
        else:
            changed_at = [now, now]
        self.eye_changed_at = changed_at
        self.previous = hashes, context, palms, location
        return True

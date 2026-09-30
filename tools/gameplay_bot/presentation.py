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

    @staticmethod
    def context(state):
        values = tuple(state.get(name) for name in
                       ("title", "loading", "demo", "menu", "idroid", "camera_active", "activation"))
        return values + ((state.get("rendered") or {}).get("binocular_held"),
                         (state.get("controls") or {}).get("context"))

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
        if self.previous:
            old_hashes, old_context, old_palms, old_location = self.previous
            changed = context != old_context
            if location and old_location and not state.get("menu"):
                changed = changed or math.dist(location, old_location) > .025
            if state.get("camera_active"):
                changed = changed or any(self.moved(palm, old_palms.get(hand)) for hand,palm in palms.items())
            if changed and any(a == b for a,b in zip(hashes, old_hashes)):
                return False
            changed_at = [now if a != b else previous for a,b,previous in
                          zip(hashes, old_hashes, self.eye_changed_at)]
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

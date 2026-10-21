"""Observe a complete bounded interval, including brief presentation failures."""
import math
import time

from .core import BotFault, matches


def observe_interval(live, step, *, clock=time.monotonic, sleep=time.sleep):
    seconds, predicate = step.get('seconds'), step.get('state_during')
    if (type(seconds) not in (int, float) or not math.isfinite(seconds)
            or not .1 <= seconds <= 15 or not isinstance(predicate, dict)
            or not predicate or live.held):
        raise BotFault('Interval observation requires released input, explicit invariants and .1..15 seconds')
    native = any(key.startswith('native.') for key in predicate)
    started, samples = clock(), 0
    while True:
        state = live.observe(native=native)
        elapsed = clock() - started
        samples += 1
        live.events.emit('interval_sample', elapsed_seconds=elapsed, state=state)
        if not matches(state, predicate):
            raise BotFault('Presentation invariant failed during bounded observation')
        if elapsed >= seconds:
            break
        sleep(min(.1, seconds - elapsed))
    result = {'observed_seconds': elapsed, 'samples': samples}
    live.events.emit('interval_complete', **result)
    return result

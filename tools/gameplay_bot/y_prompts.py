"""Recognize learned player-visible Y glyphs from owned final-eye pixels.

Glyphs classify an affordance, never an exact native target or action result.
The private catalog retains exact owned image hashes and reviewed crop bounds.
No UI text history, cached prompt string or input acknowledgement is current UI.
"""
import hashlib
import json
import math
from pathlib import Path

from .core import BotFault


def _mask(rgb):
    import numpy as np
    a = np.asarray(rgb, dtype=np.int16)
    return ((a[:, :, 0] > 165) & (a[:, :, 1] > 190) & (a[:, :, 2] > 190) &
            ((a.max(axis=2)-a.min(axis=2)) < 65)).astype(np.uint8)


def _best_match(mask, template):
    import numpy as np
    h, w = template.shape; ih, iw = mask.shape
    if h > ih or w > iw or int(template.sum()) < 40:
        return None
    # Exact translated foreground intersection and foreground-count precision.
    # Background-only white scenery cannot pass recall plus precision together.
    shape = (ih-h+1, iw-w+1)
    intersection = np.zeros(shape, dtype=np.uint16)
    for y, x in zip(*np.nonzero(template)):
        intersection += mask[y:y+shape[0], x:x+shape[1]]
    integral = np.pad(mask.astype(np.int32), ((1, 0), (1, 0))).cumsum(0).cumsum(1)
    count = integral[h:, w:]-integral[:-h, w:]-integral[h:, :-w]+integral[:-h, :-w]
    expected = int(template.sum())
    score = 2.*intersection/(count+expected)
    y, x = np.unravel_index(score.argmax(), shape)
    return {'score': float(score[y, x]), 'recall': float(intersection[y, x]/expected),
            'precision': float(intersection[y, x]/max(1, count[y, x])),
            'box': [int(x), int(y), int(x+w), int(y+h)]}


def classify_y_image(image_path, catalog_path, *, minimum_score=.94):
    """Pure pixel classification. Freshness and native target remain separate."""
    from PIL import Image
    import numpy as np
    catalog = json.loads(Path(catalog_path).read_text(encoding='utf-8-sig'))
    if (catalog.get('schema') != 1 or not isinstance(catalog.get('templates'), list) or
            type(minimum_score) not in (int, float) or not .9 <= minimum_score <= 1.):
        raise BotFault('Learned current-Y catalog or threshold is unavailable')
    path = Path(image_path)
    rgb = Image.open(path).convert('RGB'); width, height = rgb.size
    if width < 320 or height < 320:
        raise BotFault('Current-Y recognition requires actual retained player-eye pixels')
    search = (int(width*.05), int(height*.55), int(width*.45), int(height*.80))
    mask = _mask(rgb.crop(search)); observations = []
    for template in catalog['templates']:
        source = Path(template['source_image'])
        if not source.is_absolute(): source = Path(catalog_path).parent/source
        raw = source.read_bytes()
        if hashlib.sha256(raw).hexdigest() != template.get('source_sha256'):
            raise BotFault('Learned Y glyph source differs from its retained image identity')
        source_image = Image.open(source).convert('RGB')
        canonical = _mask(source_image.crop(tuple(template['crop'])))
        best = None
        for scale in (.9, 1., 1.1):
            factor = width/source_image.width*scale
            shape = (max(1, round(canonical.shape[1]*factor)),
                     max(1, round(canonical.shape[0]*factor)))
            scaled = np.asarray(Image.fromarray(canonical*255).resize(shape, Image.Resampling.NEAREST))//255
            match = _best_match(mask, scaled)
            if match is not None and (best is None or match['score'] > best['score']):
                best = {**match, 'scale': factor}
        if best is not None:
            best['box'] = [best['box'][0]+search[0], best['box'][1]+search[1],
                           best['box'][2]+search[0], best['box'][3]+search[1]]
            observations.append({'class': template['class'], **best,
                'source_sha256': template['source_sha256'],
                'recognized': best['score'] >= minimum_score})
    by_class = {}
    for row in observations:
        previous = by_class.get(row['class'])
        if previous is None or row['score']>previous['score']: by_class[row['class']]=row
    observations=list(by_class.values())
    found = [row['class'] for row in observations if row['recognized']]
    # Shared base geometry must not make a use/exit class ambiguous. Keep the
    # ambiguity visible rather than picking the highest score and guessing.
    return {'schema': 1, 'image': str(path.resolve()),
            'image_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'size': [width, height], 'classes': found, 'matches': observations,
            'mortar_transition_ambiguous': 'mortar_use' in found and 'mortar_exit_arrow' in found,
            'fulton_and_emplacement_slots_visible': 'fulton_hold' in found and
                any(c.startswith('mortar_') for c in found),
            'freshness_verified': False, 'exact_native_target_id': None,
            'native_action_success_proven': False,
            'scope': 'translated/scaled glyphs from owned final-eye pixels; current lifecycle must be joined separately'}


def current_y_lifecycle(before, after, *, max_milliseconds=2000):
    """Join a bounded static capture to the same fresh world/rig publication."""
    def identity(state):
        n, f = state.get('native') or {}, state.get('rendered') or {}
        return n.get('mission'), n.get('location'), state.get('activation'), f.get('player_owner'), f.get('presentation_epoch')
    a, b = identity(before), identity(after)
    if any(type(v) is not int or v <= 0 for v in a) or b != a:
        raise BotFault('Visible-Y capture changed exact native world/VR owner')
    for s in (before, after):
        f = s.get('rendered') or {}; stamp = f.get('sample_ms')
        if (type(stamp) is not int or type(s.get('now_ms')) is not int or
                not 0 <= s['now_ms']-stamp <= 150 or f.get('presentation_focused') is not True):
            raise BotFault('Visible-Y capture lacks a fresh focused rig publication')
    elapsed = after['now_ms']-before['now_ms']
    if not 0 < elapsed <= max_milliseconds:
        raise BotFault('Visible-Y capture exceeded its bounded current-prompt interval')
    pa = before['rendered'].get('native_pose') or {}; pb = after['rendered'].get('native_pose') or {}
    positions = [pa.get('position'), pb.get('position')]; attitudes = [pa.get('orientation'), pb.get('orientation')]
    for value, length in [(v, 3) for v in positions]+[(v, 4) for v in attitudes]:
        if (not isinstance(value, list) or len(value) != length or
                any(type(v) not in (int, float) or not math.isfinite(v) for v in value)):
            raise BotFault('Visible-Y capture pose is unavailable')
    dot = abs(sum(a*b for a, b in zip(*attitudes)))
    norms = math.prod(math.sqrt(sum(v*v for v in q)) for q in attitudes)
    if (math.dist(*positions) > .05 or norms < .99 or
            dot/norms < math.cos(math.radians(3.)/2.)):
        raise BotFault('Visible-Y capture moved beyond its static current-prompt pose bound')
    return {'identity': a, 'milliseconds': elapsed, 'freshness_verified': True,
            'exact_native_target_link_verified': False}


def observe_current_y(live, *, catalog_path, anchor=None, label='current-y-prompts'):
    """Owning runner's neutral current-capture adapter; no button is dispatched."""
    from .combat import fresh_rig
    from .locomotion import _neutral_audit
    if getattr(live, 'held', None):
        raise BotFault('Current-Y capture requires a released input lease')
    before, identity = fresh_rig(live, anchor)
    _neutral_audit(before['controls'])
    captures = live.capture(label)
    after, _ = fresh_rig(live, identity)
    _neutral_audit(after['controls'])
    lifecycle = current_y_lifecycle(before, after)
    eyes = []
    for item in captures:
        path = item.get('path') if isinstance(item, dict) else item
        if isinstance(path, str) and path.endswith('-left.png'):
            eyes.append(classify_y_image(path, catalog_path))
    if len(eyes)!=1:
        raise BotFault('Current-Y capture lacks one exact retained left player-eye image')
    result={**eyes[0], 'freshness_verified': True, 'lifecycle': lifecycle,
            'before': before, 'after': after, 'captures': captures,
            'sequential_stereo_or_native_target_acceptance': False}
    live.events.emit('current_visible_y_classes', **result)
    return result

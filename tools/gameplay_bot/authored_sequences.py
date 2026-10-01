"""Index literal sequence facts from owned Lua without executing game scripts.

This is a discovery aid, not a Lua interpreter or a proof of legal transitions.
Conditions, helper calls, dynamic targets and runtime eligibility remain open.
"""
import hashlib
from pathlib import Path
import re

from .core import BotFault

SEQUENCE = re.compile(r'Seq_[A-Za-z0-9_]+\Z')
LONG_STRING = re.compile(r'\[(=*)\[')
IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')


def _tokens(text):
    result, i = [], 0
    while i < len(text):
        if text[i].isspace():
            i += 1
            continue
        comment = text.startswith('--', i)
        start = i + 2 if comment else i
        long = LONG_STRING.match(text, start)
        if long:
            body = start + len(long[0])
            end = text.find(']' + long[1] + ']', body)
            if end < 0:
                raise BotFault('Unterminated owned Lua long string/comment')
            if not comment:
                result.append(('string', text[body:end]))
            i = end + len(long[1]) + 2
        elif comment:
            end = text.find('\n', start)
            i = len(text) if end < 0 else end + 1
        elif text[i] in "\"'":
            quote, start = text[i], i + 1
            i = start
            while i < len(text) and text[i] != quote:
                i += 2 if text[i] == '\\' else 1
            if i >= len(text):
                raise BotFault('Unterminated owned Lua quoted string')
            # Only plain identifier literals are admitted as target facts.
            # Escaped/expression-built names are intentionally unresolved.
            result.append(('string', text[start:i]))
            i += 1
        else:
            identifier = IDENTIFIER.match(text, i)
            value = identifier[0] if identifier else text[i]
            result.append(('name' if identifier else 'punct', value))
            i += len(value)
    return result


def _table_end(tokens, start):
    depth = 0
    for i in range(start, len(tokens)):
        if tokens[i] == ('punct', '{'):
            depth += 1
        elif tokens[i] == ('punct', '}'):
            depth -= 1
            if depth == 0:
                return i
    raise BotFault('Unterminated owned Lua sequence table')


def _calls(tokens):
    prefix = [('name', 'TppSequence'), ('punct', '.'), ('name', 'SetNextSequence'), ('punct', '(')]
    for i in range(len(tokens) - len(prefix)):
        if tokens[i:i+4] == prefix:
            argument = tokens[i+4]
            literal = (argument[0] == 'string' and SEQUENCE.fullmatch(argument[1])
                       and i+5 < len(tokens) and tokens[i+5] in [('punct', ')'), ('punct', ',')])
            yield i, argument[1] if literal else None


def index_source(path, *, identifier, mission_codes):
    path = Path(path)
    if not re.fullmatch(r'[A-Z0-9_]+', identifier):
        raise BotFault('Authored sequence source needs an explicit identifier')
    if not mission_codes or any(type(code) is not int or not 10000 <= code <= 99999 for code in mission_codes):
        raise BotFault('Authored source needs explicit candidate native mission codes')
    data = path.read_bytes()
    if len(data) > 4 * 1024 * 1024:
        raise BotFault('Owned sequence source exceeds the bounded index size')
    tokens = _tokens(data.decode('utf-8-sig'))
    tables, registered, unresolved_registration = {}, set(), 0
    for i in range(len(tokens) - 4):
        if (tokens[i:i+2] == [('name', 'sequences'), ('punct', '.')]
                and tokens[i+2][0] == 'name' and SEQUENCE.fullmatch(tokens[i+2][1])
                and tokens[i+3:i+5] == [('punct', '='), ('punct', '{')]):
            name = tokens[i+2][1]
            if name in tables:
                raise BotFault('Repeated sequence definition needs manual scope review: ' + name)
            tables[name] = (i+4, _table_end(tokens, i+4))
        if tokens[i:i+3] != [('name', 'TppSequence'), ('punct', '.'), ('name', 'RegisterSequences')]:
            continue
        argument = i+3
        if tokens[argument] == ('punct', '('):
            argument += 1
        table = argument if tokens[argument] == ('punct', '{') else None
        if table is None and tokens[argument][0] == 'name':
            assignments = [j+2 for j in range(argument-2)
                if tokens[j:j+3] == [tokens[argument], ('punct', '='), ('punct', '{')]]
            if len(assignments) == 1:
                table = assignments[0]
        if table is None:
            unresolved_registration += 1
        else:
            registered.update(value for kind, value in tokens[table:_table_end(tokens, table)+1]
                              if kind == 'string' and SEQUENCE.fullmatch(value))
    transitions, dynamic, attributed = {}, {}, set()
    for name, (start, end) in tables.items():
        dynamic[name] = 0
        for index, target in _calls(tokens[start:end+1]):
            attributed.add(index+start)
            if target is None:
                dynamic[name] += 1
            else:
                transitions[(name, target)] = transitions.get((name, target), 0) + 1
    outside, outside_dynamic = set(), 0
    for index, target in _calls(tokens):
        if index not in attributed:
            if target is None:
                outside_dynamic += 1
            else:
                outside.add(target)
    names = registered | set(tables) | {target for _, target in transitions} | outside
    return {'id': identifier, 'asset': path.name, 'source_sha256': hashlib.sha256(data).hexdigest(),
        'mission_candidates': sorted(set(mission_codes)),
        'states': [{'name': name, 'registration_literal': name in registered,
                    'definition_present': name in tables, 'dynamic_target_calls': dynamic.get(name, 0)}
                   for name in sorted(names)],
        'transitions': [{'from': a, 'to': b, 'literal_call_sites': count}
                        for (a, b), count in sorted(transitions.items())],
        'unattributed_targets': sorted(outside), 'unattributed_dynamic_calls': outside_dynamic,
        'unresolved_registration_calls': unresolved_registration,
        'discovery_complete': False, 'native_acceptance': 'unproven',
        'limits': ['Literal registration, definitions and target calls only; branch/helper conditions are not evaluated',
                   'Mission candidates do not establish eligible packs, exact UI pages, VR actions or native outcomes']}

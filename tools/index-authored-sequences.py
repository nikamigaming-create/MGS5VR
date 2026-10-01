"""Refresh factual state/target metadata from explicit locally owned Lua sources."""
import argparse
import json
from pathlib import Path

from gameplay_bot.authored_sequences import index_source
from gameplay_bot.campaign import read_json
from gameplay_bot.core import BotFault, atomic_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True, help='Private JSON source manifest; no retail data is copied')
    parser.add_argument('--output', type=Path, required=True, help='Factual sequence metadata only')
    args = parser.parse_args()
    manifest = read_json(args.sources)
    sources, seen = [], set()
    if manifest.get('schema') != 1 or not isinstance(manifest.get('sources'), list) or not manifest['sources']:
        raise BotFault('An explicit nonempty owned sequence source manifest is required')
    for row in manifest['sources']:
        if row['id'] in seen:
            raise BotFault('Duplicate authored source identity: ' + row['id'])
        seen.add(row['id'])
        path = Path(row['file'])
        if not path.is_absolute():
            path = args.sources.resolve().parent / path
        sources.append(index_source(path, identifier=row['id'], mission_codes=row['mission_candidates']))
    catalog = {'schema': 1,
        'description': 'Factual authored identifiers and literal transition targets only, derived from locally owned sequence files. Trigger, condition, pack eligibility and VR recipe discovery remain open. No retail script bodies are included.',
        'discovery_complete': False, 'sources': sources}
    atomic_json(args.output, catalog)
    print(json.dumps({'source_sets': len(sources), 'named_states': sum(len(row['states']) for row in sources),
        'literal_target_edges': sum(len(row['transitions']) for row in sources), 'discovery_complete': False}))


if __name__ == '__main__':
    main()

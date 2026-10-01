"""Keep VR menu states and presentations separate during discovery."""
from .campaign import read_json
from .core import BotFault


def menu_inventory(root):
    source = "tools/gameplay_bot/catalogs/vr-menu-paths.json"
    catalog = read_json(root / source)
    modes, presentations = catalog["modes"], catalog["presentations"]
    rows, seen = [], set()
    for family in catalog["families"]:
        states = list(family.get("modes", []))
        if family.get("mode_set"):
            name = family["mode_set"]
            if name not in catalog["mode_sets"]:
                raise BotFault("Unknown menu mode set: " + name)
            states += catalog["mode_sets"][name]
        if not states or not family.get("presentations"):
            raise BotFault("Menu family needs states and VR presentations: " + family["id"])
        for mode in states:
            if mode not in modes:
                raise BotFault("Unknown native menu state: " + mode)
            for presentation in family["presentations"]:
                if presentation not in presentations:
                    raise BotFault("Unknown VR menu presentation: " + presentation)
                identifier = f"MENU.{catalog['game']}.{family['id']}.{mode}.{presentation}"
                if identifier in seen:
                    raise BotFault("Duplicate menu obligation: " + identifier)
                seen.add(identifier)
                rows.append({"id": identifier, "kind": "menu_path", "family": family["id"],
                    "title": f"{family['title']} · {modes[mode]} · {presentations[presentation]}",
                    "game": catalog["game"], "mode": mode, "presentation": presentation,
                    "status": "unproven", "applicability": "not_established",
                    "discovery_complete": False, "native_page": "not_discovered",
                    "acceptance": catalog["acceptance"], "safety": catalog["safety"], "source": source})
                if mode == "acc" and family["id"] in {"HELICOPTER_DEVELOPMENT", "ACC_CUSTOMIZATION"}:
                    rows[-1]["latest_report"] = (
                        "Another player reports an upgrade-menu lock-up in the ACC. Exact "
                        "development/customization screen is unknown; field list/Back evidence "
                        "does not reproduce it.")
    return rows

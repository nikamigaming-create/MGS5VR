"""Keep VR menu states and presentations separate during discovery."""
import copy

from .campaign import read_json
from .core import BotFault


def _modes(catalog, entry):
    states = list(entry.get("modes", []))
    if entry.get("mode_set"):
        name = entry["mode_set"]
        if name not in catalog["mode_sets"]:
            raise BotFault("Unknown menu mode set: " + name)
        states += catalog["mode_sets"][name]
    for mode in states:
        if mode not in catalog["modes"]:
            raise BotFault("Unknown native menu state: " + mode)
    return states


def _variants(catalog, family, identifier):
    """Independent unresolved obligations; never expand combinations as pages."""
    names = catalog.get("default_variant_dimensions", []) + family.get("variant_dimensions", [])
    if len(names) != len(set(names)):
        raise BotFault("Duplicate menu variant dimension: " + family["id"])
    result = []
    for name in names:
        dimension = catalog.get("variant_dimensions", {}).get(name)
        if not dimension or not dimension.get("title") or not dimension.get("values"):
            raise BotFault("Unknown or empty menu variant dimension: " + name)
        seen = set()
        for variant in dimension["values"]:
            key = variant.get("key")
            if not key or key in seen or not variant.get("title") or not variant.get("acceptance"):
                raise BotFault("Invalid or duplicate menu variant: " + name + "." + str(key))
            seen.add(key)
            result.append({"id": identifier + ".VARIANT." + name + "." + key,
                "dimension": name, "dimension_title": dimension["title"], "key": key,
                "title": variant["title"], "acceptance": list(variant["acceptance"]),
                "status": "unproven", "applicability": "not_established",
                "discovery_complete": False, "native_page": "not_discovered", "evidence": []})
    return result


def menu_inventory(root):
    source = "tools/gameplay_bot/catalogs/vr-menu-paths.json"
    catalog = read_json(root / source)
    modes, presentations = catalog["modes"], catalog["presentations"]
    reports = []
    report_ids = set()
    family_ids = {family["id"] for family in catalog["families"]}
    for report in catalog.get("reports", []):
        report_modes = _modes(catalog, report)
        if (not report.get("id") or report["id"] in report_ids or not report.get("summary")
                or not report_modes or not report.get("families")
                or not set(report["families"]) <= family_ids):
            raise BotFault("Invalid or duplicate menu report: " + str(report.get("id")))
        report_ids.add(report["id"])
        reports.append((report, report_modes))
    rows, seen = [], set()
    for family in catalog["families"]:
        states = _modes(catalog, family)
        if not states or not family.get("presentations"):
            raise BotFault("Menu family needs states and VR presentations: " + family["id"])
        for mode in states:
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
                    "acceptance": list(catalog["acceptance"]), "safety": catalog["safety"], "source": source,
                    "variant_obligations": _variants(catalog, family, identifier),
                    "variant_policy": catalog.get("variant_policy", "Variant discovery remains incomplete.")})
                for report, report_modes in reports:
                    if family["id"] in report["families"] and mode in report_modes:
                        details = copy.deepcopy({key: value for key, value in report.items()
                            if key not in {"families", "modes", "mode_set"}})
                        rows[-1].setdefault("reports", []).append(details)
                        rows[-1]["latest_report"] = report["summary"]
    return rows

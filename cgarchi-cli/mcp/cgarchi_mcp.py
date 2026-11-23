#!/usr/bin/env python3
"""Serveur MCP (stdio) des colonnes classiques de cgarchi.

Expose a un assistant les cinq ordres de Vignole tels que cgarchi les porte :
les donnees embarquees (cinq_ordres.json, avec statuts et planches) et la
generation de colonnes et de colonnades en OBJ / GLB / STL. Toute la logique
est dans cgarchi-cli ; ce serveur ne fait que l'appeler.

Bibliotheque standard seule (pas de SDK MCP) : JSON-RPC 2.0, un message par
ligne sur stdin / stdout, comme le transport stdio de MCP.

Executable : variable d'environnement CGARCHI_CLI, sinon
build/cgarchi-cli/cgarchi-cli/Release/cgarchi-cli(.exe) a la racine du depot.
"""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

SERVER_NAME = "cgarchi"
SERVER_VERSION = "0.1.0"
PROTOCOL_VERSION = "2025-06-18"
ORDERS = ["toscan", "dorique", "ionique", "corinthien", "composite"]
MAX_TEXT = 60_000  # au-dela, get_data demande un chemin plus precis

REPO = Path(__file__).resolve().parents[2]
EXE = "cgarchi-cli.exe" if os.name == "nt" else "cgarchi-cli"


def cli_path() -> Path:
    env = os.environ.get("CGARCHI_CLI")
    if env:
        return Path(env)
    for config in ("Release", "RelWithDebInfo", "Debug", "."):
        p = REPO / "build" / "cgarchi-cli" / "cgarchi-cli" / config / EXE
        if p.is_file():
            return p
    raise ToolError("cgarchi-cli introuvable : construire la cible (ENABLE_CGARCHI_CLI=ON) "
                    "ou definir CGARCHI_CLI")


class ToolError(Exception):
    pass


def run_cli(*args: str, stdin: str | None = None) -> str:
    proc = subprocess.run([str(cli_path()), *args], input=stdin, capture_output=True,
                          text=True, encoding="utf-8", timeout=300)
    if proc.returncode == 2:
        raise ToolError(proc.stderr.strip() or "ligne de commande refusee")
    return proc.stdout


def run_cli_json(*args: str, stdin: str | None = None) -> dict:
    out = run_cli(*args, stdin=stdin)
    try:
        return json.loads(out)
    except json.JSONDecodeError as e:
        raise ToolError(f"sortie illisible de cgarchi-cli : {e}") from e


_data_cache: dict[str, dict] = {}


def dataset(name: str) -> dict:
    if name not in _data_cache:
        _data_cache[name] = json.loads(run_cli("data", name))
    return _data_cache[name]


def walk(doc, path: str):
    """Descend dans `doc` par un chemin pointe ("ordres.toscan.blocs.base",
    indices de liste admis : "membres.0")."""
    node = doc
    for part in [p for p in path.split(".") if p]:
        if isinstance(node, dict):
            if part not in node:
                raise ToolError(f"cle absente : {part!r} ; cles disponibles : {sorted(node)}")
            node = node[part]
        elif isinstance(node, list):
            try:
                node = node[int(part)]
            except (ValueError, IndexError) as e:
                raise ToolError(f"indice invalide : {part!r} (liste de {len(node)})") from e
        else:
            raise ToolError(f"{part!r} : impossible de descendre dans une valeur scalaire")
    return node


def outline(node, depth: int = 0):
    """Apercu d'un sous-arbre trop gros : cles et tailles."""
    if isinstance(node, dict):
        return {k: (outline(v, depth + 1) if depth < 1 else type_label(v)) for k, v in node.items()}
    if isinstance(node, list):
        return f"liste de {len(node)} elements"
    return node


def type_label(v):
    if isinstance(v, dict):
        return f"objet ({len(v)} cles)"
    if isinstance(v, list):
        return f"liste de {len(v)} elements"
    return v


# ---------------------------------------------------------------- reglages

SETTING_PROPS = {
    "module": {"type": "number", "minimum": 0.1, "maximum": 10,
               "description": "Module = demi-diametre inferieur du fut, en unites de scene "
                              "(metres dans Blender). Defaut 1."},
    "pedestal": {"type": "boolean", "description": "Ajoute le piedestal."},
    "entablature": {"type": "boolean", "description": "Ajoute l'entablement (colonne seule)."},
    "doricCornice": {"type": "string", "enum": ["dentils", "mutules"],
                     "description": "Corniche du dorique."},
    "envelope": {"type": "boolean",
                 "description": "Ornements rendus par leur enveloppe plutot que par leur noyau."},
    "volutes": {"type": "boolean", "description": "Coussinets a volutes de l'ionique."},
    "flutes": {"type": "boolean", "description": "Cannelures du fut."},
    "doricFlutes": {"type": "string", "enum": ["segment60", "semicircle"],
                    "description": "Section des cannelures du dorique."},
    "radialSegments": {"type": "integer", "minimum": 8, "maximum": 256},
    "curveSegments": {"type": "integer", "minimum": 2, "maximum": 32},
}

ORDER_PROP = {"type": "string", "enum": ORDERS, "description": "Ordre de Vignole."}
OUT_PROP = {"type": "string",
            "description": "Chemin absolu du fichier a ecrire ; l'extension choisit le format "
                           "(.obj, .glb, .stl). Defaut : un .obj dans le dossier temporaire. "
                           "OBJ : coordonnees brutes, Z vers le haut, en unites de scene ; dans "
                           "Blender, bpy.ops.wm.obj_import(filepath=..., forward_axis='Y', "
                           "up_axis='Z'). GLB : cgmesh suppose des millimetres (noeud a l'echelle "
                           "0,001, Y vers le haut), a eviter pour une scene en metres."}


def settings_document(args: dict) -> str:
    order = args.get("order")
    if order not in ORDERS:
        raise ToolError(f"order doit valoir l'un de {ORDERS}")
    doc = json.loads(run_cli("defaults", "--order", order))
    for key in SETTING_PROPS:
        if key in args:
            doc[key] = args[key]
    return json.dumps(doc)


def out_path(args: dict, stem: str) -> str:
    out = args.get("out")
    if out:
        p = Path(out)
        if not p.is_absolute():
            raise ToolError("out doit etre un chemin absolu")
        p.parent.mkdir(parents=True, exist_ok=True)
        return str(p)
    d = Path(tempfile.gettempdir()) / "cgarchi-mcp"
    d.mkdir(exist_ok=True)
    return str(d / f"{stem}-{args['order']}.obj")


# ---------------------------------------------------------------- outils

def tool_list_orders(_args):
    orders = dataset("orders")
    res = {
        "source": orders.get("titre"),
        "version": orders.get("version"),
        "unites": orders.get("conventions", {}).get("unites"),
        "temoins": orders.get("temoins"),
        "ordres": {k: {"parties_par_module": v.get("parties_par_module"), "general": v.get("general")}
                   for k, v in orders.get("ordres", {}).items()},
    }
    return res


def tool_get_data(args):
    name = args.get("dataset", "orders")
    if name not in ("orders", "hephaistos"):
        raise ToolError("dataset : 'orders' ou 'hephaistos'")
    node = walk(dataset(name), args.get("path", ""))
    text = json.dumps(node, ensure_ascii=False, indent=1)
    if len(text) > MAX_TEXT:
        return {"tronque": True,
                "raison": f"{len(text)} caracteres ; preciser `path` vers un sous-arbre",
                "apercu": outline(node)}
    return node


def tool_build_column(args):
    settings = settings_document(args)
    out = out_path(args, "colonne")
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as f:
        f.write(settings)
    try:
        res = run_cli_json("column", "--settings", f.name, "--out", out)
    finally:
        os.unlink(f.name)
    return res


def tool_build_colonnade(args):
    settings = settings_document(args)
    out = out_path(args, "colonnade")
    cli = ["colonnade", "--settings", "-", "--out", out]
    if "columns" in args:
        cli += ["--columns", str(int(args["columns"]))]
    if args.get("ornaments") is False:
        cli.append("--no-ornaments")
    return run_cli_json(*cli, stdin=settings)


def tool_column_levels(args):
    return run_cli_json("levels", "--settings", "-", stdin=settings_document(args))


TOOLS = {
    "list_orders": {
        "fn": tool_list_orders,
        "description": "Liste les cinq ordres de Vignole portes par cgarchi, avec leurs proportions "
                       "generales (en modules et parties) et les editions de reference.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
    },
    "get_data": {
        "fn": tool_get_data,
        "description": "Lit un sous-arbre des donnees historiques embarquees. dataset 'orders' = "
                       "cinq_ordres.json (Vignole : conventions, ordres.<ordre>.blocs.<bloc>.membres, "
                       "chaque cote avec son statut lu/deduit/mesure/convention et sa planche) ; "
                       "'hephaistos' = releve du temple d'Hephaistos. Chemin pointe, ex. "
                       "'ordres.corinthien.blocs.chapiteau'. Un sous-arbre trop gros renvoie un apercu.",
        "inputSchema": {"type": "object", "additionalProperties": False, "properties": {
            "dataset": {"type": "string", "enum": ["orders", "hephaistos"]},
            "path": {"type": "string", "description": "Chemin pointe ; vide = racine."}}},
    },
    "build_column": {
        "fn": tool_build_column,
        "description": "Genere une colonne d'un ordre de Vignole (axe Z, pied a z=0) et l'ecrit en "
                       ".obj/.glb/.stl. Renvoie le chemin, les compteurs et les cotes (pied, fut, "
                       "chapiteau...) en unites de scene. Hors perimetre : acanthe, caulicoles, "
                       "fleurons, volutes composites (rendus par leur noyau ou enveloppe).",
        "inputSchema": {"type": "object", "required": ["order"], "additionalProperties": False,
                        "properties": {"order": ORDER_PROP, "out": OUT_PROP, **SETTING_PROPS}},
    },
    "build_colonnade": {
        "fn": tool_build_colonnade,
        "description": "Genere une colonnade de N colonnes sous un entablement file, avec ses "
                       "ornements (triglyphes, mutules, denticules...). Axe X le long de la "
                       "colonnade, centree sur X=0, pied a z=0.",
        "inputSchema": {"type": "object", "required": ["order"], "additionalProperties": False,
                        "properties": {"order": ORDER_PROP, "out": OUT_PROP,
                                       "columns": {"type": "integer", "minimum": 1, "maximum": 12},
                                       "ornaments": {"type": "boolean"},
                                       **{k: v for k, v in SETTING_PROPS.items() if k != "entablature"}}},
    },
    "column_levels": {
        "fn": tool_column_levels,
        "description": "Cotes z d'une colonne (pied, haut du piedestal, fut, chapiteau, sommet, pied "
                       "de l'entablement) sans generer de maillage.",
        "inputSchema": {"type": "object", "required": ["order"], "additionalProperties": False,
                        "properties": {"order": ORDER_PROP, **SETTING_PROPS}},
    },
}


# ---------------------------------------------------------------- JSON-RPC

def result_text(payload, is_error=False):
    text = payload if isinstance(payload, str) else json.dumps(payload, ensure_ascii=False, indent=1)
    return {"content": [{"type": "text", "text": text}], "isError": is_error}


def handle(msg: dict):
    method = msg.get("method")
    params = msg.get("params") or {}
    if method == "initialize":
        return {"protocolVersion": params.get("protocolVersion", PROTOCOL_VERSION),
                "capabilities": {"tools": {}},
                "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION}}
    if method == "ping":
        return {}
    if method == "tools/list":
        return {"tools": [{"name": n, "description": t["description"], "inputSchema": t["inputSchema"]}
                          for n, t in TOOLS.items()]}
    if method == "tools/call":
        tool = TOOLS.get(params.get("name"))
        if tool is None:
            raise RpcError(-32602, f"outil inconnu : {params.get('name')}")
        try:
            res = tool["fn"](params.get("arguments") or {})
        except ToolError as e:
            return result_text(str(e), is_error=True)
        except subprocess.TimeoutExpired:
            return result_text("cgarchi-cli : delai depasse", is_error=True)
        is_error = isinstance(res, dict) and res.get("ok") is False
        return result_text(res, is_error)
    raise RpcError(-32601, f"methode inconnue : {method}")


class RpcError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def main():
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    for line in sys.stdin.buffer:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            reply = {"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "parse error"}}
            print(json.dumps(reply), flush=True)
            continue
        if "id" not in msg:  # notification
            continue
        try:
            reply = {"jsonrpc": "2.0", "id": msg["id"], "result": handle(msg)}
        except RpcError as e:
            reply = {"jsonrpc": "2.0", "id": msg["id"], "error": {"code": e.code, "message": str(e)}}
        except Exception as e:  # pragma: no cover - garde-fou du serveur
            reply = {"jsonrpc": "2.0", "id": msg["id"], "error": {"code": -32603, "message": repr(e)}}
        print(json.dumps(reply, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()

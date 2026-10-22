#!/usr/bin/env python3
"""Pont embind de la microarchitecture gothique, sans navigateur.

Relie les objets de `maker` deja construits (preset maker-wasm) pour node
(-sENVIRONMENT=node, sortie hors de maker/web/), puis lance
microarch_bridge.cjs : les liaisons de la microarchitecture, la garde des
exports generiques (exportObj, exportGlb, gothicPrintReport), regenerate, qui
ne produit pas d'OBJ texte pour elle, et l'export STL du solide imprimable (J7-d :
verdicts, seuil de verification, refus au plafond, tas WASM stable).

  (maker construit : cmake --build build/maker-wasm --target maker)
  python maker/probes/microarch_bridge.py --emsdk C:/home/dev/extern/emsdk
  python maker/probes/microarch_bridge.py --native-json <reseau natif>   (empreinte)
  python maker/probes/microarch_bridge.py --wasm-api <mutant.cpp>       (mutation)
  python maker/probes/microarch_bridge.py --cgarchi-source <mutant.cpp> (mutation d'une source de cgarchi)
  python maker/probes/microarch_bridge.py --quick                       (sans les cas denses, ~1 min de moins)
  python maker/probes/microarch_bridge.py --verified-threshold 2097152  (chemin non verifie, seuil injecte)

Sortie 0 si tous les controles passent. --wasm-api compile une autre version de
maker/wasm_api.cpp avec les options du build et la substitue a son objet ;
--cgarchi-source compile une version de l'une des sources de cgarchi avec les
options de cgarchi et place son objet devant les bibliotheques, ou il remplace le
membre de libcgarchi.a qui porte les memes symboles. --verified-threshold ecrit, dans
le dossier de sortie, une copie de wasm_api.cpp dont la microarchitecture est construite
avec ce seuil de verification (M36 : au defaut, 2^22, aucun solide accepte n'est au-dela ;
le chemin non verifie ne se joue que par un seuil injecte, M38 Q-A6). Le depot n'est pas
modifie.
Le module de la sonde exporte HEAPU8 (taille du tas WASM), pas celui de la page.
"""
import argparse
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
HERE = pathlib.Path(__file__).resolve().parent

LINK_FLAGS = [
    "-O2", "-lembind", "-fexceptions", "-sMODULARIZE=1", "-sEXPORT_NAME=createMakerModule",
    "-sALLOW_MEMORY_GROWTH=1", "-sFORCE_FILESYSTEM=1", "-sENVIRONMENT=node",
    "-sMAX_WEBGL_VERSION=2", "-sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap,HEAPU8",
]


def tool(name, *fallbacks):
    for candidate in (name, *fallbacks):
        path = shutil.which(str(candidate))
        if path:
            return path
    sys.exit(f"{name} introuvable : passer --emsdk (ou EMSDK), ou activer emsdk_env")


def emsdk_tools(root):
    """em++ et node, depuis le PATH ou depuis la racine d'emsdk."""
    if root:
        root = pathlib.Path(root)
        # Chemin natif (antislashs sous Windows) : un autre format invalide le cache d'Emscripten.
        os.environ.setdefault("EM_CONFIG", str(root / ".emscripten"))
        nodes = sorted((root / "node").glob("*/bin/node*")) + sorted((root / "node").glob("*/node.exe"))
        return tool("em++", root / "upstream" / "emscripten" / "em++"), tool("node", *nodes)
    return tool("em++"), tool("node")


def make_var(flags_make, key):
    m = re.search(rf"^{key} = (.*)$", flags_make, re.M)
    return m.group(1).split() if m else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=str(REPO / "build" / "maker-wasm"))
    ap.add_argument("--emsdk", default=os.environ.get("EMSDK"), help="racine d'emsdk, si em++ n'est pas dans le PATH")
    ap.add_argument("--out-dir", default=None)
    ap.add_argument("--wasm-api", default=None, help="source de remplacement de maker/wasm_api.cpp")
    ap.add_argument("--native-json", default=None, help="JSON du reseau D4 ecrit par le test natif")
    ap.add_argument("--cgarchi-source", action="append", default=[],
                    help="source de remplacement d'un fichier de src/cgarchi (repetable)")
    ap.add_argument("--quick", action="store_true", help="sans les cas denses (seuil M33, plafond)")
    ap.add_argument("--verified-threshold", type=int, default=None,
                    help="seuil de verification injecte (copie de wasm_api.cpp), au lieu de 2^22")
    args = ap.parse_args()

    build = pathlib.Path(args.build_dir).resolve()
    target = build / "maker"
    objdir = target / "CMakeFiles" / "maker.dir"
    out = pathlib.Path(args.out_dir).resolve() if args.out_dir else build / "probes" / "microarch_bridge"
    out.mkdir(parents=True, exist_ok=True)
    empp, node = emsdk_tools(args.emsdk)

    objects = (objdir / "objects1.rsp").read_text().replace('"', "").split()
    libs = (objdir / "linkLibs.rsp").read_text().split()
    threshold = 1 << 22
    if args.verified_threshold is not None:
        if args.wasm_api:
            sys.exit("--verified-threshold et --wasm-api s'excluent")
        threshold = args.verified_threshold
        site = "std::make_unique<cgarchi::ParameterizedGothicMicroarchitecture>()"
        text = (REPO / "maker" / "wasm_api.cpp").read_text(encoding="utf-8")
        if text.count(site) != 1:
            sys.exit("site de construction de la microarchitecture introuvable dans wasm_api.cpp")
        injected = out / "wasm_api_threshold.cpp"
        injected.write_text(text.replace(site, "std::make_unique<cgarchi::ParameterizedGothicMicroarchitecture>("
                                         "[] { cgarchi::MicroarchMakerLimits l; l.max_verified_triangles = "
                                         f"{threshold}u; return l; }}())"), encoding="utf-8")
        args.wasm_api = str(injected)
    if args.wasm_api:
        flags_make = (objdir / "flags.make").read_text(encoding="utf-8-sig")
        mutant_obj = out / "wasm_api_alt.o"
        cmd = [empp, *make_var(flags_make, "CXX_FLAGS"), *make_var(flags_make, "CXX_DEFINES"),
               f"@{objdir / 'includes_CXX.rsp'}", f"-I{REPO / 'maker'}",
               "-c", str(pathlib.Path(args.wasm_api).resolve()), "-o", str(mutant_obj)]
        if subprocess.run(cmd, cwd=target).returncode:
            sys.exit("compilation de --wasm-api en echec")
        objects = [str(mutant_obj) if o.endswith("wasm_api.cpp.o") else o for o in objects]
    if args.cgarchi_source:
        cg_dir = build / "src" / "cgarchi" / "CMakeFiles" / "cgarchi.dir"
        cg_flags = (cg_dir / "flags.make").read_text(encoding="utf-8-sig")
        for k, src in enumerate(args.cgarchi_source):
            obj = out / f"cgarchi_alt_{k}.o"
            cmd = [empp, *make_var(cg_flags, "CXX_FLAGS"), *make_var(cg_flags, "CXX_DEFINES"),
                   *make_var(cg_flags, "CXX_INCLUDES"), "-c", str(pathlib.Path(src).resolve()), "-o", str(obj)]
            # CXX_INCLUDES designe un fichier de reponse relatif au dossier de la cible.
            if subprocess.run(cmd, cwd=build / "src" / "cgarchi").returncode:
                sys.exit(f"compilation de --cgarchi-source {src} en echec")
            objects.append(str(obj))

    js = out / "maker_node.js"
    if subprocess.run([empp, *objects, *libs, *LINK_FLAGS, "-o", str(js)], cwd=target).returncode:
        sys.exit("lien en echec")

    cmd = [node, "--max-old-space-size=4096", str(HERE / "microarch_bridge.cjs"), str(js),
           str(REPO / "test" / "data" / "gothic_micro_d4_segment.json"),
           "" if args.quick else str(HERE / "microarch_dense_b2.json")]
    cmd.append(hashlib.sha256(pathlib.Path(args.native_json).read_bytes()).hexdigest() if args.native_json else "")
    cmd.append(str(threshold))
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())

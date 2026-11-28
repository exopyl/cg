#!/usr/bin/env python3
"""Compilation des gabarits de feuille d'acanthe (acanthus_drawing.json) en maillages plans figes.

Source : acanthus_drawing.json, voisin de ce script, dessin d'une demi-feuille par style, decalque
sur des images de reference tenues hors du depot. Sortie : acanthus_leaves.json, commite a cote
et embarque dans cgarchi ; le C++ ne triangule pas, il plaque ce maillage sur la porteuse.

Repere du dessin : u = X / hw du rang ; y developpe le long du meridien d'axe, corps sur [0, 1], arc
du bec sur [1, 2] (crete vers y = 1,53), bande de tige sur [-1, 0). Demi-feuille u >= 0 ; l'axe
u = 0 est une couture, la symetrie se fait en C++. La triangulation se fait dans la metrique nominale
du style (X = hw u ; Y = body y sur le corps, body + curl (y - 1) sur l'arc), l'echantillonnage des
segments dans (u, y) : le bord maille reste exactement sur le contour dessine.

Bande de tige (bloc stem du style : u, taper, rows, metric) : apres la compilation d'un niveau, les
sommets du bord de pied d'abscisse u <= stem.u sont prolonges en rows rangs de quadrilateres,
y = -k / rows, u = u_pied (1 - (1 - taper) k / rows) ; H = relief de la tige a la colonne, Hb nul, region de
la tige ; diagonale choisie au meilleur angle dans la metrique de la bande (X = metric.hw u ;
Y = metric.length y). Sommets et triangles de la bande suivent ceux de la feuille : la feuille
seule (F) est le prefixe leafVertices / leafTriangles, la feuille et sa bande (E) le tout.

Compilation d'un niveau (light : points du dessin, lignes step et lignes de niveau light ;
heavy : toutes les lignes) :
  1. graphe de contraintes : contour ferme par l'axe, lignes dont les extremites proches d'un
     sommet ou d'un segment y sont recollees ; segments coupes aux sommets qu'ils portent ;
     aucun croisement admis ;
  2. echantillonnage des segments au pas h, semis hexagonal au pas h a plus de 0,6 h des
     contraintes ;
  3. Delaunay incremental (Bowyer-Watson), ordre d'insertion de Hilbert, predicats exacts ;
  4. contraintes retablies par bascules (Sloan), sans point ajoute, puis Lawson ;
  5. exterieur retire par remplissage arrete par le contour ; regions par remplissage depuis
     leur graine, arrete par les lignes step ;
  6. relief par distance aux lignes : H (face exterieure), Hb (revers, crosse seulement). Une region
     sans nervure est la tige ; une region a nervure, un lobe.
Aucun point n'etant ajoute, le nombre de triangles est connu avant la triangulation
(T = 2V - B - 2) : le pas h est cherche par bissection sur ce compte pour viser le budget du
niveau, puis le maillage est construit une fois.

Determinisme : aucune graine aleatoire, aucun bruit sur les coordonnees ; la topologie ne depend
que de predicats exacts ; le relief n'emploie que des operations numpy element par element
(aucune reduction BLAS). Les nombres sont ecrits arrondis (u, y a 1e-6, relief a 1e-5) ; un
sommet de l'axe a u = 0 exactement.

Usage :
    python -I acanthus_leaves_from_drawing.py --write [--drawing <json>] [--out <json>]
    python -I acanthus_leaves_from_drawing.py --check [--drawing <json>] [--out <json>]
    python -I acanthus_leaves_from_drawing.py --overlay --style <style> --image <image>
              --overlay-out <png> [--trace <calage>] [--drawing <json>]

--write    compile chaque style et ecrit la sortie ; rien n'est ecrit si un controle echoue.
--check    compile en memoire et compare a la sortie existante, octet par octet apres
           conversion des fins de ligne CRLF en LF ; compare aussi l'empreinte du dessin.
--overlay  superpose le gabarit (regions teintees, contour, lignes) a l'image de reference,
           calee par les trois points d'ancrage du style (trace.anchors : u, y, px, py, ou un calage
           nomme de trace.views choisi par --trace) ; la crosse suit la table curl du calage (hauteur
           apparente de face), la bande de tige l'echelle stem du calage s'il en porte une ; demande
           Pillow.

Controles internes (refus, sortie 1) : format, etiquettes et sorties du contour, polygone
simple et direct, lignes sans croisement, graine dans le domaine et une region par graine,
chaque triangle dans une region, chaque ligne step entre deux regions differentes, chaque
contrainte presente dans le maillage, aucun sommet inutilise, Euler, triangles directs apres
arrondi, angle minimal au-dessus du plancher du style, compte dans la tolerance du budget ; pour
la bande : une seule region de tige, colonnes contigues sur le bord de pied de u = 0 a u = stem.u
exactement, aucun sommet de bande hors de [0, stem.u] x [-1, 0], angle minimal et sens direct.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
import os
import sys
import time
from collections import deque
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path

if sys.version_info < (3, 10):  # noqa: UP036
    sys.exit(f"acanthus_leaves_from_drawing.py demande Python 3.10 ou plus recent, "
             f"lance par {sys.version.split()[0]}")

import numpy as np

DRAWING_FORMAT = "cgarchi-acanthus-drawing/1"
LEAVES_FORMAT = "cgarchi-acanthus-leaves/2"
DERIVED_BY = "src/cgarchi/data/acanthus_leaves_from_drawing.py"
DEFAULT_DRAWING = Path(__file__).resolve().with_name("acanthus_drawing.json")
DEFAULT_OUT = Path(__file__).resolve().with_name("acanthus_leaves.json")

LEVELS = ("light", "heavy")
TAGS = ("tip", "notch", "smooth", "rim", "base", "apex", "axis")
REGION_KEYS = frozenset({"id", "seed", "layer", "cup", "vein"})
LINE_KINDS = ("step", "vein", "veinlet", "flute", "ridge")
# Recollement des extremites de ligne, dans (u, y) : sur un sommet ou un segment ; une ligne
# "attach" cherche son origine sur sa ligne porteuse jusqu'a ATTACH.
SNAP = 0.006
ATTACH = 0.05
# Un point d'une ligne est sur un segment s'il en est a moins de ON_SEGMENT, dans (u, y).
ON_SEGMENT = 1e-9
# Semis hexagonal : un point du semis reste a plus de SEED_CLEARANCE h des contraintes.
SEED_CLEARANCE = 0.6
DIGITS_UY = 6
DIGITS_RELIEF = 5
BISECTION_STEPS = 48


class DrawingError(Exception):
    """Refus de compilation ; porte un message par controle echoue."""

    def __init__(self, errors: list[str]) -> None:
        super().__init__("; ".join(errors))
        self.errors = errors


def refuse(message: str) -> DrawingError:
    return DrawingError([message])


# ---------------------------------------------------------------------------------------------
# Predicats exacts : filtre flottant (bornes A de Shewchuk), repli rationnel sous la borne.

_EPS = 2.0 ** -53
_CCW_BOUND = (3.0 + 16.0 * _EPS) * _EPS
_ICC_BOUND = (10.0 + 96.0 * _EPS) * _EPS

Point = tuple[float, float]


def orient(a: Point, b: Point, c: Point) -> int:
    """Signe de l'aire de (a, b, c) : +1 sens trigonometrique, -1 horaire, 0 alignes."""
    detl = (b[0] - a[0]) * (c[1] - a[1])
    detr = (b[1] - a[1]) * (c[0] - a[0])
    det = detl - detr
    bound = _CCW_BOUND * (abs(detl) + abs(detr))
    if det > bound:
        return 1
    if -det > bound:
        return -1
    ax, ay, bx, by, cx, cy = (Fraction(v) for v in (*a, *b, *c))
    e = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
    return (e > 0) - (e < 0)


def incircle(a: Point, b: Point, c: Point, d: Point) -> int:
    """+1 si d est strictement dans le cercle de (a, b, c) direct, -1 dehors, 0 dessus."""
    adx, ady = a[0] - d[0], a[1] - d[1]
    bdx, bdy = b[0] - d[0], b[1] - d[1]
    cdx, cdy = c[0] - d[0], c[1] - d[1]
    bdxcdy, cdxbdy = bdx * cdy, cdx * bdy
    cdxady, adxcdy = cdx * ady, adx * cdy
    adxbdy, bdxady = adx * bdy, bdx * ady
    alift = adx * adx + ady * ady
    blift = bdx * bdx + bdy * bdy
    clift = cdx * cdx + cdy * cdy
    det = alift * (bdxcdy - cdxbdy) + blift * (cdxady - adxcdy) + clift * (adxbdy - bdxady)
    permanent = ((abs(bdxcdy) + abs(cdxbdy)) * alift + (abs(cdxady) + abs(adxcdy)) * blift
                 + (abs(adxbdy) + abs(bdxady)) * clift)
    bound = _ICC_BOUND * permanent
    if det > bound:
        return 1
    if -det > bound:
        return -1
    ax, ay, bx, by, cx, cy, dx, dy = (Fraction(v) for v in (*a, *b, *c, *d))
    adx, ady, bdx, bdy, cdx, cdy = ax - dx, ay - dy, bx - dx, by - dy, cx - dx, cy - dy
    e = ((adx * adx + ady * ady) * (bdx * cdy - cdx * bdy) + (bdx * bdx + bdy * bdy) * (cdx * ady - adx * cdy)
         + (cdx * cdx + cdy * cdy) * (adx * bdy - bdx * ady))
    return (e > 0) - (e < 0)


def proper_cross(a: Point, b: Point, c: Point, d: Point) -> bool:
    """Les segments ab et cd se coupent en un point interieur aux deux."""
    return (orient(a, b, c) * orient(a, b, d) < 0) and (orient(c, d, a) * orient(c, d, b) < 0)


# ---------------------------------------------------------------------------------------------
# Dessin.

@dataclass(frozen=True)
class Line:
    ident: str
    kind: str
    level: str
    attach: str | None
    points: tuple[Point, ...]


@dataclass(frozen=True)
class Region:
    ident: str
    seed: Point
    layer: float
    cup: float
    vein: str | None


@dataclass(frozen=True)
class Stem:
    u: float
    taper: float
    rows: dict[str, int]
    hw: float
    length: float


@dataclass(frozen=True)
class Style:
    name: str
    hw: float
    body: float
    curl: float
    contour: tuple[Point, ...]
    tags: tuple[str, ...]
    regions: tuple[Region, ...]
    lines: tuple[Line, ...]
    relief: dict
    budget: dict
    min_angle: dict
    trace: dict | None
    stem: Stem

    def to_metric(self, u: float, y: float) -> Point:
        return (u * self.hw, y * self.body if y <= 1.0 else self.body + (y - 1.0) * self.curl)

    def to_uy(self, x: float, yy: float) -> Point:
        return (x / self.hw, yy / self.body if yy <= self.body else 1.0 + (yy - self.body) / self.curl)


def _number(value: object, where: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise refuse(f"{where} : nombre attendu, lu {value!r}")
    return float(value)


def _point(value: object, where: str) -> Point:
    if not isinstance(value, list) or len(value) != 2:
        raise refuse(f"{where} : couple (u, y) attendu, lu {value!r}")
    return (_number(value[0], where), _number(value[1], where))


def _require(obj: dict, key: str, where: str) -> object:
    if key not in obj:
        raise refuse(f"{where} : cle {key!r} absente")
    return obj[key]


def parse_style(name: str, d: dict) -> Style:
    where = f"styles.{name}"
    nominal = _require(d, "nominal", where)
    hw = _number(_require(nominal, "hw", where + ".nominal"), where + ".nominal.hw")
    body = _number(_require(nominal, "body", where + ".nominal"), where + ".nominal.body")
    curl = _number(_require(nominal, "curl", where + ".nominal"), where + ".nominal.curl")
    if min(hw, body, curl) <= 0.0:
        raise refuse(f"{where}.nominal : hw, body et curl doivent etre positifs")
    raw = _require(d, "contour", where)
    if not isinstance(raw, list) or len(raw) < 4:
        raise refuse(f"{where}.contour : au moins quatre sommets attendus")
    contour, tags = [], []
    for k, p in enumerate(raw):
        w = f"{where}.contour[{k}]"
        if not isinstance(p, list) or len(p) != 3 or p[2] not in TAGS:
            raise refuse(f"{w} : [u, y, etiquette] attendu, etiquette parmi {TAGS}, lu {p!r}")
        contour.append((_number(p[0], w), _number(p[1], w)))
        tags.append(p[2])
    if contour[0] != (0.0, 0.0) or tags[0] != "axis":
        raise refuse(f"{where}.contour : le premier sommet est le pied de l'axe [0, 0, \"axis\"]")
    if contour[-1][0] != 0.0 or tags[-1] != "apex":
        raise refuse(f"{where}.contour : le dernier sommet est l'apex, sur l'axe [0, y, \"apex\"]")
    for k, (u, _y) in enumerate(contour[1:-1], start=1):
        if u <= 0.0:
            raise refuse(f"{where}.contour[{k}] : u > 0 attendu hors des extremites de l'axe, lu {u}")
    regions = []
    for k, r in enumerate(_require(d, "regions", where)):
        w = f"{where}.regions[{k}]"
        if not isinstance(r, dict):
            raise refuse(f"{w} : objet attendu")
        unknown = sorted(set(r) - REGION_KEYS)
        if unknown:
            raise refuse(f"{w} : cle(s) inconnue(s) {unknown}, parmi {sorted(REGION_KEYS)}")
        vein = r.get("vein")
        if vein is not None and not isinstance(vein, str):
            raise refuse(f"{w}.vein : identifiant de ligne ou null attendu")
        regions.append(Region(str(_require(r, "id", w)), _point(_require(r, "seed", w), w + ".seed"),
                              _number(_require(r, "layer", w), w + ".layer"),
                              _number(_require(r, "cup", w), w + ".cup"), vein))
    lines = []
    for k, ln in enumerate(_require(d, "lines", where)):
        w = f"{where}.lines[{k}]"
        kind = _require(ln, "kind", w)
        if kind not in LINE_KINDS:
            raise refuse(f"{w}.kind : parmi {LINE_KINDS}, lu {kind!r}")
        level = ln.get("level", "light" if kind == "step" else "heavy")
        if level not in LEVELS:
            raise refuse(f"{w}.level : parmi {LEVELS}, lu {level!r}")
        pts = tuple(_point(p, f"{w}.points") for p in _require(ln, "points", w))
        if len(pts) < 2:
            raise refuse(f"{w}.points : au moins deux points")
        lines.append(Line(str(_require(ln, "id", w)), kind, level, ln.get("attach"), pts))
    ids = [ln.ident for ln in lines]
    if len(set(ids)) != len(ids):
        raise refuse(f"{where}.lines : identifiants en double")
    for r in regions:
        if r.vein is not None and r.vein not in ids:
            raise refuse(f"{where}.regions.{r.ident}.vein : ligne {r.vein!r} inconnue")
    for ln in lines:
        if ln.attach is not None and ln.attach not in ids:
            raise refuse(f"{where}.lines.{ln.ident}.attach : ligne {ln.attach!r} inconnue")
    budget = _require(d, "budget", where)
    min_angle = _require(d, "minAngle", where)
    for level in LEVELS:
        _number(_require(budget, level, where + ".budget"), f"{where}.budget.{level}")
        _number(_require(min_angle, level, where + ".minAngle"), f"{where}.minAngle.{level}")
    _number(_require(budget, "tolerance", where + ".budget"), where + ".budget.tolerance")
    return Style(name, hw, body, curl, tuple(contour), tuple(tags), tuple(regions), tuple(lines),
                 _require(d, "relief", where), budget, min_angle, d.get("trace"), parse_stem(d, where))


def parse_stem(d: dict, where: str) -> Stem:
    w = where + ".stem"
    s = _require(d, "stem", where)
    u = _number(_require(s, "u", w), w + ".u")
    taper = _number(_require(s, "taper", w), w + ".taper")
    if not (u > 0.0 and 0.0 < taper <= 1.0):
        raise refuse(f"{w} : u > 0 et taper dans ]0, 1] attendus, lus {u}, {taper}")
    rows = {}
    for level in LEVELS:
        n = _require(_require(s, "rows", w), level, w + ".rows")
        if isinstance(n, bool) or not isinstance(n, int) or n < 1:
            raise refuse(f"{w}.rows.{level} : entier >= 1 attendu, lu {n!r}")
        rows[level] = n
    metric = _require(s, "metric", w)
    hw = _number(_require(metric, "hw", w + ".metric"), w + ".metric.hw")
    length = _number(_require(metric, "length", w + ".metric"), w + ".metric.length")
    if min(hw, length) <= 0.0:
        raise refuse(f"{w}.metric : hw et length positifs attendus")
    return Stem(u, taper, rows, hw, length)


def parse_drawing(raw: bytes) -> list[Style]:
    try:
        doc = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        raise refuse(f"dessin illisible : {e}") from e
    if doc.get("format") != DRAWING_FORMAT:
        raise refuse(f"format {doc.get('format')!r}, attendu {DRAWING_FORMAT!r}")
    styles = doc.get("styles")
    if not isinstance(styles, dict) or not styles:
        raise refuse("styles : objet non vide attendu")
    return [parse_style(name, resolve_derived(name, styles)) for name in styles]


# Cles qu'un style derive (derivedFrom) peut remplacer : le contour, les regions, les lignes, le relief et la
# metrique nominale sont ceux du style parent.
DERIVED_KEYS = {"derivedFrom", "source", "budget", "minAngle", "stem"}


def resolve_derived(name: str, styles: dict) -> dict:
    """Style `name` du dessin ; s'il porte derivedFrom, le style parent dont les cles DERIVED_KEYS sont remplacees."""
    d = styles[name]
    if not isinstance(d, dict):
        raise refuse(f"styles.{name} : objet attendu")
    parent = d.get("derivedFrom")
    if parent is None:
        return d
    where = f"styles.{name}"
    unknown = sorted(set(d) - DERIVED_KEYS)
    if unknown:
        raise refuse(f"{where} : cle(s) {unknown} non admise(s) dans un style derive, parmi {sorted(DERIVED_KEYS)}")
    if not isinstance(parent, str) or parent not in styles or parent == name:
        raise refuse(f"{where}.derivedFrom : style parent du dessin attendu, lu {parent!r}")
    base = styles[parent]
    if not isinstance(base, dict) or "derivedFrom" in base:
        raise refuse(f"{where}.derivedFrom : le parent {parent!r} doit etre un style dessine, non derive")
    merged = {k: v for k, v in base.items()}
    merged.update({k: v for k, v in d.items() if k != "derivedFrom"})
    return merged


# ---------------------------------------------------------------------------------------------
# Graphe de contraintes (PSLG), dans (u, y).

def _project(p: Point, a: Point, b: Point) -> tuple[float, Point]:
    abx, aby = b[0] - a[0], b[1] - a[1]
    t = ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / (abx * abx + aby * aby)
    t = min(1.0, max(0.0, t))
    c = (a[0] + t * abx, a[1] + t * aby)
    return math.hypot(p[0] - c[0], p[1] - c[1]), c


@dataclass
class Polyline:
    ident: str
    kind: str
    attach: str | None
    points: list[Point]


def polygon_is_simple(poly: list[Point]) -> str | None:
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            c, d = poly[j], poly[(j + 1) % n]
            if proper_cross(a, b, c, d) or (orient(a, b, c) == 0 and _project(c, a, b)[0] == 0.0):
                return f"contour : les cotes {i} et {j} se touchent"
    return None


def build_polylines(style: Style, level: str) -> list[Polyline]:
    """Contour ferme et lignes du niveau, extremites recollees ; segments coupes aux sommets."""
    contour = list(style.contour)
    # Pied de l'axe en tete, apex en queue et u > 0 entre les deux (parse_style) : un contour simple
    # est alors en sens trigonometrique, aucune autre garde de sens n'est necessaire.
    if (msg := polygon_is_simple(contour)) is not None:
        raise refuse(f"{style.name} : {msg}")
    polys = [Polyline("contour", "contour", None, contour + [contour[0]])]
    for ln in style.lines:
        if level == "light" and ln.level != "light":
            continue
        polys.append(Polyline(ln.ident, ln.kind, ln.attach, list(ln.points)))
    keys: list[Point] = list(contour)
    for pl in polys[1:]:
        for end in (0, -1):
            p = pl.points[end]
            near = [q for q in keys if math.hypot(q[0] - p[0], q[1] - p[1]) < SNAP]
            if near:
                pl.points[end] = near[0]
                continue
            if pl.attach is not None and end == 0:
                targets = [q for q in polys if q.ident == pl.attach]
                if not targets:
                    raise refuse(f"{style.name}.{pl.ident} : ligne porteuse {pl.attach} absente du niveau {level}")
                tol = ATTACH
            else:
                targets = [q for q in polys if q is not pl]
                tol = SNAP
            best = (math.inf, p)
            for q in targets:
                for a, b in itertools.pairwise(q.points):
                    dd, c = _project(p, a, b)
                    if dd < best[0]:
                        best = (dd, c)
            if best[0] < tol:
                pl.points[end] = best[1]
            elif pl.attach is not None and end == 0:
                raise refuse(f"{style.name}.{pl.ident} : origine a {best[0]:.4f} de sa ligne porteuse {pl.attach}")
        keys.extend(pl.points)
    for pl in polys:
        out = [pl.points[0]]
        for a, b in itertools.pairwise(pl.points):
            abx, aby = b[0] - a[0], b[1] - a[1]
            l2 = abx * abx + aby * aby
            if l2 == 0.0:
                raise refuse(f"{style.name}.{pl.ident} : segment de longueur nulle en {a}")
            cuts = []
            for k in keys:
                t = ((k[0] - a[0]) * abx + (k[1] - a[1]) * aby) / l2
                if 0.0 < t < 1.0 and _project(k, a, b)[0] < ON_SEGMENT and k != a and k != b:
                    cuts.append((t, k))
            out.extend(k for _t, k in sorted(set(cuts)))
            out.append(b)
        pl.points = out
    segs = [(pl.ident, a, b) for pl in polys for a, b in itertools.pairwise(pl.points)]
    for i, (ia, a, b) in enumerate(segs):
        for ib, c, d in segs[i + 1:]:
            if proper_cross(a, b, c, d):
                raise refuse(f"{style.name} : les lignes {ia} et {ib} se croisent pres de {a}")
    return polys


# ---------------------------------------------------------------------------------------------
# Echantillonnage et semis.

@dataclass
class Pslg:
    uy: list[Point]
    xy: list[Point]
    constraints: dict[tuple[int, int], tuple[str, str]]
    boundary: list[int]
    axis_edges: int


def sample(style: Style, polys: list[Polyline], h: float) -> Pslg:
    uy: list[Point] = []
    index: dict[Point, int] = {}

    def vid(p: Point) -> int:
        if p not in index:
            index[p] = len(uy)
            uy.append(p)
        return index[p]

    cons: dict[tuple[int, int], tuple[str, str]] = {}
    boundary: list[int] = []
    for pl in polys:
        chain: list[int] = []
        for a, b in itertools.pairwise(pl.points):
            ma, mb = style.to_metric(*a), style.to_metric(*b)
            n = max(1, math.ceil(math.hypot(mb[0] - ma[0], mb[1] - ma[1]) / h - 1e-9))
            for j in range(n):
                chain.append(vid(a if j == 0 else (a[0] + (b[0] - a[0]) * j / n, a[1] + (b[1] - a[1]) * j / n)))
        chain.append(vid(pl.points[-1]))
        for i, j in itertools.pairwise(chain):
            key = (min(i, j), max(i, j))
            if key in cons and cons[key] != (pl.kind, pl.ident):
                raise refuse(f"{style.name} : {pl.ident} et {cons[key][1]} partagent un segment")
            cons[key] = (pl.kind, pl.ident)
        if pl.kind == "contour":
            boundary = chain[:-1]
    axis = sum(1 for i, j in zip(boundary, boundary[1:] + boundary[:1]) if uy[i][0] == 0.0 and uy[j][0] == 0.0)
    return Pslg(uy, [style.to_metric(*p) for p in uy], cons, boundary, axis)


def _inside(poly: np.ndarray, pts: np.ndarray) -> np.ndarray:
    x, y = pts[:, 0], pts[:, 1]
    res = np.zeros(len(pts), bool)
    for i in range(len(poly)):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % len(poly)]
        if y1 == y2:
            continue
        crosses = (y1 > y) != (y2 > y)
        xi = x1 + (y - y1) * ((x2 - x1) / (y2 - y1))
        res ^= crosses & (x < xi)
    return res


def _segment_distance(pts: np.ndarray, a: np.ndarray, b: np.ndarray) -> np.ndarray:
    """Distance de chaque point au plus proche des segments [a_k, b_k] ; element par element."""
    best = np.full(len(pts), np.inf)
    abx, aby = b[:, 0] - a[:, 0], b[:, 1] - a[:, 1]
    l2 = abx * abx + aby * aby
    for k in range(len(a)):
        px, py = pts[:, 0] - a[k, 0], pts[:, 1] - a[k, 1]
        t = np.clip((px * abx[k] + py * aby[k]) / l2[k], 0.0, 1.0)
        dx, dy = px - t * abx[k], py - t * aby[k]
        best = np.minimum(best, np.sqrt(dx * dx + dy * dy))
    return best


def seed(style: Style, pslg: Pslg, h: float) -> list[Point]:
    poly = np.array([pslg.xy[i] for i in pslg.boundary])
    xmin, ymin = poly.min(0)
    xmax, ymax = poly.max(0)
    dy = h * math.sqrt(3.0) / 2.0
    grid = [(xmin + (i + 0.5 * (j % 2)) * h, ymin + j * dy)
            for j in range(int((ymax - ymin) / dy) + 2) for i in range(int((xmax - xmin) / h) + 2)]
    if not grid:
        return []
    g = np.array(grid)
    keep = _inside(poly, g)
    g = g[keep]
    if not len(g):
        return []
    segs = list(pslg.constraints)
    a = np.array([pslg.xy[i] for i, _ in segs])
    b = np.array([pslg.xy[j] for _, j in segs])
    g = g[_segment_distance(g, a, b) > SEED_CLEARANCE * h]
    return [(float(x), float(y)) for x, y in g]


def predicted_counts(pslg: Pslg, seeds: int) -> dict[str, int]:
    """Comptes de la demi-feuille et de la coque fermee (deux faces et murs de bord, axe exclu)."""
    v = len(pslg.uy) + seeds
    b = len(pslg.boundary)
    t = 2 * v - b - 2
    axis_vertices = pslg.axis_edges + 1
    walls = b - pslg.axis_edges
    return {"vertices": v, "triangles": t, "shellTriangles": 4 * t + 4 * walls,
            "shellVertices": 2 * (2 * v - axis_vertices)}


# ---------------------------------------------------------------------------------------------
# Triangulation contrainte.

def _hilbert(x: int, y: int, order: int) -> int:
    d, s = 0, order // 2
    while s > 0:
        rx = 1 if x & s else 0
        ry = 1 if y & s else 0
        d += s * s * ((3 * rx) ^ ry)
        if ry == 0:
            if rx == 1:
                x, y = s - 1 - x, s - 1 - y
            x, y = y, x
        s //= 2
    return d


class Triangulation:
    """Triangles directs ; dictionnaire des aretes orientees (a, b) -> triangle qui les porte."""

    def __init__(self, pts: list[Point]) -> None:
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        cx, cy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
        r = 64.0 * max(max(xs) - min(xs), max(ys) - min(ys), 1.0)
        self.n = len(pts)
        self.p = list(pts) + [(cx - r, cy - r), (cx + r, cy - r), (cx, cy + r)]
        self.tris: list[list[int] | None] = [[self.n, self.n + 1, self.n + 2]]
        self.edges: dict[tuple[int, int], int] = {}
        self._register(0)
        self.last = 0

    def _register(self, t: int) -> None:
        a, b, c = self.tris[t]
        self.edges[(a, b)] = t
        self.edges[(b, c)] = t
        self.edges[(c, a)] = t

    def _unregister(self, t: int) -> None:
        a, b, c = self.tris[t]
        for e in ((a, b), (b, c), (c, a)):
            del self.edges[e]

    def _locate(self, q: Point) -> int:
        t = self.last
        for _ in range(4 * len(self.tris) + 16):
            a, b, c = self.tris[t]
            for x, y in ((a, b), (b, c), (c, a)):
                if orient(self.p[x], self.p[y], q) < 0:
                    t = self.edges[(y, x)]
                    break
            else:
                return t
        raise refuse("localisation : marche sans fin")

    def insert(self, i: int) -> None:
        q = self.p[i]
        t0 = self._locate(q)
        cavity = {t0}
        rejected: set[int] = set()
        stack = [t0]
        while stack:
            t = stack.pop()
            a, b, c = self.tris[t]
            for x, y in ((a, b), (b, c), (c, a)):
                n = self.edges.get((y, x))
                if n is None or n in cavity or n in rejected:
                    continue
                na, nb, nc = self.tris[n]
                if incircle(self.p[na], self.p[nb], self.p[nc], q) > 0:
                    cavity.add(n)
                    stack.append(n)
                else:
                    rejected.add(n)
        rim = []
        for t in sorted(cavity):
            a, b, c = self.tris[t]
            for x, y in ((a, b), (b, c), (c, a)):
                if self.edges.get((y, x)) not in cavity:
                    rim.append((x, y))
        free = sorted(cavity)
        for t in free:
            self._unregister(t)
            self.tris[t] = None
        for x, y in rim:
            if orient(self.p[x], self.p[y], q) <= 0:
                raise refuse(f"insertion du sommet {i} : cavite non etoilee")
            t = free.pop(0) if free else len(self.tris)
            if t == len(self.tris):
                self.tris.append(None)
            self.tris[t] = [x, y, i]
            self._register(t)
            self.last = t

    def _third(self, t: int, x: int, y: int) -> int:
        return next(v for v in self.tris[t] if v != x and v != y)

    def flip(self, x: int, y: int) -> tuple[int, int] | None:
        """Bascule de l'arete xy si le quadrilatere est strictement convexe ; rend la nouvelle arete."""
        t1, t2 = self.edges.get((x, y)), self.edges.get((y, x))
        if t1 is None or t2 is None:
            return None
        r, s = self._third(t1, x, y), self._third(t2, y, x)
        if not proper_cross(self.p[x], self.p[y], self.p[r], self.p[s]):
            return None
        self._unregister(t1)
        self._unregister(t2)
        self.tris[t1] = [r, x, s]
        self.tris[t2] = [s, y, r]
        self._register(t1)
        self._register(t2)
        return (r, s)

    def _crossing(self, a: int, b: int) -> list[tuple[int, int]]:
        pa, pb = self.p[a], self.p[b]
        start = None
        for t, tri in enumerate(self.tris):
            if tri is None or a not in tri:
                continue
            k = tri.index(a)
            x, y = tri[(k + 1) % 3], tri[(k + 2) % 3]
            if orient(pa, pb, self.p[x]) == 0 and _between(pa, pb, self.p[x]):
                raise refuse(f"le sommet {x} est sur la contrainte {a}-{b}")
            if orient(pa, pb, self.p[y]) == 0 and _between(pa, pb, self.p[y]):
                raise refuse(f"le sommet {y} est sur la contrainte {a}-{b}")
            if proper_cross(pa, pb, self.p[x], self.p[y]):
                start = (x, y)
                break
        if start is None:
            raise refuse(f"contrainte {a}-{b} : aucune arete croisee depuis {a}")
        out = [start]
        x, y = start
        for _ in range(len(self.tris) + 1):
            t = self.edges[(y, x)]
            v = self._third(t, y, x)
            if v == b:
                return out
            o = orient(pa, pb, self.p[v])
            if o == 0:
                raise refuse(f"le sommet {v} est sur la contrainte {a}-{b}")
            # (x, y) : x a droite de ab, y a gauche ; v remplace celui de son cote.
            x, y = (x, v) if o > 0 else (v, y)
            out.append((x, y))
        raise refuse(f"contrainte {a}-{b} : traversee sans fin")

    def enforce(self, a: int, b: int) -> int:
        """Sloan : bascule les aretes qui croisent ab jusqu'a ce que ab soit une arete."""
        if (a, b) in self.edges or (b, a) in self.edges:
            return 0
        queue = deque(self._crossing(a, b))
        flips = 0
        stall = 0
        pa, pb = self.p[a], self.p[b]
        while queue:
            x, y = queue.popleft()
            e = self.flip(x, y)
            if e is None:
                queue.append((x, y))
                stall += 1
                if stall > len(queue) + 1:
                    raise refuse(f"contrainte {a}-{b} : aucune bascule possible")
                continue
            stall = 0
            flips += 1
            r, s = e
            if a not in e and b not in e and proper_cross(pa, pb, self.p[r], self.p[s]):
                queue.append(e)
        if (a, b) not in self.edges and (b, a) not in self.edges:
            raise refuse(f"contrainte {a}-{b} absente apres bascules")
        return flips

    def lawson(self, fixed: set[tuple[int, int]]) -> int:
        stack = sorted({(min(x, y), max(x, y)) for (x, y) in self.edges})
        stack.reverse()
        flips = 0
        while stack:
            x, y = stack.pop()
            if (x, y) in fixed:
                continue
            t1, t2 = self.edges.get((x, y)), self.edges.get((y, x))
            if t1 is None or t2 is None:
                continue
            a, b, c = self.tris[t1]
            s = self._third(t2, y, x)
            if incircle(self.p[a], self.p[b], self.p[c], self.p[s]) <= 0:
                continue
            r = self._third(t1, x, y)
            if self.flip(x, y) is None:
                continue
            flips += 1
            for e in ((x, s), (s, y), (y, r), (r, x)):
                stack.append((min(e), max(e)))
        return flips


def _between(a: Point, b: Point, c: Point) -> bool:
    return min(a[0], b[0]) <= c[0] <= max(a[0], b[0]) and min(a[1], b[1]) <= c[1] <= max(a[1], b[1]) and c != a and c != b


def triangulate(pts: list[Point], constraints: dict[tuple[int, int], tuple[str, str]],
                contour: set[tuple[int, int]]) -> tuple[list[tuple[int, int, int]], int]:
    """CDT du domaine borne par `contour` ; rend les triangles interieurs directs et les bascules."""
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    x0, y0 = min(xs), min(ys)
    span = max(max(xs) - x0, max(ys) - y0, 1e-300)
    order = 1 << 16
    ranks = sorted(range(len(pts)), key=lambda i: (_hilbert(int((pts[i][0] - x0) / span * (order - 1)),
                                                            int((pts[i][1] - y0) / span * (order - 1)), order), i))
    tri = Triangulation(pts)
    for i in ranks:
        tri.insert(i)
    flips = sum(tri.enforce(a, b) for (a, b) in constraints)
    flips += tri.lawson(set(constraints))
    for a, b in constraints:
        if (a, b) not in tri.edges and (b, a) not in tri.edges:
            raise refuse(f"contrainte {a}-{b} perdue")
    n = tri.n
    outside = set()
    stack = [t for t, v in enumerate(tri.tris) if v is not None and max(v) >= n]
    outside.update(stack)
    while stack:
        t = stack.pop()
        a, b, c = tri.tris[t]
        for x, y in ((a, b), (b, c), (c, a)):
            if (min(x, y), max(x, y)) in contour:
                continue
            m = tri.edges.get((y, x))
            if m is not None and m not in outside:
                outside.add(m)
                stack.append(m)
    inner = [tuple(v) for t, v in enumerate(tri.tris) if v is not None and t not in outside]
    if any(max(v) >= n for v in inner):
        raise refuse("le contour ne ferme pas le domaine")
    return inner, flips


# ---------------------------------------------------------------------------------------------
# Regions et relief.

def label_regions(style: Style, xy: list[Point], tris: list[tuple[int, int, int]],
                  constraints: dict[tuple[int, int], tuple[str, str]]) -> list[int]:
    adj: dict[tuple[int, int], list[int]] = {}
    for k, (a, b, c) in enumerate(tris):
        for x, y in ((a, b), (b, c), (c, a)):
            adj.setdefault((min(x, y), max(x, y)), []).append(k)
    label = [-1] * len(tris)
    for r, reg in enumerate(style.regions):
        s = style.to_metric(*reg.seed)
        hits = [k for k, (a, b, c) in enumerate(tris)
                if orient(xy[a], xy[b], s) >= 0 and orient(xy[b], xy[c], s) >= 0 and orient(xy[c], xy[a], s) >= 0]
        if not hits:
            raise refuse(f"{style.name}.{reg.ident} : graine {reg.seed} hors du domaine")
        k0 = hits[0]
        if label[k0] >= 0:
            raise refuse(f"{style.name}.{reg.ident} : graine dans la region {style.regions[label[k0]].ident}")
        label[k0] = r
        stack = [k0]
        while stack:
            k = stack.pop()
            a, b, c = tris[k]
            for x, y in ((a, b), (b, c), (c, a)):
                key = (min(x, y), max(x, y))
                if constraints.get(key, ("", ""))[0] == "step":
                    continue
                for j in adj[key]:
                    if label[j] < 0:
                        label[j] = r
                        stack.append(j)
    missing = [k for k, v in enumerate(label) if v < 0]
    if missing:
        a, b, c = tris[missing[0]]
        g = style.to_uy((xy[a][0] + xy[b][0] + xy[c][0]) / 3.0, (xy[a][1] + xy[b][1] + xy[c][1]) / 3.0)
        raise refuse(f"{style.name} : {len(missing)} triangles sans region, dont un vers (u, y) = "
                     f"({g[0]:.3f}, {g[1]:.3f})")
    for key, (kind, ident) in constraints.items():
        if kind == "step" and len(adj.get(key, [])) == 2 and label[adj[key][0]] == label[adj[key][1]]:
            raise refuse(f"{style.name}.{ident} : ligne step a l'interieur de la region "
                         f"{style.regions[label[adj[key][0]]].ident}")
    return label


def _smootherstep(a: float, b: float, x: np.ndarray) -> np.ndarray:
    """Rampe C2 de a a b : 6 t^5 - 15 t^4 + 10 t^3."""
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * t * (10.0 - 15.0 * t + 6.0 * t * t)


def _bump(x: np.ndarray) -> np.ndarray:
    t = np.maximum(0.0, 1.0 - x)
    return t * t


def relief(style: Style, uy: list[Point], xy: list[Point], tris: list[tuple[int, int, int]], label: list[int],
           constraints: dict[tuple[int, int], tuple[str, str]]) -> tuple[np.ndarray, np.ndarray, list[int], np.ndarray]:
    """H sur la face exterieure, Hb sur le revers ; region du sommet = region incidente de H maximal ; relief
    de la tige evalue en chaque sommet (pour la bande)."""
    rp = style.relief
    n = len(xy)
    X = np.array(xy)
    y = np.array([p[1] for p in uy])
    regs: list[set[int]] = [set() for _ in range(n)]
    for k, t in enumerate(tris):
        for v in t:
            regs[v].add(label[k])

    def dist(pred) -> np.ndarray:
        segs = [(a, b) for (a, b), (kind, ident) in constraints.items() if pred(kind, ident, a, b)]
        if not segs:
            return np.full(n, 1e9)
        return _segment_distance(X, np.array([xy[a] for a, _ in segs]), np.array([xy[b] for _, b in segs]))

    on_axis = [p[0] == 0.0 for p in uy]
    d_edge = dist(lambda k, i, a, b: k == "contour" and not (on_axis[a] and on_axis[b]))
    d_step = dist(lambda k, i, a, b: k == "step")
    d_flute = dist(lambda k, i, a, b: k == "flute")
    d_ridge = dist(lambda k, i, a, b: k == "ridge")
    d_veinlet = dist(lambda k, i, a, b: k == "veinlet")
    tips = np.array([style.to_metric(*p) for p, t in zip(style.contour, style.tags) if t == "tip"])
    if len(tips):
        d_tip = np.full(n, np.inf)
        for tx, ty in tips:
            dx, dy = X[:, 0] - tx, X[:, 1] - ty
            d_tip = np.minimum(d_tip, np.sqrt(dx * dx + dy * dy))
    else:
        d_tip = np.full(n, 1e9)
    fade = 1.0 - _smootherstep(rp["fade"][0], rp["fade"][1], y)
    stem = rp["stem"]
    h_stem = (stem["height"] + stem["round"] * np.sqrt(np.minimum(1.0, d_step / stem["radius"]))
              - rp["flute"]["depth"] * _bump(d_flute / rp["flute"]["radius"]))
    best = np.full(n, -np.inf)
    best_region = np.full(n, -1)
    for r, reg in enumerate(style.regions):
        mask = np.array([r in s for s in regs])
        if not mask.any():
            continue
        if reg.vein is None:
            h = h_stem
        else:
            dv = dist(lambda k, i, a, b, v=reg.vein: i == v)
            rho = dv / np.maximum(dv + d_edge, 1e-9)
            h = (rp["layer"] * reg.layer + rp["cup"] * reg.cup * rho * rho
                 - rp["vein"]["depth"] * _bump(dv / rp["vein"]["radius"])
                 - rp["veinlet"]["depth"] * _bump(d_veinlet / rp["veinlet"]["radius"])
                 + rp["tip"]["height"] * _bump(d_tip / rp["tip"]["radius"]))
        upd = mask & (h > best)
        best = np.where(upd, h, best)
        best_region = np.where(upd, r, best_region)
    H = fade * best + (1.0 - fade) * rp["capOut"]
    edge = np.minimum(1.0, d_edge / rp["capBack"]["radius"])
    Hb = (1.0 - fade) * (rp["capBack"]["height"] * np.sqrt(edge)
                         + rp["ridge"]["height"] * _bump(d_ridge / rp["ridge"]["radius"]))
    return H, Hb, [int(v) for v in best_region], fade * h_stem + (1.0 - fade) * rp["capOut"]


def min_angles(xy: list[Point], tris: list[tuple[int, int, int]]) -> list[float]:
    out = []
    for t in tris:
        p = [xy[v] for v in t]
        m = 180.0
        for k in range(3):
            a, b, c = p[k], p[(k + 1) % 3], p[(k + 2) % 3]
            ux, uy_, vx, vy = b[0] - a[0], b[1] - a[1], c[0] - a[0], c[1] - a[1]
            m = min(m, math.degrees(math.atan2(abs(ux * vy - uy_ * vx), ux * vx + uy_ * vy)))
        out.append(m)
    return out


# ---------------------------------------------------------------------------------------------
# Compilation d'un niveau.

@dataclass
class Level:
    name: str
    h: float
    uy: list[Point]
    xy: list[Point]
    tris: list[tuple[int, int, int]]
    tri_region: list[int]
    vertex_region: list[int]
    H: np.ndarray
    Hb: np.ndarray
    constraints: dict[tuple[int, int], tuple[str, str]]
    counts: dict[str, int]
    min_angle: float
    flips: int
    seconds: float
    band: Band


@dataclass
class Band:
    uy: list[Point]
    H: list[float]
    tris: list[tuple[int, int, int]]
    region: int
    min_angle: float
    counts: dict[str, int]


def shell_counts(uy: list[Point], tris: list[tuple[int, int, int]]) -> dict[str, int]:
    """Comptes de la demi-feuille et de sa coque fermee (deux faces et murs de bord, couture d'axe exclue)."""
    edges: dict[tuple[int, int], int] = {}
    for a, b, c in tris:
        for x, y in ((a, b), (b, c), (c, a)):
            key = (min(x, y), max(x, y))
            edges[key] = edges.get(key, 0) + 1
    bad = [k for k, n in edges.items() if n > 2]
    if bad:
        raise refuse(f"arete {bad[0]} partagee par plus de deux triangles")
    border = [k for k, n in edges.items() if n == 1]
    axis = sum(1 for a, b in border if uy[a][0] == 0.0 and uy[b][0] == 0.0)
    axis_vertices = sum(1 for u, _y in uy if u == 0.0)
    v, t = len(uy), len(tris)
    return {"vertices": v, "triangles": t, "shellTriangles": 4 * t + 4 * (len(border) - axis),
            "shellVertices": 2 * (2 * v - axis_vertices)}


def stem_region(style: Style) -> int:
    found = [r for r, reg in enumerate(style.regions) if reg.vein is None]
    if len(found) != 1:
        raise refuse(f"{style.name} : une seule region de tige attendue (sans vein), "
                     f"{len(found)} trouvee(s)")
    return found[0]


def _band_angle(p: list[Point]) -> float:
    return min_angles(p, [(0, 1, 2)])[0]


def build_band(style: Style, level: str, uy: list[Point], tris: list[tuple[int, int, int]],
               H_stem: np.ndarray) -> Band:
    """Bande de tige sous le bord de pied : colonnes u <= stem.u, rangs de quadrilateres ; relief de la tige
    recopie de la colonne."""
    st = style.stem
    where = f"{style.name}.{level}.stem"
    cols = sorted((i for i, (u, y) in enumerate(uy) if y == 0.0 and u <= st.u), key=lambda i: uy[i][0])
    if not cols or uy[cols[0]][0] != 0.0:
        raise refuse(f"{where} : pas de sommet de pied sur l'axe")
    if uy[cols[-1]][0] != st.u:
        raise refuse(f"{where} : pas de sommet de pied a u = stem.u = {st.u} (dernier : {uy[cols[-1]][0]})")
    edges = {(min(x, y), max(x, y)) for a, b, c in tris for x, y in ((a, b), (b, c), (c, a))}
    for a, b in itertools.pairwise(cols):
        if (min(a, b), max(a, b)) not in edges:
            raise refuse(f"{where} : colonnes {uy[a]} et {uy[b]} non reliees par une arete du bord de pied")
    region = stem_region(style)
    rows = st.rows[level]
    n0, nc = len(uy), len(cols)
    band_uy: list[Point] = []
    band_H: list[float] = []
    for k in range(1, rows + 1):
        t = k / rows
        f = 1.0 - (1.0 - st.taper) * t
        for c in cols:
            band_uy.append((uy[c][0] * f, -t))
            band_H.append(float(H_stem[c]))
    for u, y in band_uy:
        if not (0.0 <= u <= st.u and -1.0 <= y < 0.0):
            raise refuse(f"{where} : sommet de bande hors de [0, stem.u] x [-1, 0) : ({u}, {y})")

    def vid(k: int, i: int) -> int:
        return cols[i] if k == 0 else n0 + (k - 1) * nc + i

    def metric(v: int) -> Point:
        u, y = uy[v] if v < n0 else band_uy[v - n0]
        return (u * st.hw, y * st.length)

    out: list[tuple[int, int, int]] = []
    worst = 180.0
    for k in range(1, rows + 1):
        for i in range(nc - 1):
            a, b, c, d = vid(k - 1, i), vid(k - 1, i + 1), vid(k, i + 1), vid(k, i)
            options = (((a, d, c), (a, c, b)), ((a, d, b), (b, d, c)))
            scored = []
            for pair in options:
                angles = [_band_angle([metric(v) for v in tri]) for tri in pair]
                scored.append((min(angles), pair))
            best = max(scored, key=lambda s: s[0])
            for tri in best[1]:
                p = [metric(v) for v in tri]
                o = orient(*p)
                if o == 0:
                    raise refuse(f"{where} : triangle de bande plat au rang {k}, colonne {i}")
                out.append(tri if o > 0 else (tri[0], tri[2], tri[1]))
            worst = min(worst, best[0])
    floor = float(style.min_angle[level])
    if worst < floor:
        raise refuse(f"{where} : angle minimal de bande {worst:.2f} deg < {floor} deg")
    counts = shell_counts(uy + band_uy, tris + out)
    return Band(band_uy, band_H, out, region, worst, counts)


def find_step(style: Style, polys: list[Polyline], budget: float) -> tuple[float, Pslg, list[Point], dict]:
    """Bissection sur log h : compte de coque le plus proche du budget ; a egalite, le plus grand h."""
    def evaluate(h: float):
        p = sample(style, polys, h)
        s = seed(style, p, h)
        return p, s, predicted_counts(p, len(s))

    lo, hi = math.log(0.02 * style.hw), math.log(4.0 * style.hw)
    best = None
    for _ in range(BISECTION_STEPS):
        mid = 0.5 * (lo + hi)
        h = math.exp(mid)
        p, s, c = evaluate(h)
        gap = abs(c["shellTriangles"] - budget)
        if best is None or gap < best[0] or (gap == best[0] and h > best[1]):
            best = (gap, h, p, s, c)
        if c["shellTriangles"] > budget:
            lo = mid
        else:
            hi = mid
    _gap, h, p, s, c = best
    return h, p, s, c


def compile_level(style: Style, level: str) -> Level:
    t0 = time.perf_counter()
    polys = build_polylines(style, level)
    budget = float(style.budget[level])
    h, pslg, seeds, predicted = find_step(style, polys, budget)
    tol = float(style.budget["tolerance"])
    if abs(predicted["shellTriangles"] - budget) > tol * budget:
        raise refuse(f"{style.name}.{level} : {predicted['shellTriangles']} triangles de coque au mieux "
                     f"(h = {h:.4f}), budget {budget:.0f} a {tol:.0%} pres")
    xy = pslg.xy + seeds
    uy = pslg.uy + [style.to_uy(*p) for p in seeds]
    n = len(pslg.boundary)
    contour = {(min(pslg.boundary[k], pslg.boundary[(k + 1) % n]), max(pslg.boundary[k], pslg.boundary[(k + 1) % n]))
               for k in range(n)}
    tris, flips = triangulate(xy, pslg.constraints, contour)
    used = sorted({v for t in tris for v in t})
    if len(used) != len(xy):
        raise refuse(f"{style.name}.{level} : {len(xy) - len(used)} sommets hors du maillage")
    if len(tris) != predicted["triangles"]:
        raise refuse(f"{style.name}.{level} : {len(tris)} triangles, Euler en attend {predicted['triangles']}")
    label = label_regions(style, xy, tris, pslg.constraints)
    H, Hb, vreg, H_stem = relief(style, uy, xy, tris, label, pslg.constraints)
    if not (np.all(np.isfinite(H)) and np.all(np.isfinite(Hb))):
        raise refuse(f"{style.name}.{level} : relief non fini")
    angles = min_angles(xy, tris)
    worst = min(angles)
    floor = float(style.min_angle[level])
    if worst < floor:
        k = angles.index(worst)
        g = uy[tris[k][0]]
        raise refuse(f"{style.name}.{level} : angle minimal {worst:.2f} deg < {floor} deg, "
                     f"triangle {k} pres de (u, y) = ({g[0]:.3f}, {g[1]:.3f})")
    if shell_counts(uy, tris) != predicted:
        raise refuse(f"{style.name}.{level} : comptes du maillage {shell_counts(uy, tris)}, Euler en attend {predicted}")
    H_leaf, Hb_leaf = H.copy(), Hb.copy()
    band = build_band(style, level, uy, tris, H_stem)
    if not (np.array_equal(H, H_leaf) and np.array_equal(Hb, Hb_leaf)):
        raise refuse(f"{style.name}.{level} : la bande a modifie le relief de la feuille")
    return Level(level, h, uy, xy, tris, label, vreg, H, Hb, pslg.constraints, predicted, worst, flips,
                 time.perf_counter() - t0, band)


# ---------------------------------------------------------------------------------------------
# Rendu de la sortie.

def fmt(x: float, digits: int) -> str:
    s = f"{x:.{digits}f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def render_level(style: Style, lv: Level) -> tuple[list[str], list[str]]:
    """Sommets et triangles de la feuille, puis ceux de la bande de tige."""
    bd = lv.band
    uy = lv.uy + bd.uy
    H = [float(h) for h in lv.H] + bd.H
    Hb = [float(h) for h in lv.Hb] + [0.0] * len(bd.uy)
    vreg = list(lv.vertex_region) + [bd.region] * len(bd.uy)
    tris = lv.tris + bd.tris
    treg = list(lv.tri_region) + [bd.region] * len(bd.tris)
    verts = [f"[{fmt(u, DIGITS_UY)}, {fmt(y, DIGITS_UY)}, {fmt(h, DIGITS_RELIEF)}, {fmt(hb, DIGITS_RELIEF)}, {r}]"
             for (u, y), h, hb, r in zip(uy, H, Hb, vreg)]
    leaf_only = [f"[{fmt(u, DIGITS_UY)}, {fmt(y, DIGITS_UY)}, {fmt(float(h), DIGITS_RELIEF)}, "
                 f"{fmt(float(hb), DIGITS_RELIEF)}, {r}]"
                 for (u, y), h, hb, r in zip(lv.uy, lv.H, lv.Hb, lv.vertex_region)]
    if verts[:len(lv.uy)] != leaf_only:
        raise refuse(f"{style.name}.{lv.name} : le prefixe feuille differe de la feuille compilee sans bande")
    columns = {i for i, (u, y) in enumerate(lv.uy) if y == 0.0 and u <= style.stem.u}
    shared = {v for t in bd.tris for v in t if v < len(lv.uy)}
    if any(v >= len(lv.uy) for t in lv.tris for v in t) or shared != columns:
        raise refuse(f"{style.name}.{lv.name} : la bande ne partage pas exactement le bord de pied de la feuille")
    rounded = [(float(fmt(u, DIGITS_UY)), float(fmt(y, DIGITS_UY))) for u, y in uy]
    for p0, (u, _y) in zip(rounded, uy):
        if u == 0.0 and p0[0] != 0.0:
            raise refuse(f"{style.name}.{lv.name} : sommet d'axe deplace par l'arrondi")
    for k, (a, b, c) in enumerate(tris):
        pa, pb, pc = (style.to_metric(*rounded[v]) for v in (a, b, c))
        if orient(pa, pb, pc) <= 0:
            raise refuse(f"{style.name}.{lv.name} : triangle {k} retourne ou plat apres arrondi")
    return verts, [f"[{a}, {b}, {c}, {r}]" for (a, b, c), r in zip(tris, treg)]


def _counts(c: dict[str, int]) -> str:
    return (f'{{"vertices": {c["vertices"]}, "triangles": {c["triangles"]}, '
            f'"shellVertices": {c["shellVertices"]}, "shellTriangles": {c["shellTriangles"]}}}')


def render(styles: list[Style], levels: dict[str, dict[str, Level]], drawing: bytes) -> bytes:
    out = ["{",
           f'  "format": "{LEAVES_FORMAT}",',
           f'  "derivedBy": "{DERIVED_BY}",',
           f'  "drawing": {{"sha256": "{hashlib.sha256(drawing).hexdigest()}", "bytes": {len(drawing)}}},',
           '  "vertex": "[u, y, H, Hb, region] ; y < 0 : bande de tige",',
           '  "triangle": "[a, b, c, region], sens trigonometrique dans (u, y)",',
           ('  "prefix": "feuille seule (F) : leafVertices premiers sommets, leafTriangles premiers triangles ; '
            'feuille et bande (E) : le tout",'),
           '  "styles": {']
    for si, style in enumerate(styles):
        out.append(f'    "{style.name}": {{')
        regs = ", ".join(json.dumps({"id": r.ident, "layer": r.layer, "cup": r.cup}) for r in style.regions)
        out.append(f'      "regions": [{regs}],')
        tips = ", ".join(f"[{fmt(u, DIGITS_UY)}, {fmt(y, DIGITS_UY)}]"
                         for (u, y), t in zip(style.contour, style.tags) if t == "tip")
        out.append(f'      "tips": [{tips}],')
        outline = ", ".join(f"[{fmt(u, DIGITS_UY)}, {fmt(y, DIGITS_UY)}]" for u, y in style.contour)
        out.append(f'      "outline": [{outline}],')
        st = style.stem
        out.append(f'      "stem": {{"u": {fmt(st.u, DIGITS_UY)}, "taper": {fmt(st.taper, DIGITS_UY)}, '
                   f'"region": {stem_region(style)}}},')
        out.append(f'      "nominal": {{"hw": {fmt(style.hw, DIGITS_UY)}, "body": {fmt(style.body, DIGITS_UY)}, '
                   f'"curl": {fmt(style.curl, DIGITS_UY)}}},')
        rp = style.relief
        out.append(f'      "relief": {{"backHeight": {fmt(float(rp["capBack"]["height"]), DIGITS_RELIEF)}, '
                   f'"fade": [{fmt(float(rp["fade"][0]), DIGITS_UY)}, {fmt(float(rp["fade"][1]), DIGITS_UY)}]}},')
        for li, level in enumerate(LEVELS):
            lv = levels[style.name][level]
            verts, tris = render_level(style, lv)
            out.append(f'      "{level}": {{')
            out.append(f'        "h": {fmt(lv.h, 6)},')
            out.append(f'        "leafVertices": {len(lv.uy)},')
            out.append(f'        "leafTriangles": {len(lv.tris)},')
            out.append(f'        "counts": {_counts(lv.counts)},')
            out.append(f'        "countsWithStem": {_counts(lv.band.counts)},')
            out.append('        "vertices": [')
            out.append(",\n".join("  " + v for v in verts))
            out.append('        ],')
            out.append('        "triangles": [')
            out.append(",\n".join("  " + t for t in tris))
            out.append('        ]')
            out.append("      }" + ("," if li + 1 < len(LEVELS) else ""))
        out.append("    }" + ("," if si + 1 < len(styles) else ""))
    out.append("  }")
    out.append("}")
    return ("\n".join(out) + "\n").encode("utf-8")


def compile_all(drawing: bytes, log=None) -> tuple[list[Style], dict[str, dict[str, Level]]]:
    styles = parse_drawing(drawing)
    levels: dict[str, dict[str, Level]] = {}
    errors: list[str] = []
    for style in styles:
        levels[style.name] = {}
        for level in LEVELS:
            try:
                lv = compile_level(style, level)
            except DrawingError as e:
                errors.extend(e.errors)
                continue
            levels[style.name][level] = lv
            if log is not None:
                c = lv.counts
                log(f"{style.name} {level} : h {lv.h:.4f} ; demi-feuille {c['vertices']} sommets, "
                    f"{c['triangles']} triangles ; coque {c['shellTriangles']} triangles, {c['shellVertices']} "
                    f"sommets (budget {style.budget[level]}) ; angle min {lv.min_angle:.2f} deg ; "
                    f"{lv.flips} bascules ; {lv.seconds:.2f} s ; avec bande {lv.band.counts['shellTriangles']} triangles, "
                    f"{lv.band.counts['shellVertices']} sommets, angle min de bande {lv.band.min_angle:.2f} deg")
    if errors:
        raise DrawingError(errors)
    return styles, levels


# ---------------------------------------------------------------------------------------------
# Superposition.

KIND_COLOURS = {"contour": (255, 230, 0), "step": (230, 30, 30), "vein": (40, 90, 255), "veinlet": (0, 200, 220),
                "flute": (200, 60, 220), "ridge": (40, 210, 60)}
REGION_TINTS = [(255, 220, 120), (120, 220, 120), (120, 170, 255), (255, 130, 170), (200, 150, 255),
                (255, 170, 90), (150, 230, 220)]


def affine(anchors: list[list[float]]):
    """Application (u, y) -> (px, py) passant par trois ancrages [u, y, px, py]."""
    if len(anchors) != 3:
        raise refuse("trace.anchors : trois ancrages [u, y, px, py] attendus")
    (u1, y1, _, _), (u2, y2, _, _), (u3, y3, _, _) = anchors
    det = u1 * (y2 - y3) - y1 * (u2 - u3) + (u2 * y3 - u3 * y2)
    if abs(det) < 1e-12:
        raise refuse("trace.anchors : ancrages alignes")

    def solve(col: int) -> tuple[float, float, float]:
        v1, v2, v3 = (a[col] for a in anchors)
        a = (v1 * (y2 - y3) - y1 * (v2 - v3) + (v2 * y3 - v3 * y2)) / det
        b = (u1 * (v2 - v3) - v1 * (u2 - u3) + (u2 * v3 - u3 * v2)) / det
        c = (u1 * (y2 * v3 - y3 * v2) - y1 * (u2 * v3 - u3 * v2) + v1 * (u2 * y3 - u3 * y2)) / det
        return a, b, c

    ax, bx, cx = solve(2)
    ay, by, cy = solve(3)
    return lambda u, y: (ax * u + bx * y + cx, ay * u + by * y + cy)


def trace_view(style: Style, name: str | None) -> tuple[str, dict]:
    """Calage nomme : trace.views[name], ou trace lui-meme s'il porte ses ancrages ; sans nom, le premier."""
    trace = style.trace or {}
    views = trace.get("views", {"trace": trace} if "anchors" in trace else {})
    if not views:
        raise refuse(f"{style.name} : aucun calage (trace.anchors ou trace.views) dans le dessin")
    key = name if name is not None else next(iter(views))
    if key not in views or "anchors" not in views[key]:
        raise refuse(f"{style.name} : calage {key!r} absent ou sans ancrages ({', '.join(views)})")
    return key, views[key]


def overlay(style: Style, image: Path, out: Path, view: str | None = None) -> None:
    from PIL import Image, ImageDraw

    view, trace = trace_view(style, view)
    body = affine(trace["anchors"])
    # La crosse s'enroule : vue de face, sa hauteur apparente n'est pas affine en y. trace.curl,
    # table [y, y apparent] croissante en y sur [1, 2], la donne en unites du corps.
    table = trace.get("curl", [[1.0, 1.0], [2.0, 2.0]])
    if [p[0] for p in table] != sorted(p[0] for p in table) or table[0][0] != 1.0 or table[-1][0] != 2.0:
        raise refuse(f"{style.name} : trace.curl doit couvrir [1, 2] en y croissant")

    # Bande de tige : trace.stem, longueur apparente de la bande en unites du corps ; sans elle, la
    # bande n'est pas tracee.
    stem_scale = trace.get("stem")

    def m(u: float, y: float) -> tuple[float, float]:
        if y > 1.0:
            for (y0, a0), (y1, a1) in itertools.pairwise(table):
                if y <= y1:
                    y = a0 + (a1 - a0) * (y - y0) / (y1 - y0)
                    break
        elif y < 0.0:
            y = y * stem_scale
        return body(u, y)

    lv = compile_level(style, "light")
    hv = compile_level(style, "heavy")
    ref = Image.open(image).convert("RGB")
    pts = [m(s * u, y) for u, y in style.contour for s in (1.0, -1.0)]
    if stem_scale is not None:
        pts += [m(s * u, y) for u, y in lv.band.uy for s in (1.0, -1.0)]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    pad = 0.12 * (max(ys) - min(ys))
    box = (int(min(xs) - pad), int(min(ys) - pad), int(max(xs) + pad), int(max(ys) + pad))
    k = max(1, round(1100 / (box[3] - box[1])))
    base = ref.crop(box).resize(((box[2] - box[0]) * k, (box[3] - box[1]) * k), Image.LANCZOS)

    def px(u: float, y: float) -> tuple[float, float]:
        x, yy = m(u, y)
        return ((x - box[0]) * k, (yy - box[1]) * k)

    tint = Image.new("RGBA", base.size, (0, 0, 0, 0))
    td = ImageDraw.Draw(tint)
    for (a, b, c), r in zip(lv.tris, lv.tri_region):
        col = REGION_TINTS[r % len(REGION_TINTS)] + (70,)
        for s in (1.0, -1.0):
            td.polygon([px(s * lv.uy[v][0], lv.uy[v][1]) for v in (a, b, c)], fill=col)
    over = Image.alpha_composite(base.convert("RGBA"), tint).convert("RGB")
    d = ImageDraw.Draw(over)
    if stem_scale is not None:
        allv = hv.uy + hv.band.uy
        for tri in hv.band.tris:
            for s in (1.0, -1.0):
                d.polygon([px(s * allv[v][0], allv[v][1]) for v in tri], outline=(255, 150, 0))
    for (a, b), (kind, _ident) in sorted(hv.constraints.items(), key=lambda e: e[1][0] != "contour"):
        width = 3 if kind in ("contour", "step") else 2
        for s in (1.0, -1.0):
            d.line([px(s * hv.uy[a][0], hv.uy[a][1]), px(s * hv.uy[b][0], hv.uy[b][1])],
                   fill=KIND_COLOURS[kind], width=width)
    for (u, y), t in zip(style.contour, style.tags):
        if t == "tip":
            x, yy = px(u, y)
            d.ellipse([x - 4, yy - 4, x + 4, yy + 4], outline=(255, 255, 255), width=2)
    for u, y, _, _ in trace["anchors"]:
        x, yy = px(u, y)
        d.line([x - 12, yy, x + 12, yy], fill=(0, 255, 255), width=3)
        d.line([x, yy - 12, x, yy + 12], fill=(0, 255, 255), width=3)
    sheet = Image.new("RGB", (2 * base.size[0] + 20, base.size[1] + 60), "white")
    sheet.paste(base, (0, 60))
    sheet.paste(over, (base.size[0] + 20, 60))
    sd = ImageDraw.Draw(sheet)
    legend = ("contour jaune, step rouge, nervure bleue, nervilles cyan, cannelures violettes, cotes vertes "
              "(revers) ; regions teintees (light) ; pointes blanches ; ancrages cyan")
    sd.text((10, 8), f"{style.name} : gabarit sur {image.name}, calage {view} par 3 points {trace['anchors']}",
            fill=(0, 0, 0))
    sd.text((10, 30), legend, fill=(0, 0, 0))
    sheet.save(out)


# ---------------------------------------------------------------------------------------------

def write_replacing(out: Path, data: bytes) -> None:
    """Ecrit dans un fichier temporaire voisin puis le substitue : une ecriture interrompue
    laisse l'ancienne sortie intacte."""
    tmp = out.with_name(out.name + ".tmp")
    try:
        tmp.write_bytes(data)
        os.replace(tmp, out)
    finally:
        tmp.unlink(missing_ok=True)


def check(derived: bytes, out: Path, drawing: bytes) -> list[str]:
    try:
        existing = out.read_bytes()
    except OSError as e:
        return [f"{out} : illisible ({e})"]
    if existing.replace(b"\r\n", b"\n") == derived:
        return []
    problems = []
    try:
        old = json.loads(existing.decode("utf-8"))
        sha = hashlib.sha256(drawing).hexdigest()
        if old.get("drawing", {}).get("sha256") != sha:
            problems.append(f"empreinte du dessin : sortie {old.get('drawing', {}).get('sha256')}, dessin lu {sha}")
    except (UnicodeDecodeError, json.JSONDecodeError, AttributeError) as e:
        problems.append(f"{out} : JSON illisible ({e})")
    problems.append("la sortie existante n'est pas la compilation du dessin")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=f"Compile {DEFAULT_DRAWING.name} en {DEFAULT_OUT.name}.")
    parser.add_argument("--drawing", type=Path, default=DEFAULT_DRAWING, help="dessin source")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="sortie a ecrire ou a comparer")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="compiler et ecrire la sortie")
    mode.add_argument("--check", action="store_true", help="comparer la compilation a la sortie existante")
    mode.add_argument("--overlay", action="store_true", help="superposer le gabarit a l'image de reference")
    parser.add_argument("--style", help="style a superposer (--overlay)")
    parser.add_argument("--image", type=Path, help="image de reference (--overlay)")
    parser.add_argument("--overlay-out", type=Path, help="planche PNG a ecrire (--overlay)")
    parser.add_argument("--trace", help="calage nomme de trace.views (--overlay) ; defaut : le premier")
    args = parser.parse_args(argv)

    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    try:
        drawing = args.drawing.read_bytes()
    except OSError as e:
        print(f"dessin illisible : {e}", file=sys.stderr)
        return 1
    try:
        if args.overlay:
            if not (args.style and args.image and args.overlay_out):
                parser.error("--overlay demande --style, --image et --overlay-out")
            styles = {s.name: s for s in parse_drawing(drawing)}
            if args.style not in styles:
                print(f"REFUS style {args.style!r} absent du dessin ({', '.join(styles)})", file=sys.stderr)
                return 1
            overlay(styles[args.style], args.image, args.overlay_out, args.trace)
            print(f"ecrit {args.overlay_out}")
            return 0
        styles, levels = compile_all(drawing, log=print if args.write else None)
        derived = render(styles, levels, drawing)
    except DrawingError as refusal:
        for error in refusal.errors:
            print(f"REFUS {error}", file=sys.stderr)
        return 1
    if args.write:
        write_replacing(args.out, derived)
        print(f"ecrit {args.out} ({len(derived)} octets, dessin {hashlib.sha256(drawing).hexdigest()})")
        return 0
    problems = check(derived, args.out, drawing)
    for problem in problems:
        print(f"ECART {problem}", file=sys.stderr)
    if not problems:
        print(f"conforme : {args.out} est la compilation du dessin {hashlib.sha256(drawing).hexdigest()}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

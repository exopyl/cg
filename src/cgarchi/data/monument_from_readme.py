"""Moteur de derivation d'un document "monument/v9" depuis le README d'un monument.

Le moteur porte les regles de lecture, l'ordre des cles du format et le rendu ; les
tables d'un monument (selecteurs, nom, chemin du README, blocs documentes) sont dans
son module, qui est aussi son point d'entree : temple_hephaistos_from_readme.py,
temple_ares_from_readme.py. Les README sont tenus hors du depot, dans les documents
de l'utilisateur ; les JSON produits sont commites a cote des scripts et embarques
dans cgarchi (embed_file.cmake).

Blocs : un monument documente une partie des blocs du format (BLOCKS de son module).
Un bloc documente porte toutes ses cles (SECTION_ORDER), hors OPTIONAL_KEYS ; un bloc
non documente est absent du document. Une valeur lue hors des blocs documentes, ou une
cle exigee non lue, est un refus.

Usage :
    python -I <monument>_from_readme.py --readme <README.md> --write [--out <json>]
    python -I <monument>_from_readme.py --readme <README.md> --check [--out <json>]

--write  derive le document et l'ecrit ; rien n'est ecrit si une valeur est refusee.
--check  derive le document en memoire et le compare au JSON existant, octet par
         octet, apres conversion des fins de ligne CRLF de ce fichier en LF : avec
         core.autocrlf, l'arbre de travail Windows porte des CRLF, le depot des LF.
         Compare aussi l'empreinte du README lu a celle que le JSON enregistre.

Sortie 0 si tout concorde ; 1 sinon, une ligne par ecart sur stderr, cle et ligne du
README nommees. Un statut du README place entre une valeur de prose et le statut retenu
pour elle est un refus : le statut doit etre celui de la valeur.

Regles d'extraction (plan_implementation_03_revision.md, sections 15.3, 18.5, 20.5, 21.6, 23.6, 24.23.7 et 26) :
  - une valeur par selecteur explicite ; chaque selecteur nomme sa cle JSON, son
    lieu dans le README (section, puis debut du libelle de ligne ou prose de la
    section), le motif de sa valeur, son unite, et le statut et les sources attendus ;
  - le motif doit avoir une et une seule occurrence dans son lieu ; sauf regle R2,
    il commence la cellule de valeur ;
  - statut et sources sont lus dans la premiere clause de la cellule de statut
    (avant le premier « ; »), sauf regles R1, R3 et R4, et compares a ceux du
    selecteur : le script verifie les statuts, il n'en attribue aucun ;
  - la valeur est recopiee en texte, virgule decimale changee en point, sans
    passer par un flottant, avec l'unite que le motif exige ("m", "in", "mm", "ft",
    "°" pour un angle, "ratio" pour un rapport (regle R12), ou "1" pour un compte) ;
    aucune conversion d'unite, aucun calcul. Seule la
    notation « ± » des coordonnees de la section 4.3 est developpee en deux
    valeurs. L'unite "ft" est reservee aux cles qui finissent par « Ft ».
Regles de lecture des chapitres 5 et 6 (registre MO3) :
  - R1, clause etiquetee : la cellule de statut se coupe en clauses a « ; » et a
    « . » suivi d'un mot a majuscule initiale, hors parentheses ; une clause
    « etiquettes : statut [ref] » porte le statut des grandeurs qu'elle nomme
    (etiquettes separees par « , » ou « et ») ; le selecteur nomme son etiquette,
    qui doit avoir une seule occurrence ;
  - R2, valeur reperee par son libelle : le motif contient le libelle et l'unite,
    et peut se trouver n'importe ou dans la cellule ; une seule occurrence ;
  - R3, tableau a lignes de statut : la valeur se lit a la ligne de valeur, dans
    la colonne designee par le nombre qui commence son en-tete ; son statut, a la
    meme colonne de la ligne de statut ;
  - R4, statut de prose entre parentheses simples, « (lu [S] ...) » ou
    « (**manquant**) », sur la ligne de puce qui porte la valeur ;
  - R5, suite a unite finale partagee : apres une amorce (« en plan : »), des
    termes « mot nombre [unite] » separes par « , » et clos par « ; » ; le nombre
    de termes et l'unite finale sont exiges, toute unite intermediaire est
    l'unite finale ; chaque terme garde son mot ;
  - R6, pieds et pouces (« 2 ft 8,55 in ») : une cle et une unite par groupe,
    chaque unite suivant directement son groupe ; pieds entiers ; aucune somme ;
  - R8, unite partagee par « et » (« 0,685 et 0,916 m ») : comme « → » et « × »,
    les groupes separes par « et » prennent l'unite qui suit le dernier ;
  - R9, unite collee au nombre (« 14° ») : l'unite « ° » suit directement l'unique
    groupe decimal du motif, sans blanc ;
  - R11, liste de sommets : dans la cellule de valeur, entre une amorce et une fin
    uniques, des sommets « u nombre h nombre » separes par « → », chacun lu en
    entier ; l'unite est donnee par la phrase « en <unite>. » qui precede
    immediatement l'amorce (« en pouces. ») ; nombre de sommets exige ; statut par
    l'etiquette R1 du selecteur. Les deux nombres de chaque sommet sont recopies
    dans deux listes, « u » et « h », ordre conserve ;
  - R12, rapport « 1 : nombre » (« 1 : 4,0846 ») : unite "ratio" ; le motif ecrit
    « 1 : » juste devant son unique groupe decimal ; la valeur recopiee est le
    nombre de droite, sans le « 1 : ».
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import os
import re
import sys
from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
from types import ModuleType

# Le script n'a pas de pyproject : ruff suppose une version cible que l'interpreteur lance a la
# main n'a aucune raison de respecter. Le controle reste, pour un refus explicite plutot qu'une
# TypeError sur `dict | list` plus bas.
if sys.version_info < (3, 10):  # noqa: UP036
    sys.exit(f"monument_from_readme.py demande Python 3.10 ou plus recent, "
             f"lance par {sys.version.split()[0]}")

SCHEMA = "monument/v9"

STATUSES = ("lu", "déduit", "mesuré", "convention", "manquant")
UNITS = ("m", "in", "mm", "ft", "1", "°", "ratio")
# Regle R12 : rapport « 1 : nombre », le nombre seul recopie.
RATIO = "ratio"
RATIO_LEAD = "1 : "
# Regle R9 : unites ecrites collees au nombre, sans blanc.
GLUED_UNITS = ("°",)
# Regle R11 : unite d'une liste de sommets, ecrite en toutes lettres dans la phrase « en <mot>. » qui precede l'amorce.
UNIT_WORDS = {"m": "mètres", "in": "pouces", "mm": "millimètres"}
FOOT = "ft"
FOOT_KEY_SUFFIX = "Ft"
# Typographie du README : signe moins et signe de multiplication, distincts de « - » et de « x ».
MINUS = "\u2212"
TIMES = "\u00d7"
NUM = r"(\d+(?:,\d+)?)"
# Regle R6 : pieds entiers >= 1 (« 0 ft » refuse, comme par le schema et le lecteur) ; ni chiffre ni virgule
# devant, pour que « 2,5 ft » soit refuse sans ancrage.
FEET_GROUP = r"([1-9]\d*)"
FEET = rf"(?<![\d,]){FEET_GROUP} ft"
STATUS_WORD_RE = re.compile(r"\b(" + "|".join(STATUSES) + r")\b")
STATUS_RE = re.compile(r"^\(?\**\s*(" + "|".join(STATUSES) + r")\b\**")
REF_RE = re.compile(r"\[([A-Z])\]")
SIGNED_RE = re.compile(rf"^([{MINUS}+±]?)(\d+(?:,\d+)?)$")
# R1 : « etiquettes : corps », etiquettes sans ponctuation de reference ni parenthese.
LABELLED_RE = re.compile(r"^([^:()\[\]`]+?)\s:\s(.*)$", re.S)
# R1 : fin de clause a « . » suivi d'un mot a majuscule initiale (« Flèche », pas « IX »).
SENTENCE_RE = re.compile(r"\.\s+(?=[A-ZÀ-Ý][a-zà-ÿ])")
# R4 : statut de prose entre parentheses simples, gras admis autour du mot.
PAREN_STATUS_RE = re.compile(r"\(\**(" + "|".join(STATUSES) + r")\b\**([^()]*)\)")
LEADING_NUM_RE = re.compile(r"^" + NUM + r"(?:\s|$)")


class Raw(str):
    """Nombre JSON ecrit tel quel : le texte lu dans le README, jamais un flottant."""


JsonValue = dict | list | str | int | Raw


@dataclass(frozen=True)
class RowSelector:
    """Une ligne de tableau : section, debut du libelle, motif d'une cellule de valeur."""

    keys: tuple[str, ...]      # une cle par groupe du motif (aucune pour une valeur manquante)
    section: str               # numero de section : « 2. », « 3.2 »
    label: str                 # debut de la premiere cellule
    pattern: str               # motif de la cellule de valeur, sans ancre
    unit: str                  # unite que le motif exige
    status: str
    sources: tuple[str, ...]
    cell: int = 1              # cellule de la valeur
    anchored: bool = True      # le motif commence la cellule ; sinon, regle R2
    status_label: str = ""     # regle R1 : etiquette de la clause de statut
    exact: bool = False        # libelle egal a la premiere cellule, et non son debut
    group_units: tuple[str, ...] = ()   # regle R6 : une unite par groupe ; vide : `unit` pour tous


@dataclass(frozen=True)
class SeriesSelector:
    """Regle R5 : suite « mot nombre [unite], ..., mot nombre unite ; » apres une amorce, dans une cellule."""

    key: str
    section: str
    label: str                 # debut de la premiere cellule
    lead: str                  # texte qui precede la suite, une seule occurrence dans la cellule
    words: tuple[str, ...]     # mots admis devant un nombre
    count: int                 # nombre de termes exige
    unit: str                  # unite finale exigee
    status: str
    sources: tuple[str, ...]
    cell: int = 1


@dataclass(frozen=True)
class PolygonSelector:
    """Regle R11 : liste de sommets « u nombre h nombre » separes par « → », entre une amorce et une fin, dans une
    cellule ; unite par la phrase « en <mot>. » qui precede l'amorce ; statut par etiquette R1."""

    key: str
    section: str
    label: str                 # debut de la premiere cellule
    lead: str                  # texte qui precede la liste, une seule occurrence dans la cellule
    end: str                   # texte qui suit la liste, une seule occurrence apres l'amorce
    count: int                 # nombre de sommets exige
    unit: str                  # unite exigee, dont le mot (UNIT_WORDS) precede l'amorce
    status: str
    sources: tuple[str, ...]
    status_label: str          # regle R1 : etiquette de la clause de statut
    cell: int = 1


@dataclass(frozen=True)
class ProseSelector:
    """Une valeur de prose : motif compte sur toute la section ; statut en italique qui la suit."""

    key: str
    section: str
    pattern: str
    unit: str
    status: str
    sources: tuple[str, ...]


@dataclass(frozen=True)
class BulletSelector:
    """Regle R4 : valeur d'une ligne de puce, statut entre parentheses simples sur la meme ligne."""

    key: str
    section: str
    pattern: str               # repere la valeur (groupe 1), ou le libelle d'une valeur manquante
    unit: str
    status: str
    sources: tuple[str, ...]


@dataclass(frozen=True)
class TableSelector:
    """Regle R3 : tableau a lignes de statut, colonne designee par le nombre de son en-tete."""

    key: str
    section: str
    header: str                # premiere cellule de la ligne d'en-tete
    column: str                # nombre qui commence l'en-tete de la colonne
    value_row: str             # premiere cellule de la ligne de valeur (l'en-tete lui-meme permis)
    status_row: str            # premiere cellule de la ligne de statut
    unit: str
    status: str
    sources: tuple[str, ...]


@dataclass(frozen=True)
class AxesSelector:
    """En-tete d'un tableau de coordonnees de la section 4.3 (« | y | ... | »)."""

    key: str
    section: str
    axis: str


@dataclass(frozen=True)
class ResultSelector:
    """Resultat imprime d'un controle de la section 12, derniere cellule de sa ligne, lu en entier par
    `pattern` (valeur au groupe 1) : « 13,718 ✓ », « 16,125 = [M] ✓ »."""

    key: str
    section: str
    label: str
    pattern: str = NUM + r" ✓"


@dataclass(frozen=True)
class ControlSelector:
    """Controle de la colonne, sans statut : un nombre repere par son libelle dans une cellule
    (ou dans la prose de la section si label est vide)."""

    key: str
    section: str
    label: str                 # debut de la premiere cellule ; vide : prose de la section
    cell: int
    pattern: str               # valeur au premier groupe ; une seule occurrence ; un groupe de plus n'est
                               # admis qu'apres un separateur d'unite partagee (« 0,685 et 0,916 m », R8)
    unit: str
    unit_in_label: bool = False   # l'unite termine le libelle (resultat imprime seul, « 19,884 ✓ »)


@dataclass(frozen=True)
class ProfileSelector:
    """Controle de la colonne : tableau de points (en-tete numerique, ligne de valeurs)."""

    key: str
    section: str
    header: str
    value_row: str
    unit: str


# Ordre des blocs et des cles du document, fixe ici (choix de format, D3-17).
SECTION_ORDER = {
    "krepis": ("stylobateWidth", "stylobateLength", "stepCount", "stylobateHeight", "secondStepHeight",
               "thirdStepHeight", "secondStepTread", "thirdStepTread", "cornerMargin", "euthynteriaHeight",
               "euthynteriaTread"),
    "peristyle": ("frontColumns", "flankColumns", "frontSpacing", "flankSpacing", "frontCornerSpacing",
                  "flankCornerSpacing"),
    "columns": ("lowerDiameter", "cornerLowerDiameter", "upperDiameter", "height", "fluteCount", "fluteDepth",
                "entasisSagitta", "entasisHeight", "abacusHeight", "abacusHalfWidth", "echinusHeight",
                "echinusJunctionDepth", "echinusJunctionRetreat", "echinusTopRetreat", "echinusLipRadius",
                "annuletCount", "annuletsHeight", "annuletsTopRetreat", "annuletsBottomRetreat",
                "hypotrachelionHeight", "grooveHeight", "grooveDepth"),
    "controls": ("frontAxesY", "flankAxesX", "frontSum", "flankSum"),
    "krepisControls": ("footprintWidth", "footprintLength"),
    "columnControls": ("shaftHeight", "footFluteChord", "fluteCentralRadius", "fluteLateralRadius", "topFluteDepth",
                       "cornerFluteChord", "cornerFluteDepth", "capitalHeight", "annuletsAndRelief", "upperRadius",
                       "echinusLipRetreat", "echinusProfile"),
    "entablature": ("architraveHeightFt", "architraveHeightIn", "taeniaHeight", "regulaHeight", "guttaeHeight",
                    "architraveFaceHeight", "friezeHeightFt", "friezeHeightIn", "triglyphCapitalHeight",
                    "corniceHeightFt", "corniceHeightIn", "mutulePlateHeight", "dripHeight", "coronaHeight",
                    "filletHeight", "crownHeight", "architraveTopProjection", "architraveFootProjection",
                    "taeniaProjection", "triglyphFace", "triglyphCapitalBandProjection",
                    "triglyphCapitalListelProjection", "triglyphCapitalListelHeight", "triglyphCapitalBandHeight",
                    "metopeFace", "mutulePlateProjection", "dripProjection", "coronaProjection", "filletProjection",
                    "crownProjection", "innerFace", "triglyphWidthFt", "triglyphWidthIn", "metopeWidthFt",
                    "metopeWidthIn", "triglyphDivisions", "channelDepth", "regulaProjection", "regulaGuttaCount",
                    "regulaGuttaDiameter", "regulaGuttaGap", "regulaGuttaTopProjection", "regulaGuttaFootProjection",
                    "mutuleWidthFt", "mutuleWidthIn", "mutuleDepth", "soffitSlope", "mutuleGuttaFrontDiameter",
                    "mutuleGuttaMiddleDiameter", "mutuleGuttaRearDiameter", "mutuleGuttaFrontGap", "mutuleGuttaRearGap",
                    "mutuleGuttaHeight", "mutuleGuttaEdgeAxis", "mutuleGuttaPerRow", "mutuleGuttaFootFlare"),
    "entablatureControls": ("total", "corniceSum", "triglyphDivisionSum", "frontMetope", "flankMetope",
                            "metopeCount", "frontTriglyphs", "flankTriglyphs", "channelBottom", "metopeRecess",
                            "cornerTriglyphOffset", "frontCornerDistance", "frontCornerMetope", "flankCornerMetope",
                            "intermediateShift", "frontMidBayInnerMetope", "frontMidBayOuterMetope",
                            "flankMidBayInnerMetope", "flankMidBayOuterMetope", "regulaGuttaMargin",
                            "regulaGuttaShortfall", "regulaGuttaCountCheck", "mutuleBack", "mutuleGuttaRows",
                            "mutuleGuttaPitchCount", "mutuleGuttaMiddleAxis", "mutuleGuttaRearAxis", "mutuleGuttaPitch",
                            "mutuleRearMargin", "mutuleHorizontalDepth", "mutuleInclinedBack", "soffitPivotZ",
                            "mutuleFrontTopZ", "soffitBackZ", "soffitRise", "geisonBackStep", "dripTopZ",
                            "soffitPivotProjection", "mutuleGuttaRadialFlare", "mutuleGuttaRadialFlareExact",
                            "mutuleGuttaFrontFootProjection"),
    "pediment": ("apexRiseFt", "apexRiseIn", "rakingRun", "rakingProjectionFt", "rakingProjectionIn", "tympanumFace",
                 "rakingHeights", "rakingSection", "sima"),
    "pedimentControls": ("apexRise", "rakingSlope", "eaveHalfSpan", "rakingHeight", "rakingHeightM",
                         "rakingVerticalHeight", "rakingVerticalHeightM", "rakingProjection", "tympanumFaceM",
                         "rakingListelProjection", "rakingCoronaProjection", "rakingBottomFilletProjection",
                         "rakingCoronaGap"),
}

# Cles qu'un bloc documente peut ne pas porter.
OPTIONAL_KEYS = frozenset({"controls.frontSum", "controls.flankSum"})

# Bloc de controles -> bloc de sa partie, sans lequel il est refuse.
CONTROLS_OF = {"controls": "peristyle", "krepisControls": "krepis", "columnControls": "columns",
               "entablatureControls": "entablature", "pedimentControls": "pediment"}
# Blocs de controles exiges par leur partie : les axes du peristyle.
REQUIRED_CONTROLS = frozenset({"controls"})


class DerivationError(Exception):
    """Ecarts d'une derivation refusee, une ligne chacun."""

    def __init__(self, errors: list[str]) -> None:
        super().__init__("\n".join(errors))
        self.errors = errors


@dataclass
class Line:
    number: int     # 1 pour la premiere ligne du README
    section: str    # numero de la section qui la contient : « 3.2 », « 12. »
    text: str


def split_lines(text: str) -> list[Line]:
    lines = []
    section = ""
    for number, line in enumerate(text.split("\n"), 1):
        if line.startswith("#"):
            words = line.lstrip("#").split()
            section = words[0] if words else ""
        lines.append(Line(number, section, line))
    return lines


def cells_of(line: str) -> list[str]:
    return [c.strip() for c in line.strip().strip("|").split("|")]


def at_lines(hits: list[Line]) -> str:
    """« , README l. 77, 78 » pour les lignes trouvees ; vide s'il n'y en a aucune."""
    return f", README l. {', '.join(str(ln.number) for ln in hits)}" if hits else ""


def readme_place(section: str, number: int) -> str:
    return f"§{section.rstrip('.')}, l. {number}"


def decimal_text(text: str) -> Raw:
    return Raw(text.replace(",", "."))


# Groupes captures des motifs : un nombre decimal, un entier, ou des pieds entiers >= 1.
GROUP_RE = re.compile("|".join(map(re.escape, (NUM, r"(\d+)", FEET_GROUP))))
# Separateurs entre deux groupes qui partagent l'unite du dernier : intervalle (« 5,334 → 6,25 in »),
# dimensions (« 13,719 × 31,769 m ») et regle R8, enumeration (« 0,685 et 0,916 m »).
SHARED_UNIT_SEPARATORS = (" → ", f" {TIMES} ", " et ")


def unit_follows_groups(unit: str, pattern: str) -> bool:
    """Vrai si l'unite suit directement le dernier groupe capture du motif, et si chaque groupe
    anterieur est suivi d'un separateur d'unite partagee, lui-meme suivi directement du groupe
    suivant. Une unite partagee ne lie que des nombres decimaux ({NUM}), jamais un compte.
    « X in = Y m » capture X en pouces, Y en metres : l'unite d'un groupe est celle qui le suit,
    pas une unite ecrite ailleurs. Regle R9 : une unite collee (« 14° ») suit l'unique groupe, un
    nombre decimal, sans blanc."""
    groups = list(GROUP_RE.finditer(pattern))
    if not groups:
        return False
    if unit in GLUED_UNITS:
        return len(groups) == 1 and groups[0].group(0) == NUM and pattern.startswith(unit, groups[0].end())
    if unit == RATIO:
        start = groups[0].start()
        return (len(groups) == 1 and groups[0].group(0) == NUM and start >= len(RATIO_LEAD)
                and pattern[start - len(RATIO_LEAD):start] == RATIO_LEAD)
    for g, following in zip(groups, groups[1:]):
        if g.group(0) != NUM or following.group(0) != NUM:
            return False
        if not any(pattern.startswith(sep, g.end()) and following.start() == g.end() + len(sep)
                   for sep in SHARED_UNIT_SEPARATORS):
            return False
    return re.match(rf" {re.escape(unit)}(?![A-Za-z])", pattern[groups[-1].end():]) is not None


def unit_follows_each_group(units: tuple[str, ...], pattern: str) -> bool:
    """Regle R6 : vrai si le motif a autant de groupes que d'unites, et si chaque groupe est suivi
    directement de sa propre unite. « 2 ft 8,55 in » : « ft » suit les pieds, « in » les pouces."""
    groups = list(GROUP_RE.finditer(pattern))
    if len(groups) != len(units):
        return False
    return all(re.match(rf" {re.escape(u)}(?![A-Za-z])", pattern[g.end():]) is not None
               for g, u in zip(groups, units, strict=True))


def key_units(sel: RowSelector) -> tuple[str, ...]:
    """Unite de chaque cle d'un selecteur de ligne, dans l'ordre des cles."""
    return sel.group_units or (sel.unit,) * len(sel.keys)


def check_status(clause: str, status: str, sources: tuple[str, ...], references: dict[str, str]) -> str:
    """Motif de refus, ou "" si la clause porte le statut et les sources attendus."""
    m = STATUS_RE.match(clause.strip())
    if not m:
        return f"statut attendu « {status} », aucun statut en tete de la clause « {clause.strip()} »"
    if m.group(1) != status:
        return f"statut attendu « {status} », lu « {m.group(1)} »"
    found = tuple(sorted(set(REF_RE.findall(clause))))
    unknown = [r for r in found if r not in references]
    if unknown:
        return f"source inconnue de la section 1 : {unknown}"
    if found != tuple(sorted(sources)):
        return f"sources attendues {sorted(sources)}, lues {list(found)}"
    return ""


def top_level_clauses(cell: str) -> list[str]:
    """Regle R1 : clauses coupees a « ; » et a « . » suivi d'un mot a majuscule, hors parentheses."""
    clauses: list[str] = []
    depth = 0
    start = 0
    for i, c in enumerate(cell):
        if c == "(":
            depth += 1
        elif c == ")":
            depth = max(0, depth - 1)
        elif depth == 0 and c == ";":
            clauses.append(cell[start:i])
            start = i + 1
        elif depth == 0 and c == "." and SENTENCE_RE.match(cell, i):
            clauses.append(cell[start:i])
            start = i + 1
    clauses.append(cell[start:])
    return [c.strip() for c in clauses if c.strip()]


def labelled_status(cell: str, label: str) -> tuple[str | None, str]:
    """Regle R1 : corps de l'unique clause etiquetee qui nomme `label`, ou (None, motif)."""
    bodies = []
    for clause in top_level_clauses(cell):
        m = LABELLED_RE.match(clause)
        if not m:
            continue
        labels = [w.strip().lower() for w in re.split(r",|\set\s", m.group(1)) if w.strip()]
        if label.lower() in labels:
            bodies.append(m.group(2))
    if len(bodies) != 1:
        return None, f"etiquette « {label} » trouvee {len(bodies)} fois dans « {cell} », une attendue"
    return bodies[0], ""


def read_references(lines: list[Line]) -> dict[str, str]:
    """Table des references de la section 1 : identifiant -> cellule « Document », recopiee."""
    refs: dict[str, str] = {}
    for line in lines:
        if line.section != "1." or not line.text.startswith("|"):
            continue
        cells = cells_of(line.text)
        m = re.fullmatch(r"\*\*\[([A-Z])\]\*\*", cells[0])
        if m and len(cells) > 1:
            refs[m.group(1)] = cells[1]
    return refs


def value_object(value: Raw | None, unit: str, status: str, sources: tuple[str, ...], place: str) -> dict:
    obj: dict = {}
    if value is not None:
        obj["value"] = value
    obj["unit"] = unit
    obj["status"] = status
    obj["sources"] = sorted(sources)
    obj["readme"] = place
    return obj


def table_rows(lines: list[Line], section: str, label: str, exact: bool) -> list[Line]:
    return [ln for ln in lines
            if ln.section == section and ln.text.startswith("|")
            and (cells_of(ln.text)[0] == label if exact else cells_of(ln.text)[0].startswith(label))]


def check_selectors(tables: ModuleType) -> list[str]:
    """Coherence de la table des selecteurs : unite de la liste fermee ; dans un motif, ecrite
    directement apres le groupe capture ; dans un tableau, dans le libelle de la ligne de valeur
    (« Retrait (in) ») ; pour un resultat imprime seul, a la fin du libelle de sa ligne."""
    errors = []
    for sel in (*tables.ROWS, *tables.SERIES, *tables.POLYGONS, *tables.PROSE, *tables.BULLETS, *tables.TABLES,
                *tables.CONTROLS, *tables.PROFILES):
        name = getattr(sel, "key", None) or ", ".join(sel.keys)
        keys = sel.keys if isinstance(sel, RowSelector) else (sel.key,)
        units = key_units(sel) if isinstance(sel, RowSelector) else (sel.unit,)
        outside = [u for u in (sel.unit, *units) if u not in UNITS]
        if outside:
            errors.append(f"{name} : unite « {outside[0]} » hors de {list(UNITS)}")
            continue
        if len(units) != len(keys):
            errors.append(f"{name} : {len(units)} unite(s) pour {len(keys)} cle(s)")
            continue
        misplaced = [k for k, u in zip(keys, units, strict=True) if (u == FOOT) != k.endswith(FOOT_KEY_SUFFIX)]
        if misplaced:
            errors.append(f"{name} : unite « {FOOT} » reservee aux cles « …{FOOT_KEY_SUFFIX} », "
                          f"en defaut sur {misplaced}")
            continue
        if isinstance(sel, SeriesSelector):
            if sel.count < 1 or not sel.words or sel.unit == "1":
                errors.append(f"{name} : suite sans terme, sans mot ou sans dimension")
            continue
        if isinstance(sel, PolygonSelector):
            if sel.count < 3 or sel.unit not in UNIT_WORDS or not sel.status_label:
                errors.append(f"{name} : liste de moins de 3 sommets, d'unite sans mot (R11) ou sans etiquette de statut")
            continue
        if sel.unit == "1" or getattr(sel, "status", "") == "manquant":
            continue
        if isinstance(sel, RowSelector) and sel.group_units:
            if sel.group_units[-1] != sel.unit or not unit_follows_each_group(sel.group_units, sel.pattern):
                errors.append(f"{name} : unites {list(sel.group_units)} ne suivent pas chacune son groupe")
            continue
        if isinstance(sel, (TableSelector, ProfileSelector)):
            ok = f"({sel.unit})" in sel.value_row
        elif getattr(sel, "unit_in_label", False):
            ok = sel.label.endswith(f" {sel.unit}")
        else:
            ok = unit_follows_groups(sel.unit, sel.pattern)
        if not ok:
            errors.append(f"{name} : unite « {sel.unit} » ne suit pas la valeur capturee")
    return errors


def apply_rows(tables: ModuleType, lines: list[Line],
                refs: dict[str, str], values: dict, errors: list[str]) -> None:
    for sel in tables.ROWS:
        hits = table_rows(lines, sel.section, sel.label, sel.exact)
        name = ", ".join(sel.keys)
        if len(hits) != 1:
            errors.append(f"{name} : {len(hits)} ligne(s) « {sel.label} » en section {sel.section}{at_lines(hits)}, "
                          "une attendue")
            continue
        ln = hits[0]
        cells = cells_of(ln.text)
        where = f"{name}, README l. {ln.number}"
        if len(cells) < 3 or sel.cell >= len(cells) - 1:
            errors.append(f"{where} : {len(cells)} cellule(s), cellule de valeur {sel.cell} et cellule de statut "
                          "attendues")
            continue
        matches = list(re.finditer(sel.pattern, cells[sel.cell]))
        if len(matches) != 1:
            errors.append(f"{where} : motif trouve {len(matches)} fois dans « {cells[sel.cell]} », une attendue")
            continue
        if sel.anchored and matches[0].start() != 0:
            errors.append(f"{where} : la valeur ne commence pas la cellule « {cells[sel.cell]} »")
            continue
        if sel.status_label:
            clause, why = labelled_status(cells[-1], sel.status_label)
            if clause is None:
                errors.append(f"{where} : {why}")
                continue
        else:
            clause = cells[-1].split(";")[0]
        why = check_status(clause, sel.status, sel.sources, refs)
        if why:
            errors.append(f"{where} : {why}")
            continue
        place = readme_place(sel.section, ln.number)
        if sel.status == "manquant":
            values[sel.keys[0]] = value_object(None, sel.unit, sel.status, sel.sources, place)
            continue
        for key, unit, text in zip(sel.keys, key_units(sel), matches[0].groups(), strict=True):
            values[key] = value_object(decimal_text(text), unit, sel.status, sel.sources, place)


def series_terms(body: str, sel: SeriesSelector) -> tuple[list[tuple[str, str, str | None]], str]:
    """Regle R5 : termes (mot, nombre, unite ou None) de la suite `body`, ou ([], motif de refus).
    Separateur « , » suivi d'un blanc : la virgule decimale n'en est pas suivie."""
    words = "|".join(re.escape(w) for w in sorted(sel.words, key=len, reverse=True))
    term_re = re.compile(rf"({words}) {NUM}(?: ([A-Za-z]+))?")
    terms = []
    for text in re.split(r",\s+", body.strip()):
        m = term_re.fullmatch(text.strip())
        if not m:
            return [], f"terme illisible « {text.strip()} », attendu « mot nombre [unite] », mots {list(sel.words)}"
        terms.append((m.group(1), m.group(2), m.group(3)))
    if len(terms) != sel.count:
        return [], f"{len(terms)} termes, {sel.count} attendus"
    final = terms[-1][2]
    if final is None:
        return [], f"unite finale absente apres « {terms[-1][0]} {terms[-1][1]} », « {sel.unit} » attendue"
    if final != sel.unit:
        return [], f"unite finale « {final} », « {sel.unit} » attendue"
    for i, (word, number, unit) in enumerate(terms[:-1], 1):
        if unit is not None and unit != final:
            return [], f"unite intermediaire « {unit} » au terme {i} (« {word} {number} »), unite finale « {final} »"
    return terms, ""


def apply_series(tables: ModuleType, lines: list[Line],
                refs: dict[str, str], values: dict, errors: list[str]) -> None:
    for sel in tables.SERIES:
        hits = table_rows(lines, sel.section, sel.label, False)
        if len(hits) != 1:
            errors.append(f"{sel.key} : {len(hits)} ligne(s) « {sel.label} » en section {sel.section}"
                          f"{at_lines(hits)}, une attendue")
            continue
        ln = hits[0]
        cells = cells_of(ln.text)
        where = f"{sel.key}, README l. {ln.number}"
        if len(cells) < 3 or sel.cell >= len(cells) - 1:
            errors.append(f"{where} : {len(cells)} cellule(s), cellule de valeur {sel.cell} et cellule de statut "
                          "attendues")
            continue
        cell = cells[sel.cell]
        if cell.count(sel.lead) != 1:
            errors.append(f"{where} : amorce « {sel.lead} » trouvee {cell.count(sel.lead)} fois, une attendue")
            continue
        rest = cell[cell.index(sel.lead) + len(sel.lead):]
        if ";" not in rest:
            errors.append(f"{where} : suite non close par « ; »")
            continue
        terms, why = series_terms(rest[:rest.index(";")], sel)
        if why:
            errors.append(f"{where} : {why}")
            continue
        why = check_status(cells[-1].split(";")[0], sel.status, sel.sources, refs)
        if why:
            errors.append(f"{where} : {why}")
            continue
        values[sel.key] = {"values": [decimal_text(number) for _, number, _ in terms],
                           "kinds": [word for word, _, _ in terms],
                           "unit": sel.unit, "status": sel.status, "sources": sorted(sel.sources),
                           "readme": readme_place(sel.section, ln.number)}


VERTEX_RE = re.compile(rf"u {NUM} h {NUM}")


def apply_polygons(tables: ModuleType, lines: list[Line],
                refs: dict[str, str], values: dict, errors: list[str]) -> None:
    """Regle R11 : sommets « u nombre h nombre » separes par « → », entre l'amorce et la fin du selecteur."""
    for sel in tables.POLYGONS:
        hits = table_rows(lines, sel.section, sel.label, False)
        if len(hits) != 1:
            errors.append(f"{sel.key} : {len(hits)} ligne(s) « {sel.label} » en section {sel.section}"
                          f"{at_lines(hits)}, une attendue")
            continue
        ln = hits[0]
        cells = cells_of(ln.text)
        where = f"{sel.key}, README l. {ln.number}"
        if len(cells) < 3 or sel.cell >= len(cells) - 1:
            errors.append(f"{where} : {len(cells)} cellule(s), cellule de valeur {sel.cell} et cellule de statut "
                          "attendues")
            continue
        cell = cells[sel.cell]
        if cell.count(sel.lead) != 1:
            errors.append(f"{where} : amorce « {sel.lead} » trouvee {cell.count(sel.lead)} fois, une attendue")
            continue
        start = cell.index(sel.lead)
        rest = cell[start + len(sel.lead):]
        if rest.count(sel.end) != 1:
            errors.append(f"{where} : fin « {sel.end} » trouvee {rest.count(sel.end)} fois apres l'amorce, une attendue")
            continue
        unit_phrase = f"en {UNIT_WORDS[sel.unit]}."
        if not cell[:start].rstrip().endswith(unit_phrase):
            errors.append(f"{where} : unite « {unit_phrase} » absente juste avant l'amorce")
            continue
        vertices = []
        bad = ""
        for text in rest[:rest.index(sel.end)].split(" → "):
            m = VERTEX_RE.fullmatch(text.strip())
            if not m:
                bad = f"sommet illisible « {text.strip()} », attendu « u nombre h nombre »"
                break
            vertices.append((decimal_text(m.group(1)), decimal_text(m.group(2))))
        if bad:
            errors.append(f"{where} : {bad}")
            continue
        if len(vertices) != sel.count:
            errors.append(f"{where} : {len(vertices)} sommets, {sel.count} attendus")
            continue
        clause, why = labelled_status(cells[-1], sel.status_label)
        if clause is None:
            errors.append(f"{where} : {why}")
            continue
        why = check_status(clause, sel.status, sel.sources, refs)
        if why:
            errors.append(f"{where} : {why}")
            continue
        values[sel.key] = {"u": [u for u, _ in vertices], "h": [h for _, h in vertices],
                           "unit": sel.unit, "status": sel.status, "sources": sorted(sel.sources),
                           "readme": readme_place(sel.section, ln.number)}


def apply_prose(tables: ModuleType, lines: list[Line],
                refs: dict[str, str], values: dict, errors: list[str]) -> None:
    for sel in tables.PROSE:
        hits = [(ln, m) for ln in lines if ln.section == sel.section and not ln.text.startswith("#")
                for m in re.finditer(sel.pattern, ln.text)]
        if len(hits) != 1:
            errors.append(f"{sel.key} : motif trouve {len(hits)} fois en section {sel.section}"
                          f"{at_lines([ln for ln, _ in hits])}, une attendue")
            continue
        ln, m = hits[0]
        where = f"{sel.key}, README l. {ln.number}"
        after = ln.text[m.end():]
        st = re.search(r"\(\*(" + "|".join(STATUSES) + r")\*\s*((?:\[[A-Z]\]\s*)*)\)", after)
        if not st:
            errors.append(f"{where} : aucun statut en italique apres la valeur")
            continue
        between = STATUS_WORD_RE.findall(after[:st.start()])
        if between:
            errors.append(f"{where} : statut « {between[0]} » entre la valeur et le statut retenu "
                          f"« {st.group(1)} »")
            continue
        why = check_status(st.group(1) + " " + st.group(2), sel.status, sel.sources, refs)
        if why:
            errors.append(f"{where} : {why}")
            continue
        values[sel.key] = value_object(decimal_text(m.group(1)), sel.unit, sel.status, sel.sources,
                                       readme_place(sel.section, ln.number))


def apply_bullets(tables: ModuleType, lines: list[Line],
                refs: dict[str, str], values: dict, errors: list[str]) -> None:
    for sel in tables.BULLETS:
        hits = [(ln, m) for ln in lines if ln.section == sel.section and not ln.text.startswith("#")
                for m in re.finditer(sel.pattern, ln.text)]
        if len(hits) != 1:
            errors.append(f"{sel.key} : motif trouve {len(hits)} fois en section {sel.section}"
                          f"{at_lines([ln for ln, _ in hits])}, une attendue")
            continue
        ln, m = hits[0]
        where = f"{sel.key}, README l. {ln.number}"
        if not ln.text.startswith("- "):
            errors.append(f"{where} : la valeur n'est pas sur une ligne de puce")
            continue
        after = ln.text[m.end():]
        st = PAREN_STATUS_RE.search(after)
        if not st:
            errors.append(f"{where} : aucun statut entre parentheses sur la ligne de puce")
            continue
        between = STATUS_WORD_RE.findall(after[:st.start()])
        if between:
            errors.append(f"{where} : statut « {between[0]} » entre la valeur et le statut retenu "
                          f"« {st.group(1)} »")
            continue
        why = check_status(st.group(1) + " " + st.group(2), sel.status, sel.sources, refs)
        if why:
            errors.append(f"{where} : {why}")
            continue
        value = None if sel.status == "manquant" else decimal_text(m.group(1))
        values[sel.key] = value_object(value, sel.unit, sel.status, sel.sources,
                                       readme_place(sel.section, ln.number))


def table_column(header: list[str], column: str) -> list[int]:
    """Indices des cellules d'en-tete (hors libelle) dont le nombre initial est `column`."""
    return [j for j, cell in enumerate(header) if j > 0 and (m := LEADING_NUM_RE.match(cell)) and m.group(1) == column]


def apply_tables(tables: ModuleType, lines: list[Line],
                refs: dict[str, str], values: dict, errors: list[str]) -> None:
    for sel in tables.TABLES:
        rows = {}
        bad = False
        for role, label in (("header", sel.header), ("value", sel.value_row), ("status", sel.status_row)):
            hits = table_rows(lines, sel.section, label, True)
            if len(hits) != 1:
                errors.append(f"{sel.key} : {len(hits)} ligne(s) « {label} » en section {sel.section}"
                              f"{at_lines(hits)}, une attendue")
                bad = True
                break
            rows[role] = hits[0]
        if bad:
            continue
        header = cells_of(rows["header"].text)
        cols = table_column(header, sel.column)
        where = f"{sel.key}, README l. {rows['value'].number}"
        if len(cols) != 1:
            errors.append(f"{where} : colonne « {sel.column} » trouvee {len(cols)} fois, une attendue")
            continue
        j = cols[0]
        value_cells = cells_of(rows["value"].text)
        status_cells = cells_of(rows["status"].text)
        if len(value_cells) != len(header) or len(status_cells) != len(header):
            errors.append(f"{where} : lignes de {len(value_cells)} et {len(status_cells)} cellules, "
                          f"{len(header)} attendues")
            continue
        m = LEADING_NUM_RE.match(value_cells[j])
        if not m:
            errors.append(f"{where} : valeur illisible « {value_cells[j]} »")
            continue
        why = check_status(status_cells[j], sel.status, sel.sources, refs)
        if why:
            errors.append(f"{sel.key}, README l. {rows['status'].number} : {why}")
            continue
        values[sel.key] = value_object(decimal_text(m.group(1)), sel.unit, sel.status, sel.sources,
                                       readme_place(sel.section, rows["value"].number))


def apply_axes(tables: ModuleType, lines: list[Line],
                values: dict, errors: list[str]) -> None:
    for sel in tables.AXES:
        hits = [ln for ln in lines if ln.section == sel.section and ln.text.startswith("|")
                and cells_of(ln.text)[0] == sel.axis]
        if len(hits) != 1:
            errors.append(f"{sel.key} : {len(hits)} ligne(s) « | {sel.axis} | » en section {sel.section}"
                          f"{at_lines(hits)}, une attendue")
            continue
        ln = hits[0]
        coords: list[Decimal] = []
        bad = False
        for cell in cells_of(ln.text)[1:]:
            m = SIGNED_RE.match(cell)
            if not m:
                errors.append(f"{sel.key}, README l. {ln.number} : coordonnee illisible « {cell} »")
                bad = True
                continue
            magnitude = Decimal(m.group(2).replace(",", "."))
            if m.group(1) == "±":
                coords.extend((-magnitude, magnitude))
            else:
                coords.append(-magnitude if m.group(1) == MINUS else magnitude)
        if bad:
            continue
        coords.sort()
        if any(a == b for a, b in itertools.pairwise(coords)):
            errors.append(f"{sel.key}, README l. {ln.number} : coordonnee repetee")
            continue
        values[sel.key] = {"values": [Raw(str(c)) for c in coords],
                           "readme": readme_place(sel.section, ln.number)}


def apply_results(tables: ModuleType, lines: list[Line],
                values: dict, errors: list[str]) -> None:
    for sel in tables.RESULTS:
        hits = table_rows(lines, sel.section, sel.label, False)
        if len(hits) != 1:
            errors.append(f"{sel.key} : {len(hits)} ligne(s) « {sel.label} » en section {sel.section}"
                          f"{at_lines(hits)}, une attendue")
            continue
        ln = hits[0]
        cells = cells_of(ln.text)
        m = re.fullmatch(sel.pattern, cells[-1])
        if not m:
            errors.append(f"{sel.key}, README l. {ln.number} : resultat illisible « {cells[-1]} »")
            continue
        values[sel.key] = {"value": decimal_text(m.group(1)), "readme": readme_place(sel.section, ln.number)}


def apply_controls(tables: ModuleType, lines: list[Line],
                values: dict, errors: list[str]) -> None:
    for sel in tables.CONTROLS:
        if sel.label:
            hits = table_rows(lines, sel.section, sel.label, False)
            if len(hits) != 1:
                errors.append(f"{sel.key} : {len(hits)} ligne(s) « {sel.label} » en section {sel.section}"
                              f"{at_lines(hits)}, une attendue")
                continue
            cells = cells_of(hits[0].text)
            if not -len(cells) <= sel.cell < len(cells):
                errors.append(f"{sel.key}, README l. {hits[0].number} : {len(cells)} cellule(s), "
                              f"cellule {sel.cell} attendue")
                continue
            found = [(hits[0], m) for m in re.finditer(sel.pattern, cells[sel.cell])]
        else:
            found = [(ln, m) for ln in lines if ln.section == sel.section and not ln.text.startswith("#")
                     for m in re.finditer(sel.pattern, ln.text)]
        if len(found) != 1:
            errors.append(f"{sel.key} : motif trouve {len(found)} fois en section {sel.section}"
                          f"{at_lines([ln for ln, _ in found])}, une attendue")
            continue
        ln, m = found[0]
        values[sel.key] = {"value": decimal_text(m.group(1)), "unit": sel.unit,
                           "readme": readme_place(sel.section, ln.number)}


def apply_profiles(tables: ModuleType, lines: list[Line],
                values: dict, errors: list[str]) -> None:
    for sel in tables.PROFILES:
        rows = []
        for label in (sel.header, sel.value_row):
            hits = table_rows(lines, sel.section, label, True)
            if len(hits) != 1:
                errors.append(f"{sel.key} : {len(hits)} ligne(s) « {label} » en section {sel.section}"
                              f"{at_lines(hits)}, une attendue")
                break
            rows.append(hits[0])
        if len(rows) != 2:
            continue
        header, retreats = (cells_of(ln.text)[1:] for ln in rows)
        depth_m = [LEADING_NUM_RE.match(c) for c in header]
        retreat_m = [LEADING_NUM_RE.match(c) for c in retreats]
        if len(header) != len(retreats) or not all(depth_m) or not all(retreat_m):
            errors.append(f"{sel.key}, README l. {rows[0].number} : tableau de points illisible")
            continue
        values[sel.key] = {"depths": [decimal_text(m.group(1)) for m in depth_m],
                           "retreats": [decimal_text(m.group(1)) for m in retreat_m],
                           "unit": sel.unit, "readme": readme_place(sel.section, rows[0].number)}


def cited_sources(values: dict) -> list[str]:
    return sorted({s for obj in values.values() for s in obj.get("sources", [])})


def derive(readme: bytes, tables: ModuleType) -> dict:
    """Document derive du README par les tables `tables` ; leve DerivationError avec chaque ecart."""
    errors = check_selectors(tables)
    if errors:
        raise DerivationError(errors)
    try:
        text = readme.decode("utf-8")
    except UnicodeDecodeError as e:
        raise DerivationError([f"README : UTF-8 illisible ({e})"]) from e
    lines = split_lines(text)
    refs = read_references(lines)
    if not refs:
        errors.append("section 1 : aucune reference « **[X]** »")
    values: dict = {}
    apply_rows(tables, lines, refs, values, errors)
    apply_series(tables, lines, refs, values, errors)
    apply_polygons(tables, lines, refs, values, errors)
    apply_prose(tables, lines, refs, values, errors)
    apply_bullets(tables, lines, refs, values, errors)
    apply_tables(tables, lines, refs, values, errors)
    apply_axes(tables, lines, values, errors)
    apply_results(tables, lines, values, errors)
    apply_controls(tables, lines, values, errors)
    apply_profiles(tables, lines, values, errors)
    errors.extend(block_errors(tables.BLOCKS, values))
    if errors:
        raise DerivationError(errors)

    doc: dict = {
        "schema": SCHEMA,
        "monument": tables.MONUMENT,
        "source": {
            "document": tables.DOCUMENT,
            "sha256": hashlib.sha256(readme).hexdigest(),
            "bytes": len(readme),
            "derivedBy": tables.DERIVED_BY,
        },
        "sources": {ref: refs[ref] for ref in cited_sources(values)},
    }
    for section, keys in SECTION_ORDER.items():
        if section in tables.BLOCKS:
            doc[section] = {key: values[f"{section}.{key}"] for key in keys if f"{section}.{key}" in values}
    return doc


def block_errors(blocks: tuple[str, ...], values: dict) -> list[str]:
    """Refus de completude des blocs : bloc inconnu du format, valeur hors des blocs documentes, cle exigee
    d'un bloc documente non lue."""
    errors = [f"bloc « {b} » inconnu du format {SCHEMA}" for b in blocks if b not in SECTION_ORDER]
    errors += [f"bloc « {c} » documente sans le bloc « {part} »" for c, part in CONTROLS_OF.items()
               if c in blocks and part not in blocks]
    errors += [f"bloc « {part} » documente sans le bloc « {c} »" for c, part in CONTROLS_OF.items()
               if c in REQUIRED_CONTROLS and part in blocks and c not in blocks]
    errors += [f"{key} : lue hors des blocs documentes {list(blocks)}" for key in values
               if key.split(".")[0] not in blocks]
    errors += [f"{section}.{key} : cle exigee du bloc documente « {section} », non lue"
               for section in blocks if section in SECTION_ORDER for key in SECTION_ORDER[section]
               if f"{section}.{key}" not in values and f"{section}.{key}" not in OPTIONAL_KEYS]
    return errors


def dump(value: JsonValue, indent: int = 0) -> str:
    """JSON deterministe : objets developpes (indentation 2), tableaux de scalaires sur une ligne."""
    pad = "  " * indent
    if isinstance(value, dict):
        if not value:
            return "{}"
        items = [f'{pad}  {json.dumps(k, ensure_ascii=False)}: {dump(v, indent + 1)}' for k, v in value.items()]
        return "{\n" + ",\n".join(items) + "\n" + pad + "}"
    if isinstance(value, list):
        return "[" + ", ".join(dump(v, indent + 1) for v in value) + "]"
    if isinstance(value, Raw):
        if not re.fullmatch(r"-?\d+(\.\d+)?", value):
            raise ValueError(f"nombre JSON invalide : {value!r}")
        return str(value)
    if isinstance(value, bool):
        raise TypeError(f"aucun booleen dans {SCHEMA}")
    if isinstance(value, (str, int)):
        return json.dumps(value, ensure_ascii=False)
    raise TypeError(f"type non serialisable : {type(value).__name__}")


def render(doc: dict) -> bytes:
    return (dump(doc) + "\n").encode("utf-8")


def differences(old: object, new: object, path: str = "") -> list[str]:
    """Chemins dont la valeur differe entre deux documents relus."""
    if isinstance(old, dict) and isinstance(new, dict):
        out = []
        for key in list(old) + [k for k in new if k not in old]:
            sub = f"{path}.{key}" if path else key
            if key not in old:
                out.append(f"{sub} : absent du JSON existant")
            elif key not in new:
                out.append(f"{sub} : absent de la derivation")
            else:
                out.extend(differences(old[key], new[key], sub))
        return out
    if old != new:
        return [(f"{path} : JSON existant {json.dumps(old, ensure_ascii=False)}, "
                 f"derivation {json.dumps(new, ensure_ascii=False)}")]
    return []


def check(derived: bytes, out: Path, readme: bytes) -> list[str]:
    try:
        existing = out.read_bytes()
    except OSError as e:
        return [f"{out} : illisible ({e})"]
    if existing.replace(b"\r\n", b"\n") == derived:
        return []
    problems: list[str] = []
    try:
        old = json.loads(existing.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        return [f"{out} : JSON illisible ({e})", "le JSON existant n'est pas la sortie du script"]
    source = old.get("source", {}) if isinstance(old, dict) else {}
    sha = hashlib.sha256(readme).hexdigest()
    if source.get("sha256") != sha or source.get("bytes") != len(readme):
        problems.append(f"empreinte du README : JSON {source.get('sha256')} ({source.get('bytes')} octets), "
                        f"README lu {sha} ({len(readme)} octets)")
    new = json.loads(derived.decode("utf-8"))
    problems.extend(d for d in differences(old, new) if not d.startswith("source."))
    if not problems:
        problems.append("meme contenu, texte different : le JSON existant n'est pas la sortie du script")
    return problems


def write_replacing(out: Path, data: bytes) -> None:
    """Ecrit `data` dans un fichier temporaire du repertoire de `out`, puis le substitue a `out` :
    une ecriture interrompue laisse l'ancien JSON intact."""
    tmp = out.with_name(out.name + ".tmp")
    try:
        tmp.write_bytes(data)
        os.replace(tmp, out)
    finally:
        tmp.unlink(missing_ok=True)


def main(tables: ModuleType, argv: list[str] | None = None) -> int:
    """Point d'entree du module de tables `tables` : derive, ecrit ou compare son document."""
    parser = argparse.ArgumentParser(
        description=f"Derive {tables.DEFAULT_OUT.name} ({SCHEMA}) du README de {tables.MONUMENT}.")
    parser.add_argument("--readme", required=True, type=Path, help=f"chemin du README ({tables.DOCUMENT})")
    parser.add_argument("--out", type=Path, default=tables.DEFAULT_OUT, help="JSON a ecrire ou a comparer")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true", help="ecrire le document derive")
    mode.add_argument("--check", action="store_true", help="comparer le document derive au JSON existant")
    args = parser.parse_args(argv)

    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    try:
        readme = args.readme.read_bytes()
    except OSError as e:
        print(f"README illisible : {e}", file=sys.stderr)
        return 1
    try:
        derived = render(derive(readme, tables))
    except DerivationError as refusal:
        for error in refusal.errors:
            print(f"REFUS {error}", file=sys.stderr)
        return 1

    if args.write:
        write_replacing(args.out, derived)
        print(f"ecrit {args.out} ({len(derived)} octets, README {hashlib.sha256(readme).hexdigest()})")
        return 0
    problems = check(derived, args.out, readme)
    for problem in problems:
        print(f"ECART {problem}", file=sys.stderr)
    if not problems:
        print(f"conforme : {args.out} est la derivation du README {hashlib.sha256(readme).hexdigest()}")
    return 1 if problems else 0


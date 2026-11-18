#!/usr/bin/env python3
"""Derivation de temple_parthenon.json depuis le README du Parthenon.

Source : README du Parthenon, tenu hors du depot dans les documents de l'utilisateur
(Architecture/Antique/Parthenon/README.md). Ce module porte les tables du monument
(selecteurs, blocs documentes) ; les regles de lecture, le rendu et les modes
--write et --check sont ceux du moteur, monument_from_readme.py, dont la
documentation fait foi. Blocs documentes : krepis et peristyle, avec leurs
controles ; la colonne, l'entablement et le fronton sont absents (mode partiel,
registre MO-P2).

Les cotes de Penrose ([P]) sont lues en pieds anglais decimaux, entre parentheses
apres le metre de conversion (regle R2) ; le debord du stylobate, en metres, est
deduit de la pl. IV de [P], et les entraxes d'angle en sont deduits (decisions D1 et
D4 du README).

Usage :
    python -I temple_parthenon_from_readme.py --readme <README.md> --write [--out <json>]
    python -I temple_parthenon_from_readme.py --readme <README.md> --check [--out <json>]
"""

from __future__ import annotations

import sys
from pathlib import Path

# python -I n'ajoute pas le repertoire du script au chemin d'import : le moteur, voisin, y est mis explicitement.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from monument_from_readme import (
    NUM,
    TIMES,
    AxesSelector,
    ResultSelector,
    RowSelector,
    main,
)

MONUMENT = "temple-parthenon"
DOCUMENT = "Architecture/Antique/Parthenon/README.md"
DERIVED_BY = "src/cgarchi/data/temple_parthenon_from_readme.py"
DEFAULT_OUT = Path(__file__).resolve().with_name("temple_parthenon.json")

# Cote de [P] en pieds decimaux, entre parentheses dans la cellule de valeur.
FEET = rf"\({NUM} ft"


def feet(keys: tuple[str, ...], section: str, label: str, status: str, sources: tuple[str, ...]) -> RowSelector:
    return RowSelector(keys, section, label, FEET, "ft", status, sources, anchored=False)


ROWS = (
    RowSelector(("peristyle.frontColumns", "peristyle.flankColumns"), "2.", "Colonnes du péristyle",
                rf"\*\*(\d+) {TIMES} (\d+)\*\*", "1", "lu", ("D",)),
    feet(("krepis.stylobateWidth",), "3.2", "Stylobate, largeur", "lu", ("P",)),
    feet(("krepis.stylobateLength",), "3.2", "Stylobate, longueur", "lu", ("P",)),
    RowSelector(("krepis.stepCount",), "3.2", "Degrés", r"\*\*(\d+)\*\*", "1", "lu", ("P",), exact=True),
    feet(("krepis.stylobateHeight",), "3.2", "Stylobate (degré supérieur) : hauteur", "lu", ("P",)),
    feet(("krepis.secondStepHeight",), "3.2", "Deuxième degré : hauteur", "lu", ("P",)),
    feet(("krepis.thirdStepHeight",), "3.2", "Troisième degré : hauteur", "lu", ("P",)),
    feet(("krepis.secondStepTread",), "3.2", "Giron des degrés, deuxième degré", "lu", ("P",)),
    feet(("krepis.thirdStepTread",), "3.2", "Giron des degrés, troisième degré", "lu", ("P",)),
    RowSelector(("krepis.cornerMargin",), "3.2", "Débord du stylobate au-delà du fût", rf"\*\*{NUM} m\*\*", "m", "déduit",
                ("P",)),
    feet(("krepis.euthynteriaHeight",), "3.2", "Euthyntéria (assise de réglage) : hauteur retenue", "convention", ()),
    feet(("krepis.euthynteriaTread",), "3.2", "Euthyntéria (assise de réglage) : giron", "déduit", ("P",)),
    feet(("peristyle.frontSpacing",), "4.1", "Entraxe courant, façades", "lu", ("P",)),
    feet(("peristyle.flankSpacing",), "4.1", "Entraxe courant, flancs", "lu", ("P",)),
    feet(("peristyle.frontCornerSpacing",), "4.1", "Entraxe d'angle, façades", "déduit", ("P",)),
    feet(("peristyle.flankCornerSpacing",), "4.1", "Entraxe d'angle, flancs", "déduit", ("P",)),
)

SERIES = ()
POLYGONS = ()
PROSE = ()
BULLETS = ()
TABLES = ()

AXES = (
    AxesSelector("controls.frontAxesY", "4.2", "y"),
    AxesSelector("controls.flankAxesX", "4.2", "x"),
)

# Emprise de l'euthynteria, controles de la section 12 (« 111,311 ft = 33,928 m ✓ »), en metres.
RESULTS = (
    ResultSelector("krepisControls.footprintWidth", "12.", "Krépis, largeur", rf"\d+,\d+ ft = {NUM} m ✓"),
    ResultSelector("krepisControls.footprintLength", "12.", "Krépis, longueur", rf"\d+,\d+ ft = {NUM} m ✓"),
)

CONTROLS = ()
PROFILES = ()

BLOCKS = ("krepis", "peristyle", "controls", "krepisControls")


if __name__ == "__main__":
    sys.exit(main(sys.modules[__name__]))

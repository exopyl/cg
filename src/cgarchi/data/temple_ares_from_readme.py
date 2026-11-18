#!/usr/bin/env python3
"""Derivation de temple_ares.json depuis le README du temple d'Ares.

Source : README du temple, tenu hors du depot dans les documents de l'utilisateur
(Architecture/Antique/TempleAres/README.md). Ce module porte les tables du monument
(selecteurs, blocs documentes) ; les regles de lecture, le rendu et les modes
--write et --check sont ceux du moteur, monument_from_readme.py, dont la
documentation fait foi. Blocs documentes : krepis et peristyle, avec leurs
controles ; la colonne, l'entablement et le fronton sont absents (mode partiel,
registre MO-P2).

Usage :
    python -I temple_ares_from_readme.py --readme <README.md> --write [--out <json>]
    python -I temple_ares_from_readme.py --readme <README.md> --check [--out <json>]
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

MONUMENT = "temple-ares"
DOCUMENT = "Architecture/Antique/TempleAres/README.md"
DERIVED_BY = "src/cgarchi/data/temple_ares_from_readme.py"
DEFAULT_OUT = Path(__file__).resolve().with_name("temple_ares.json")


ROWS = (
    RowSelector(("peristyle.frontColumns", "peristyle.flankColumns"), "2.", "Colonnes du péristyle",
                rf"(\d+) {TIMES} (\d+) :", "1", "lu", ("A",)),
    RowSelector(("krepis.stylobateWidth", "krepis.stylobateLength"), "3.3", f"Stylobate : largeur {TIMES} longueur",
                rf"\*\*{NUM} {TIMES} {NUM} m\*\*", "m", "déduit", ("M",)),
    RowSelector(("krepis.stepCount",), "3.3", "Degrés", r"\*\*(\d+)\*\*", "1", "lu", ("M",), exact=True),
    RowSelector(("krepis.stylobateHeight",), "3.3", "Stylobate : hauteur", rf"\*\*{NUM} m\*\*", "m", "lu", ("M",)),
    RowSelector(("krepis.secondStepHeight",), "3.3", "Degré moyen : hauteur", rf"\*\*{NUM} m\*\*", "m", "lu", ("M",)),
    RowSelector(("krepis.thirdStepHeight",), "3.3", "Degré inférieur : hauteur", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
    RowSelector(("krepis.secondStepTread",), "3.3", "Giron du degré moyen", rf"\*\*{NUM} m\*\*", "m", "lu", ("M",)),
    RowSelector(("krepis.thirdStepTread",), "3.3", "Giron du degré inférieur", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
    RowSelector(("krepis.cornerMargin",), "3.3", "Débord du stylobate au-delà du fût", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
    RowSelector(("krepis.euthynteriaHeight",), "3.3", "Euthyntéria : hauteur", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
    RowSelector(("krepis.euthynteriaTread",), "3.3", "Débord de l'euthyntéria", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
    RowSelector(("peristyle.frontSpacing",), "4.1", "Entraxe normal, façade", rf"\*\*{NUM} m\*\*", "m", "lu", ("M",)),
    RowSelector(("peristyle.flankSpacing",), "4.1", "Entraxe normal, flanc", rf"\*\*{NUM} m\*\*", "m", "lu", ("M",)),
    RowSelector(("peristyle.frontCornerSpacing",), "4.1", "Entraxe d'angle, façade", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
    RowSelector(("peristyle.flankCornerSpacing",), "4.1", "Entraxe d'angle, flanc", rf"\*\*{NUM} m\*\*", "m", "lu",
                ("M",)),
)

SERIES = ()
POLYGONS = ()
PROSE = ()
BULLETS = ()
TABLES = ()

AXES = (
    AxesSelector("controls.frontAxesY", "4.3", "y"),
    AxesSelector("controls.flankAxesX", "4.3", "x"),
)

# Emprise de l'euthynteria, controles de la section 12 (« 16,125 = [M] ✓ »).
RESULTS = (
    ResultSelector("krepisControls.footprintWidth", "12.", "Krépis, largeur", rf"{NUM} = \[M\] ✓"),
    ResultSelector("krepisControls.footprintLength", "12.", "Krépis, longueur", rf"{NUM} = \[M\] ✓"),
)

CONTROLS = ()
PROFILES = ()

BLOCKS = ("krepis", "peristyle", "controls", "krepisControls")


if __name__ == "__main__":
    sys.exit(main(sys.modules[__name__]))

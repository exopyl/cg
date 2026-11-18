#!/usr/bin/env python3
"""Derivation de temple_hephaistos.json depuis le README du temple d'Hephaistos.

Source : README du temple, tenu hors du depot dans les documents de l'utilisateur
(Architecture/Antique/TempleHephaistos/README.md). Ce module porte les tables du
monument (selecteurs, blocs documentes) ; les regles de lecture, le rendu et les
modes --write et --check sont ceux du moteur, monument_from_readme.py, dont la
documentation fait foi.

Usage :
    python -I temple_hephaistos_from_readme.py --readme <README.md> --write [--out <json>]
    python -I temple_hephaistos_from_readme.py --readme <README.md> --check [--out <json>]
"""

from __future__ import annotations

import sys
from pathlib import Path

# python -I n'ajoute pas le repertoire du script au chemin d'import : le moteur, voisin, y est mis explicitement.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from monument_from_readme import (
    FEET,
    FEET_INCHES,
    NUM,
    RATIO,
    TIMES,
    AxesSelector,
    BulletSelector,
    ControlSelector,
    PolygonSelector,
    ProfileSelector,
    ProseSelector,
    ResultSelector,
    RowSelector,
    SeriesSelector,
    TableSelector,
    main,
)

MONUMENT = "temple-hephaistos"
DOCUMENT = "Architecture/Antique/TempleHephaistos/README.md"
DERIVED_BY = "src/cgarchi/data/temple_hephaistos_from_readme.py"
DEFAULT_OUT = Path(__file__).resolve().with_name("temple_hephaistos.json")

ROWS = (
    RowSelector(("peristyle.frontColumns", "peristyle.flankColumns"), "2.", "Colonnes du péristyle",
                rf"(\d+) {TIMES} (\d+),", "1", "lu", ("D", "S", "W")),
    RowSelector(("krepis.stylobateWidth", "krepis.stylobateLength"), "3.2", f"Stylobate, largeur {TIMES} longueur",
                rf"\*\*{NUM} {TIMES} {NUM} m\*\*", "m", "lu", ("D",)),
    RowSelector(("krepis.stepCount",), "3.2", "Degrés", r"(\d+) ;", "1", "lu", ("D", "L", "W")),
    # Hauteurs du stylobate et du 2e degre reprises de [O] (plan 3 section 26, Q1).
    RowSelector(("krepis.stylobateHeight",), "3.2", "Hauteur du stylobate", rf"\*\*{NUM} m\*\*", "m", "lu", ("O",)),
    RowSelector(("krepis.secondStepHeight",), "3.2", "Hauteur du 2ᵉ degré", rf"\*\*{NUM} m\*\*", "m", "lu", ("O",)),
    RowSelector(("krepis.thirdStepHeight",), "3.2", "Hauteur du 3ᵉ degré", rf"≈ {NUM} m", "m", "convention", ()),
    RowSelector(("krepis.secondStepTread",), "3.2", "Giron du 2ᵉ degré", rf"\*\*{NUM} m\*\*", "m", "lu", ("S",)),
    RowSelector(("krepis.thirdStepTread",), "3.2", "Giron du 3ᵉ degré", rf"≈ {NUM} m", "m", "convention", ()),
    RowSelector(("krepis.cornerMargin",), "3.2", "Débord du stylobate au-delà du nu des colonnes d'angle",
                rf"\*\*{NUM} m\*\*", "m", "déduit", ("S",)),
    # Euthynteria de [O] (plan 3 section 26, Q6) : hauteur en tete de cellule, ressaut par son libelle (R2) ; statut
    # par etiquette (R1).
    RowSelector(("krepis.euthynteriaHeight",), "3.2", "Euthyntéria", rf"hauteur \*\*{NUM} m\*\*", "m", "lu", ("O",),
                status_label="hauteur"),
    RowSelector(("krepis.euthynteriaTread",), "3.2", "Euthyntéria", rf"ressaut \*\*{NUM} m\*\*", "m", "lu", ("O",),
                anchored=False, status_label="ressaut"),
    RowSelector(("columns.lowerDiameter",), "4.1", "Diamètre inférieur D", rf"\*\*{NUM} m\*\*", "m", "lu", ("D",)),
    RowSelector(("columns.cornerLowerDiameter",), "4.1", "Diamètre des colonnes d'angle D_a",
                rf"\*\*{NUM} m\*\*", "m", "lu", ("S",)),
    RowSelector(("columns.upperDiameter",), "4.1", "Diamètre supérieur d", rf"\*\*{NUM} m\*\*", "m", "lu", ("D",)),
    RowSelector(("columns.height",), "4.1", "Hauteur de colonne", rf"\*\*{NUM} m\*\*", "m", "lu", ("D",)),
    RowSelector(("columns.fluteCount",), "5.1", "Cannelures", r"\*\*(\d+)\*\*,", "1", "lu", ("D", "S"), exact=True),
    RowSelector(("columns.fluteDepth",), "5.1", "Profondeur", rf"\*\*{NUM} in = \d+,\d+ m\*\*", "in", "lu", ("S",)),
    RowSelector(("columns.entasisSagitta",), "5.1", "Entasis", rf"flèche maximale ≈ \*\*{NUM} mm\*\*", "mm",
                "déduit", ("D",), anchored=False, status_label="flèche"),
    RowSelector(("columns.entasisHeight",), "5.1", "Position du maximum d'entasis",
                rf"\*\*mi-hauteur du fût\*\* \({NUM} m au-dessus du stylobate\)", "m", "déduit", (),
                anchored=False),
    RowSelector(("columns.abacusHeight",), "5.2", "Abaque", rf"{NUM} in = ", "in", "lu", ("S",),
                status_label="hauteur"),
    RowSelector(("columns.abacusHalfWidth",), "5.2", "Abaque", rf"{NUM} in = ", "in", "lu", ("S",), cell=2,
                status_label="demi-largeur"),
    RowSelector(("columns.echinusHeight",), "5.2", "Échine", rf"{NUM} in = ", "in", "lu", ("S",),
                status_label="hauteur"),
    RowSelector(("columns.annuletCount",), "5.2", "Annelets", r"\*\*(\d+) annelets\*\*", "1", "lu", ("S",),
                anchored=False, status_label="nombre"),
    RowSelector(("columns.annuletsHeight",), "5.2", "Annelets", rf"{NUM} in = ", "in", "lu", ("S",),
                status_label="hauteur"),
    RowSelector(("columns.annuletsTopRetreat", "columns.annuletsBottomRetreat"), "5.2", "Annelets",
                rf"retraits {NUM} → {NUM} in", "in", "lu", ("S",), cell=2, anchored=False,
                status_label="retraits extrêmes"),
    RowSelector(("columns.hypotrachelionHeight",), "5.2", "Hypotrachélion", rf"{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.architraveHeight",), "6.1", "Architrave",
                rf"{FEET} {NUM} in =", FEET_INCHES, "lu", ("S",), exact=True),
    RowSelector(("entablature.taeniaHeight",), "6.1", "— ténia", rf"{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.regulaHeight",), "6.1", "— regula", rf"{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.guttaeHeight",), "6.1", "— gouttes", rf"{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.architraveFaceHeight",), "6.1", "— face lisse", rf"{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.friezeHeight",), "6.1", "Frise",
                rf"{FEET} {NUM} in =", FEET_INCHES, "lu", ("S",), exact=True),
    RowSelector(("entablature.triglyphCapitalHeight",), "6.1", "— chapiteau du triglyphe", rf"{NUM} in = ", "in",
                "lu", ("S",)),
    RowSelector(("entablature.corniceHeight",), "6.1", "Corniche (géison)",
                rf"{FEET} {NUM} in =", FEET_INCHES, "lu", ("S",), exact=True),
    RowSelector(("entablature.mutulePlateHeight",), "6.1", "— membres de la corniche",
                rf"\*\*mutules\*\* \(plaque\) {NUM} in", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.dripHeight",), "6.1", "— membres de la corniche",
                rf"\*\*bec sous le larmier\*\* {NUM} in", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.coronaHeight",), "6.1", "— membres de la corniche",
                rf"\*\*larmier\*\* {NUM} in", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.filletHeight",), "6.1", "— membres de la corniche",
                rf"\*\*filet\*\* {NUM} in", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.crownHeight",), "6.1", "— membres de la corniche",
                rf"\*\*listel\*\* {NUM} in", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.architraveTopProjection",), "6.2", "Architrave (face)",
                rf"\*\*{NUM} in = \d+,\d+ m\*\* en haut", "in", "lu", ("S",)),
    RowSelector(("entablature.architraveFootProjection",), "6.2", "Architrave (face)",
                rf"\*\*{NUM} in = \d+,\d+ m\*\* en bas", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.taeniaProjection",), "6.2", "Ténia", rf"\*\*{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.triglyphFace",), "6.2", "Triglyphe (face)", rf"\*\*{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.triglyphCapitalBandProjection",), "6.2", "Chapiteau du triglyphe, saillies",
                rf"\*\*{NUM} in = \d+,\d+ m\*\* pour la bande", "in", "lu", ("S",)),
    RowSelector(("entablature.triglyphCapitalListelProjection",), "6.2", "Chapiteau du triglyphe, saillies",
                rf"\*\*{NUM} in = \d+,\d+ m\*\* pour le listel", "in", "lu", ("S",), anchored=False),
    RowSelector(("entablature.triglyphCapitalListelHeight",), "6.2", "Chapiteau du triglyphe, partage",
                rf"listel supérieur \*\*{NUM} in = ", "in", "convention", ()),
    RowSelector(("entablature.triglyphCapitalBandHeight",), "6.2", "Chapiteau du triglyphe, partage",
                rf"bande \*\*{NUM} in = ", "in", "convention", (), anchored=False),
    RowSelector(("entablature.metopeFace",), "6.2", "Métope (face)", rf"= \*\*{NUM} in = ", "in", "déduit", ("S",),
                anchored=False),
    RowSelector(("entablature.mutulePlateProjection",), "6.2", "Mutules (plaque)", rf"\*\*{NUM} in = ", "in", "lu",
                ("S",)),
    RowSelector(("entablature.dripProjection",), "6.2", "Bec sous le larmier", rf"\*\*{NUM} in = ", "in", "lu",
                ("S",)),
    RowSelector(("entablature.coronaProjection",), "6.2", "Larmier (face)", rf"\*\*{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.filletProjection",), "6.2", "Filet", rf"\*\*{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.crownProjection",), "6.2", "Couronnement de la corniche", rf"\*\*{NUM} in = ", "in",
                "lu", ("S",)),
    RowSelector(("entablature.innerFace",), "6.2", "Face intérieure", r"—", "m", "manquant", ()),
    RowSelector(("entablature.triglyphWidth",), "6.3", "Largeur du triglyphe",
                rf"\*\*{FEET} {NUM} in =", FEET_INCHES, "lu", ("S",)),
    RowSelector(("entablature.metopeWidth",), "6.3", "Largeur de la métope (lue)",
                rf"{FEET} {NUM} in =", FEET_INCHES, "lu", ("S",)),
    RowSelector(("entablature.channelDepth",), "6.3", "Profondeur des canaux", rf"≈ \*\*{NUM} in = ", "in",
                "mesuré", ("S",)),
    # Regulae et leurs gouttes (l. 212, 231, 232), mutules (l. 273, 274).
    RowSelector(("entablature.regulaProjection",), "6.2", "Regulae", rf"\*\*{NUM} in = ", "in", "lu", ("S",)),
    RowSelector(("entablature.regulaGuttaCount",), "6.1", "— gouttes", r"; (\d+) par regula,", "1", "lu", ("S",),
                anchored=False),
    RowSelector(("entablature.regulaGuttaDiameter",), "6.1", "— gouttes", rf"Ø \*\*{NUM} in\*\*", "in", "lu", ("S",),
                anchored=False),
    RowSelector(("entablature.regulaGuttaGap",), "6.1", "— gouttes", rf"vides \*\*{NUM} in\*\*", "in", "lu", ("S",),
                anchored=False),
    RowSelector(("entablature.regulaGuttaTopProjection",), "6.2", "Gouttes des regulae", rf"{NUM} in en haut", "in",
                "lu", ("S",)),
    RowSelector(("entablature.regulaGuttaFootProjection",), "6.2", "Gouttes des regulae", rf"{NUM} in en bas", "in",
                "lu", ("S",), anchored=False),
    RowSelector(("entablature.mutuleWidth",), "6.4", "Mutules",
                rf"largeur {FEET} {NUM} in =", FEET_INCHES, "lu", ("S",), anchored=False, exact=True),
    RowSelector(("entablature.mutuleDepth",), "6.4", "Profondeur des mutules",
                rf"\*\*{NUM} in = \d+,\d+ m\*\*, mesurés le long", "in", "lu", ("S",)),
    # Sous-face du geison (l. 276, R9) et gouttes des mutules (l. 275) : diametres et vides des tetes, hauteur,
    # premier axe en largeur, gouttes par rang, evasement du pied en diametre.
    RowSelector(("entablature.soffitSlope",), "6.4", "Sous-face du géison", rf"\*\*inclinée de {NUM}°\*\*", "°",
                "mesuré", ("S",), status_label="pente"),
    RowSelector(("entablature.mutuleGuttaFrontDiameter",), "6.4", "Gouttes des mutules", rf"avant {NUM} in, milieu",
                "in", "lu", ("S",), anchored=False, status_label="diamètres"),
    RowSelector(("entablature.mutuleGuttaMiddleDiameter",), "6.4", "Gouttes des mutules",
                rf"milieu {NUM} in, arrière", "in", "lu", ("S",), anchored=False, status_label="diamètres"),
    RowSelector(("entablature.mutuleGuttaRearDiameter",), "6.4", "Gouttes des mutules", rf"arrière {NUM} in ;", "in",
                "lu", ("S",), anchored=False, status_label="diamètres"),
    RowSelector(("entablature.mutuleGuttaFrontGap", "entablature.mutuleGuttaRearGap"), "6.4", "Gouttes des mutules",
                rf"vides {NUM} et {NUM} in", "in", "lu", ("S",), anchored=False, status_label="vides"),
    RowSelector(("entablature.mutuleGuttaHeight",), "6.4", "Gouttes des mutules", rf"Hauteur \*\*{NUM} in = ", "in",
                "mesuré", ("S",), anchored=False, status_label="hauteur"),
    RowSelector(("entablature.mutuleGuttaEdgeAxis",), "6.4", "Gouttes des mutules",
                rf"premier axe à {NUM} in du bord", "in", "lu", ("S",), anchored=False,
                status_label="premier axe en largeur"),
    RowSelector(("entablature.mutuleGuttaPerRow",), "6.4", "Gouttes des mutules", r"^\d+ rangs de (\d+)\.", "1", "lu",
                ("S",), status_label="gouttes par rang"),
    RowSelector(("entablature.mutuleGuttaFootFlare",), "6.4", "Gouttes des mutules",
                rf"plus large de \*\*{NUM} in\*\* en diamètre", "in", "déduit", ("S",), anchored=False,
                status_label="forme"),
    # Fronton (l. 399, 402, 406) : hauteur et saillie en pieds et pouces (R6), nu du tympan (R2, R1), sima manquante.
    RowSelector(("pediment.apexRise",), "9.", "Hauteur du fronton", rf"{FEET} {NUM} in = ", FEET_INCHES, "lu",
                ("S",)),
    # Pente 1 : r de [O] (R12), en tete de la cellule « Pente des rampants » (plan 3 section 26).
    RowSelector(("pediment.rakingRun",), "9.", "Pente des rampants", rf"\*\*1 : {NUM}\*\*", RATIO, "lu", ("O",)),
    RowSelector(("pediment.rakingProjection",), "9.", "Corniche rampante : saillie", rf"{FEET} {NUM} in = ",
                FEET_INCHES, "lu", ("S",)),
    RowSelector(("pediment.tympanumFace",), "9.", "Corniche rampante : saillie",
                rf"= \*\*{NUM} in = \d+,\d+ m\*\* de l'axe des colonnes", "in", "déduit", ("S",), anchored=False,
                status_label="nu du tympan"),
    RowSelector(("pediment.sima",), "9.", "Sima, acrotères, tuiles", r"—", "m", "manquant", ()),
)

SERIES = (
    SeriesSelector("entablature.triglyphDivisions", "6.3", "Division du triglyphe", "en plan : ",
                   ("demi-canal", "bande", "canal"), 7, "in", "lu", ("S",)),
    # Hauteurs normales de la corniche rampante (l. 401), de haut en bas.
    SeriesSelector("pediment.rakingHeights", "9.", "Corniche rampante : hauteur", "de haut en bas : ",
                   ("listel", "filet", "larmier", "gorge de la sous-face (bec)"), 5, "in", "lu", ("S",)),
)

POLYGONS = (
    # Section de la corniche rampante (l. 402) : u depuis le nu du tympan, h normale a la pente, en pouces.
    PolygonSelector("pediment.rakingSection", "9.", "Corniche rampante : saillie",
                    "Ordre : depuis le bas du dos, vers l'avant, puis par le dessus : ",
                    ", puis retour au premier sommet.", 18, "in", "mesuré", ("S",), "section en polygone"),
)

PROSE = (
    ProseSelector("peristyle.frontCornerSpacing", "4.2", rf"\*\*a = {NUM} m\*\*", "m", "déduit", ("S",)),
    ProseSelector("peristyle.flankCornerSpacing", "4.2", rf"\*\*a_f = {NUM} m\*\*", "m", "déduit", ("S",)),
    ProseSelector("peristyle.frontSpacing", "4.2", rf"\*\*n = {NUM} m\*\*", "m", "déduit", ()),
    ProseSelector("peristyle.flankSpacing", "4.2", rf"\*\*n_f = {NUM} m\*\*", "m", "déduit", ()),
)

BULLETS = (
    BulletSelector("columns.grooveHeight", "5.2", rf"la rainure a \*\*{NUM} in = ", "in", "lu", ("S",)),
    BulletSelector("columns.grooveDepth", "5.2", r"\*\*la profondeur\*\*", "m", "manquant", ()),
    BulletSelector("columns.echinusLipRadius", "5.2", rf"de rayon \*\*{NUM} in = ", "in", "mesuré", ("S",)),
)

TABLES = (
    TableSelector("columns.echinusJunctionDepth", "5.2", "Profondeur (in)", "1,7", "Profondeur (in)",
                  "Statut de la profondeur", "in", "mesuré", ("S",)),
    TableSelector("columns.echinusJunctionRetreat", "5.2", "Profondeur (in)", "1,7", "Retrait (in)",
                  "Statut du retrait", "in", "lu", ("S",)),
    TableSelector("columns.echinusTopRetreat", "5.2", "Profondeur (in)", "0", "Retrait (in)",
                  "Statut du retrait", "in", "lu", ("S",)),
)

AXES = (
    AxesSelector("controls.frontAxesY", "4.3", "y"),
    AxesSelector("controls.flankAxesX", "4.3", "x"),
)

RESULTS = (
    ResultSelector("controls.frontSum", "12.", f"Façade : 2 {TIMES} 0,057"),
    ResultSelector("controls.flankSum", "12.", f"Flanc : 2 {TIMES} 0,057"),
)

CONTROLS = (
    ControlSelector("columnControls.shaftHeight", "5.1", "Hauteur du fût", 1, rf"= \*\*{NUM} m\*\*", "m"),
    ControlSelector("columnControls.footFluteChord", "5.1", "Corde d'une cannelure au pied", 1,
                    rf"\*\*{NUM} in = \d+,\d+ m\*\*", "in"),
    ControlSelector("columnControls.fluteCentralRadius", "5.1", "Tracé de la cannelure", 2,
                    rf"\*\*R = {NUM} in = ", "in"),
    ControlSelector("columnControls.fluteLateralRadius", "5.1", "Tracé de la cannelure", 2,
                    rf"R/3 = \*\*{NUM} in = ", "in"),
    ControlSelector("columnControls.topFluteDepth", "5.1", "Cannelures au sommet du fût", 1,
                    rf"profondeur [^=]*= {NUM} in \(", "in"),
    ControlSelector("columnControls.cornerFluteChord", "4.4", "Cannelures au pied", 1,
                    rf"sin 9° = \*\*{NUM} m\*\*", "m"),
    ControlSelector("columnControls.cornerFluteDepth", "4.4", "Cannelures au pied", 1,
                    rf"= \*\*\d+,\d+ in = {NUM} m\*\*", "m"),
    ControlSelector("columnControls.capitalHeight", "12.", "Chapiteau : 7,85 + 6,5 + 1,234 + 4,3 = 19,9 in", -1,
                    rf"^{NUM} ✓", "in", unit_in_label=True),
    ControlSelector("columnControls.annuletsAndRelief", "12.", "Annelets et ressaut", 0, rf"= {NUM} in$", "in"),
    ControlSelector("columnControls.upperRadius", "12.", "Rayon supérieur", 0, rf"= {NUM} in ;", "in"),
    ControlSelector("columnControls.echinusLipRetreat", "5.2", "", 0,
                    rf"qui saille jusqu'à {NUM} in du nu de l'abaque", "in"),
    ControlSelector("entablatureControls.total", "6.1", "**Total**", 1, rf"\*\*{NUM} m\*\*", "m"),
    ControlSelector("entablatureControls.corniceSum", "12.", "Corniche : 2,35", -1, rf"^{NUM} in = 1 ft", "in"),
    ControlSelector("entablatureControls.triglyphDivisionSum", "12.", "Triglyphe (pl. VI fig. 3)", -1,
                    rf"^{NUM} in = largeur", "in"),
    ControlSelector("entablatureControls.frontMetope", "6.3", "Largeur de la métope courante", 1,
                    rf"\*\*{NUM} m\*\* en façade", "m"),
    ControlSelector("entablatureControls.flankMetope", "6.3", "Largeur de la métope courante", 1,
                    rf"\*\*{NUM} m\*\* en flanc", "m"),
    ControlSelector("entablatureControls.metopeCount", "6.3", "Disposition", 1, r"\*\*(\d+) métopes\*\*", "1"),
    ControlSelector("entablatureControls.frontTriglyphs", "6.3", "Disposition", 1, r"façade (\d+) triglyphes", "1"),
    ControlSelector("entablatureControls.flankTriglyphs", "6.3", "Disposition", 1, r"flanc (\d+) et", "1"),
    ControlSelector("entablatureControls.channelBottom", "6.3", "Profondeur des canaux", -1, rf"= {NUM} m\)", "m"),
    ControlSelector("entablatureControls.metopeRecess", "6.2", "Métope (face)", 1, rf"\*\* : {NUM} in \(", "in"),
    # Angle de la frise (l. 255-256) : premiere solution, puis seconde (intermediaire au milieu de la travee).
    ControlSelector("entablatureControls.cornerTriglyphOffset", "6.3", "Triglyphe d'angle", 1,
                    rf"= \*\*{NUM} m\*\* au-delà", "m"),
    ControlSelector("entablatureControls.frontCornerDistance", "6.3", "Métopes d'angle", 1,
                    rf"vaut a \+ 0,231 = {NUM} m", "m"),
    ControlSelector("entablatureControls.frontCornerMetope", "6.3", "Métopes d'angle", 1,
                    rf"= \*\*{NUM} m\*\* \(flanc", "m"),
    ControlSelector("entablatureControls.flankCornerMetope", "6.3", "Métopes d'angle", 1, rf"\(flanc : {NUM} m\)", "m"),
    ControlSelector("entablatureControls.intermediateShift", "6.3", "Métopes d'angle", 1,
                    rf"décalé de \*\*{NUM} m\*\*", "m"),
    ControlSelector("entablatureControls.frontMidBayInnerMetope", "6.3", "Métopes d'angle", 1,
                    rf"valent {NUM} et {NUM} m \(flanc", "m"),
    ControlSelector("entablatureControls.frontMidBayOuterMetope", "6.3", "Métopes d'angle", 1,
                    rf"et {NUM} m \(flanc", "m"),
    ControlSelector("entablatureControls.flankMidBayInnerMetope", "6.3", "Métopes d'angle", 1,
                    rf"\(flanc : {NUM} et {NUM} m\)", "m"),
    ControlSelector("entablatureControls.flankMidBayOuterMetope", "6.3", "Métopes d'angle", 1,
                    rf"et {NUM} m\)$", "m"),
    # Gouttes des regulae (l. 212, 277) et dos des mutules (l. 274).
    ControlSelector("entablatureControls.regulaGuttaMargin", "6.1", "— gouttes", -1, rf"marges de \*\*{NUM} in\*\*",
                    "in"),
    ControlSelector("entablatureControls.regulaGuttaShortfall", "6.1", "— gouttes", -1, rf"soit \*\*{NUM} in \(", "in"),
    ControlSelector("entablatureControls.regulaGuttaCountCheck", "6.4", "Gouttes des regulae", 1, r"^(\d+) par regula$",
                    "1"),
    ControlSelector("entablatureControls.mutuleBack", "6.4", "Profondeur des mutules", 1,
                    rf"= \*\*{NUM} in = \d+,\d+ m\*\* de l'axe", "in"),
    # Gouttes des mutules (l. 275) : rangs, diviseur du pas, axes, pas, et forme (evasement en rayon, sa valeur non
    # arrondie, avant du pied) ; profondeur sous la pente (l. 274) ; sous-face du geison (l. 276).
    ControlSelector("entablatureControls.mutuleGuttaRows", "6.4", "Gouttes des mutules", 1, r"^(\d+) rangs de \d+\.",
                    "1"),
    ControlSelector("entablatureControls.mutuleGuttaPitchCount", "6.4", "Gouttes des mutules", 1, r"\) / (\d+) = \*\*",
                    "1"),
    ControlSelector("entablatureControls.mutuleGuttaMiddleAxis", "6.4", "Gouttes des mutules", 1,
                    rf"{NUM} et {NUM} in du devant", "in"),
    ControlSelector("entablatureControls.mutuleGuttaRearAxis", "6.4", "Gouttes des mutules", 1,
                    rf"et {NUM} in du devant", "in"),
    ControlSelector("entablatureControls.mutuleGuttaPitch", "6.4", "Gouttes des mutules", 1,
                    rf"\) / \d+ = \*\*{NUM} in = ", "in"),
    ControlSelector("entablatureControls.mutuleRearMargin", "6.4", "Profondeur des mutules", 1,
                    rf"marge arrière {NUM} in\.", "in"),
    ControlSelector("entablatureControls.mutuleHorizontalDepth", "6.4", "Profondeur des mutules", 1,
                    rf"projection horizontale vaut {NUM} in et", "in"),
    ControlSelector("entablatureControls.mutuleInclinedBack", "6.4", "Profondeur des mutules", 1,
                    rf"le dos est à {NUM} in = ", "in"),
    ControlSelector("entablatureControls.soffitPivotZ", "6.4", "Sous-face du géison", 1, rf"à \*\*z = {NUM} m\*\*", "m"),
    ControlSelector("entablatureControls.mutuleFrontTopZ", "6.4", "Sous-face du géison", 1,
                    rf"arête avant-haute à z = {NUM} m", "m"),
    ControlSelector("entablatureControls.soffitBackZ", "6.4", "Sous-face du géison", 1,
                    rf"\+ 3,84 in = \*\*{NUM} m\*\*", "m"),
    ControlSelector("entablatureControls.soffitRise", "6.4", "Sous-face du géison", 1, rf"7,377 \+ {NUM} in = ", "in"),
    ControlSelector("entablatureControls.geisonBackStep", "6.4", "Sous-face du géison", 1,
                    rf"descend de \*\*{NUM} in\*\*", "in"),
    ControlSelector("entablatureControls.dripTopZ", "6.4", "Sous-face du géison", 1, rf"à {NUM} m \(1,25 in\)", "m"),
    ControlSelector("entablatureControls.soffitPivotProjection", "6.4", "Sous-face du géison", 1,
                    rf"à \*\*{NUM} in = \d+,\d+ m\*\* de l'axe et", "in"),
    ControlSelector("entablatureControls.mutuleGuttaRadialFlare", "6.4", "Gouttes des mutules", 1,
                    rf"évasement de {NUM} in en rayon", "in"),
    ControlSelector("entablatureControls.mutuleGuttaRadialFlareExact", "6.4", "Gouttes des mutules", -1,
                    rf"= {NUM} in en rayon", "in"),
    ControlSelector("entablatureControls.mutuleGuttaFrontFootProjection", "6.4", "Gouttes des mutules", -1,
                    rf"l'avant du pied à {NUM} in", "in"),
    # Fronton (section 9) : hauteur, pente en degres (R9) et pied du haut de la corniche rampante ; hauteur de la
    # corniche rampante, normale et verticale ; saillies ; nu du tympan ; ecart des larmiers.
    ControlSelector("pedimentControls.apexRise", "9.", "Hauteur du fronton", 1, rf"= \*\*{NUM} m\*\*", "m"),
    ControlSelector("pedimentControls.rakingSlope", "9.", "Pente des rampants", 1, rf"≈ \*\*{NUM}°\*\*", "°"),
    ControlSelector("pedimentControls.eaveHalfSpan", "9.", "Pente des rampants", 1,
                    rf"= \*\*{NUM} m\*\* de l'axe du temple", "m"),
    ControlSelector("pedimentControls.rakingHeight", "9.", "Corniche rampante : hauteur", 1, rf"^{NUM} in = ", "in"),
    ControlSelector("pedimentControls.rakingHeightM", "9.", "Corniche rampante : hauteur", 1, rf"= \*\*{NUM} m\*\*",
                    "m"),
    ControlSelector("pedimentControls.rakingVerticalHeight", "9.", "Corniche rampante : hauteur", -1,
                    rf"= \*\*{NUM} in = \d+,\d+ m\*\*", "in"),
    ControlSelector("pedimentControls.rakingVerticalHeightM", "9.", "Corniche rampante : hauteur", -1,
                    rf"= \*\*\d+,\d+ in = {NUM} m\*\*", "m"),
    ControlSelector("pedimentControls.rakingProjection", "9.", "Corniche rampante : saillie", 1,
                    rf"= \*\*{NUM} m\*\* depuis", "m"),
    ControlSelector("pedimentControls.tympanumFaceM", "9.", "Corniche rampante : saillie", 1,
                    rf"= \*\*\d+,\d+ in = {NUM} m\*\* de l'axe", "m"),
    ControlSelector("pedimentControls.rakingListelProjection", "9.", "Corniche rampante : saillie", 1,
                    rf"listel, avec son talon, \*\*{NUM} in\*\*", "in"),
    ControlSelector("pedimentControls.rakingCoronaProjection", "9.", "Corniche rampante : saillie", 1,
                    rf"; larmier \*\*{NUM} in\*\*", "in"),
    ControlSelector("pedimentControls.rakingBottomFilletProjection", "9.", "Corniche rampante : saillie", 1,
                    rf"filet du bas jusqu'à {NUM} in", "in"),
    ControlSelector("pedimentControls.rakingCoronaGap", "9.", "Corniche rampante : saillie", -1, rf"à {NUM} in près",
                    "in"),
)

PROFILES = (
    ProfileSelector("columnControls.echinusProfile", "5.2", "Profondeur (in)", "Retrait (in)", "in"),
)


# Blocs documentes, tous ceux du format.
BLOCKS = ("krepis", "peristyle", "columns", "controls", "columnControls", "entablature", "entablatureControls",
          "pediment", "pedimentControls")


if __name__ == "__main__":
    sys.exit(main(sys.modules[__name__]))

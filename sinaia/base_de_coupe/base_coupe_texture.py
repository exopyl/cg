# -*- coding: utf-8 -*-
# Genere une base de coupe quadrillee : plaque PLATE + texture.
#
# Le quadrillage n'est plus grave dans la geometrie, il est PEINT. La piece
# geometrique retombe donc a une simple boite de 6 faces, et toute la fidelite
# metrique se deplace dans la texture.
#
# C'EST LA RESOLUTION QUI PORTE LA MESURE, et c'est pourquoi elle est choisie
# ENTIERE : 10 pixels par millimetre, soit exactement 100 pixels par centimetre.
# Une graduation k tombe alors sur le pixel 150 + 100k, sans arrondi, et le pas
# reste rigoureusement constant d'un bout a l'autre. Une resolution quelconque
# (disons 96 dpi) ferait deriver les traits d'une fraction de pixel chacun, et
# la plaque cesserait d'etre mesurable -- exactement le defaut a eviter.
#
# Les largeurs de trait sont PAIRES en pixels pour la meme raison : un trait de
# largeur impaire ne peut pas etre centre sur un pixel entier sans pencher d'un
# demi-pixel d'un cote.
#
# Le bandeau de marque de la plaque de reference n'est pas reproduit.

import io
from PIL import Image, ImageDraw, ImageFont

# --- Parametres, en MILLIMETRES -------------------------------------------
LONGUEUR   = 450.0
LARGEUR    = 300.0
EPAISSEUR  = 3.0

GRAD_X     = 42          # graduations, en cm
GRAD_Y     = 27
PAS        = 10.0        # 1 cm
ORIG_X     = (LONGUEUR - GRAD_X * PAS) / 2.0     # quadrillage centre
ORIG_Y     = (LARGEUR  - GRAD_Y * PAS) / 2.0

PPMM       = 10          # pixels par millimetre -- ENTIER, cf. en-tete
MAJEUR_TOUS = 5

FOND    = (57, 126, 82)
BISEAU  = (72, 141, 97)
MINEUR  = (132, 188, 152)
MAJEUR  = (238, 250, 242)
TEXTE   = (255, 255, 255)

L_MINEUR = 4             # largeurs en PIXELS, paires
L_MAJEUR = 10
L_DIAG   = 8
BISEAU_MM = 4.0

RACINE = r"C:\home\perso\cg\base_de_coupe"
POLICE = r"C:\Windows\Fonts\arialbd.ttf"

W, H = int(LONGUEUR * PPMM), int(LARGEUR * PPMM)


def px(x_mm):
    return int(round(x_mm * PPMM))


def py(y_mm):
    """Ordonnee image. La texture est dessinee y VERS LE HAUT, pour que la
    coordonnee v de l'OBJ (v = 0 en bas de l'image) s'y verse sans miroir."""
    return H - int(round(y_mm * PPMM))


def barre(d, cx, cy, demi_l, demi_h, couleur):
    d.rectangle([cx - demi_l, cy - demi_h, cx + demi_l - 1, cy + demi_h - 1],
                fill=couleur)


def texture():
    im = Image.new("RGB", (W, H), FOND)
    d = ImageDraw.Draw(im)

    b = px(BISEAU_MM)
    d.rectangle([0, 0, W - 1, b], fill=BISEAU)
    d.rectangle([0, H - 1 - b, W - 1, H - 1], fill=BISEAU)
    d.rectangle([0, 0, b, H - 1], fill=BISEAU)
    d.rectangle([W - 1 - b, 0, W - 1, H - 1], fill=BISEAU)

    x0, x1 = px(ORIG_X), px(ORIG_X + GRAD_X * PAS)
    y0, y1 = py(ORIG_Y), py(ORIG_Y + GRAD_Y * PAS)

    # Mineurs d'abord, majeurs ensuite : un trait fort recouvre alors le faible
    # au croisement, au lieu d'etre entaille par lui.
    for majeur in (False, True):
        c, w = (MAJEUR, L_MAJEUR) if majeur else (MINEUR, L_MINEUR)
        for k in range(GRAD_X + 1):
            if (k % MAJEUR_TOUS == 0) != majeur:
                continue
            x = px(ORIG_X + k * PAS)
            d.rectangle([x - w // 2, y1, x + w // 2 - 1, y0], fill=c)
        for k in range(GRAD_Y + 1):
            if (k % MAJEUR_TOUS == 0) != majeur:
                continue
            y = py(ORIG_Y + k * PAS)
            d.rectangle([x0, y - w // 2, x1, y + w // 2 - 1], fill=c)

    # Diagonales depuis l'origine, coupees au cadre du quadrillage.
    import math
    for deg in (30, 45, 60):
        t = math.tan(math.radians(deg))
        dx = min(GRAD_X * PAS, GRAD_Y * PAS / t)
        d.line([px(ORIG_X), py(ORIG_Y), px(ORIG_X + dx), py(ORIG_Y + dx * t)],
               fill=MAJEUR, width=L_DIAG)

    f = ImageFont.truetype(POLICE, int(2.6 * PPMM))
    for k in range(GRAD_X + 1):
        d.text((px(ORIG_X + k * PAS), py(ORIG_Y) + int(1.2 * PPMM)),
               str(k), font=f, fill=TEXTE, anchor="ma")
    for k in range(GRAD_Y + 1):
        # Les graduations de l'axe Y se lisent de bas en haut, comme sur la
        # plaque de reference : on rend l'etiquette a part et on la pivote.
        e = Image.new("RGBA", (int(5 * PPMM), int(4 * PPMM)), (0, 0, 0, 0))
        ImageDraw.Draw(e).text((int(2.5 * PPMM), int(2 * PPMM)), str(k),
                               font=f, fill=TEXTE + (255,), anchor="mm")
        e = e.rotate(90, expand=True)
        im.paste(e, (px(ORIG_X) - int(1.2 * PPMM) - e.width,
                     py(ORIG_Y + k * PAS) - e.height // 2), e)

    fd = ImageFont.truetype(POLICE, int(4.4 * PPMM))
    for deg, at in ((30, 17.0), (45, 12.5), (60, 8.0)):
        t = math.tan(math.radians(deg))
        d.text((px(ORIG_X + at * PAS) + int(2 * PPMM),
                py(ORIG_Y + at * PAS * t) + int(3 * PPMM)),
               u"%d\u00b0" % deg, font=fd, fill=TEXTE, anchor="lm")

    im.save(RACINE + ".png", optimize=True)
    return im


def modele():
    """Une boite. Le dessus porte la texture, les autres faces le vert uni :
    plaquer la texture sur les flancs y etirerait une colonne de pixels."""
    v = [(0, 0, 0), (LONGUEUR, 0, 0), (LONGUEUR, LARGEUR, 0), (0, LARGEUR, 0),
         (0, 0, EPAISSEUR), (LONGUEUR, 0, EPAISSEUR),
         (LONGUEUR, LARGEUR, EPAISSEUR), (0, LARGEUR, EPAISSEUR)]
    with io.open(RACINE + ".obj", "w", encoding="utf-8", newline="\n") as f:
        f.write("# Base de coupe quadrillee -- plaque plate, quadrillage TEXTURE.\n#\n")
        f.write("# Unite : le MILLIMETRE. Plaque %.0f x %.0f x %.0f mm.\n"
                % (LONGUEUR, LARGEUR, EPAISSEUR))
        f.write("# Quadrillage %d x %d cm centre, pas de 1 cm, trait fort tous les %d.\n"
                % (GRAD_X, GRAD_Y, MAJEUR_TOUS))
        f.write("# La texture fait %d x %d px, soit %d px/mm : une graduation k\n"
                % (W, H, PPMM))
        f.write("# tombe sur le pixel %d + %dk, sans arrondi.\n\n"
                % (px(ORIG_X), int(PAS * PPMM)))
        f.write("mtllib base_de_coupe.mtl\no base_de_coupe\n")
        for (x, y, z) in v:
            f.write("v %.4f %.4f %.4f\n" % (x, y, z))
        f.write("vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n")
        f.write("vn 0 0 1\nvn 0 0 -1\nvn 0 -1 0\nvn 1 0 0\nvn 0 1 0\nvn -1 0 0\n")
        f.write("\nusemtl dessus\n")
        f.write("f 5/1/1 6/2/1 7/3/1 8/4/1\n")
        f.write("\nusemtl flancs\n")
        f.write("f 1/1/2 4/2/2 3/3/2 2/4/2\n")     # dessous
        f.write("f 1/1/3 2/2/3 6/3/3 5/4/3\n")
        f.write("f 2/1/4 3/2/4 7/3/4 6/4/4\n")
        f.write("f 3/1/5 4/2/5 8/3/5 7/4/5\n")
        f.write("f 4/1/6 1/2/6 5/3/6 8/4/6\n")
    with io.open(RACINE + ".mtl", "w", encoding="utf-8", newline="\n") as f:
        f.write("# Materiaux de la base de coupe.\n\n")
        f.write("newmtl dessus\nKa 1 1 1\nKd 1 1 1\nKs 0.05 0.05 0.05\nNs 12\n"
                "d 1\nillum 2\nmap_Kd base_de_coupe.png\n\n")
        f.write("newmtl flancs\nKa %.4f %.4f %.4f\nKd %.4f %.4f %.4f\n"
                "Ks 0.05 0.05 0.05\nNs 12\nd 1\nillum 2\n"
                % tuple([c / 255.0 for c in FOND] * 2))


texture()
modele()
print("texture %d x %d px (%d px/mm)" % (W, H, PPMM))

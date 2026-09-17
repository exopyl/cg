#!/usr/bin/env python3
"""Genere sinaia.ico a partir de sinaia.xpm.

POURQUOI UN SCRIPT ET PAS UN .ICO POSE A LA MAIN
------------------------------------------------
Un binaire dans le depot sans moyen de le refabriquer est une impasse : le jour
ou le visuel change, personne ne sait avec quel outil ni quels reglages celui-ci
a ete produit. Le .ico est donc un ARTEFACT, et sa source reste sinaia.xpm.

    python sinaia/tools/make_icon.py

CHOIX DE REECHANTILLONNAGE, et il n'est pas neutre
--------------------------------------------------
La source fait 32x32 en 8 couleurs. C'est de la pixel-art, pas une image
continue, et les deux sens ne se traitent pas pareil :

  - REDUCTION (16, 20, 24) : LANCZOS. Moyenner plusieurs pixels sources est ce
    qu'on veut ; le nearest y perdrait des traits d'un pixel de large.
  - AGRANDISSEMENT D'UN FACTEUR ENTIER (64, 128, 256 = x2, x4, x8) : NEAREST.
    Il rend des carres nets, ce qui se lit comme un parti pris. LANCZOS y
    produirait un flou qui se lit, lui, comme un agrandissement rate.
  - AGRANDISSEMENT D'UN FACTEUR NON ENTIER (40, 48 = x1,25 et x1,5) : LANCZOS.
    Le nearest y doublerait certaines colonnes de pixels et pas d'autres, ce qui
    se voit immediatement sur des traits d'un pixel.

⚠ LIMITE ASSUMEE : aucune interpolation ne cree de l'information. A 256 px,
cette icone reste un 32x32 agrandi huit fois. Pour un rendu vraiment propre aux
grandes tailles il faut une source vectorielle ou un PNG 256x256 ; ce script
sera alors a rebrancher sur elle, en gardant la meme liste de tailles.
"""

import os
import sys

from PIL import Image

# Tailles embarquees. Windows pioche la plus proche selon le DPI et le contexte
# (16 : barre de titre et listes ; 32 : barre des taches ; 48 : Explorateur en
# icones moyennes ; 256 : grandes icones et ecrans haute densite).
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "sinaia.xpm")
DST = os.path.join(HERE, "..", "sinaia.ico")


def parse_xpm(path):
    """Rend une Image RGBA. Analyseur minimal : 1 caractere par pixel, forme
    `<char> c <couleur>`, `None` valant transparent. C'est tout ce que
    sinaia.xpm utilise ; un XPM plus riche ferait echouer bruyamment plutot que
    de rendre une image fausse."""
    with open(path, "r", encoding="utf-8") as f:
        raw = f.read()

    # Ne garder que les chaines entre guillemets : le reste est du C.
    rows = []
    for line in raw.splitlines():
        line = line.strip()
        if line.startswith('"'):
            end = line.rfind('"')
            if end > 0:
                rows.append(line[1:end])

    if not rows:
        raise SystemExit("XPM : aucune chaine trouvee dans %s" % path)

    header = rows[0].split()
    if len(header) < 4:
        raise SystemExit("XPM : en-tete illisible : %r" % rows[0])
    width, height, ncolors, cpp = (int(x) for x in header[:4])
    if cpp != 1:
        raise SystemExit("XPM : %d caracteres par pixel non gere (1 attendu)" % cpp)

    palette = {}
    for entry in rows[1:1 + ncolors]:
        char = entry[0]
        parts = entry[1:].split()
        if len(parts) < 2 or parts[0] != "c":
            raise SystemExit("XPM : entree de palette illisible : %r" % entry)
        value = parts[1]
        if value.lower() == "none":
            palette[char] = (0, 0, 0, 0)
        elif value.startswith("#") and len(value) == 7:
            palette[char] = (int(value[1:3], 16), int(value[3:5], 16),
                             int(value[5:7], 16), 255)
        else:
            raise SystemExit("XPM : couleur non geree : %r" % value)

    pixels = rows[1 + ncolors:1 + ncolors + height]
    if len(pixels) != height:
        raise SystemExit("XPM : %d lignes de pixels pour %d annoncees"
                         % (len(pixels), height))

    img = Image.new("RGBA", (width, height))
    put = img.putpixel
    for y, line in enumerate(pixels):
        if len(line) != width:
            raise SystemExit("XPM : ligne %d de %d pixels, %d attendus"
                             % (y, len(line), width))
        for x, char in enumerate(line):
            if char not in palette:
                raise SystemExit("XPM : caractere %r hors palette (ligne %d)"
                                 % (char, y))
            put((x, y), palette[char])
    return img


def main():
    base = parse_xpm(SRC)
    w, h = base.size
    print("source : %s  (%dx%d)" % (os.path.relpath(SRC, HERE), w, h))

    frames = []
    for size in SIZES:
        if size == w:
            frame = base.copy()
            how = "natif"
        elif size < w:
            frame = base.resize((size, size), Image.LANCZOS)
            how = "LANCZOS"
        elif size % w == 0:
            frame = base.resize((size, size), Image.NEAREST)
            how = "NEAREST x%d" % (size // w)
        else:
            frame = base.resize((size, size), Image.LANCZOS)
            how = "LANCZOS x%g (facteur non entier)" % (size / float(w))
        frames.append(frame)
        print("  %3d px  %s" % (size, how))

    # Pillow ecrit l'ICO depuis la plus grande image en derivant les autres ; on
    # lui passe explicitement chaque taille via append_images pour controler le
    # reechantillonnage nous-memes, sinon il refait le sien.
    largest = frames[-1]
    # Entrees en PNG (defaut de Pillow), et c'est un choix mesure. La convention
    # historique est BMP sous 256 px et PNG a 256 ; forcer BMP partout fait
    # passer le fichier de 7 Ko a 369 Ko, parce qu'un 256x256 BMP est stocke non
    # compresse -- 262 Ko pour une source de 32x32. Le PNG dans un .ico est
    # compris depuis Vista (2007), et sinaia exige de toute facon un Windows
    # bien plus recent. On echange une compatibilite sans objet contre un
    # cinquantieme de la taille.
    largest.save(DST, format="ICO",
                 sizes=[(s, s) for s in SIZES],
                 append_images=frames[:-1])
    print("ecrit : %s  (%d octets, %d tailles)"
          % (os.path.relpath(DST, HERE), os.path.getsize(DST), len(SIZES)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

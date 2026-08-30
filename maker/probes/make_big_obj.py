#!/usr/bin/env python3
"""Genere un OBJ de N triangles -- l'entree de la mesure d'empreinte (7.5).

Le catalogue de cggraph_nodes n'a pas de noeud generateur : sa seule source de
maillage est mesh.io.load. Mesurer l'empreinte sur "2 M de triangles" demande
donc un FICHIER de 2 M de triangles, et le voici.

Grille torique fermee : rows x cols quadrangles, chacun coupe en deux
triangles, donc 2 * rows * cols triangles pour rows * cols sommets. Fermee pour
qu'aucun sommet ne soit de bord -- le lissage laplacien preserve le bord sans
condition (caveat du catalogue), et une grille ouverte ferait mesurer une
enveloppe figee.

  python maker/probes/make_big_obj.py --triangles 2000000 --out .../big.obj
"""
import argparse
import math
import os


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--triangles", type=int, default=2_000_000)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    side = max(2, int(round(math.sqrt(args.triangles / 2))))
    rows = cols = side
    nv = rows * cols
    nf = 2 * rows * cols

    R, r = 3.0, 1.0
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", newline="\n") as f:
        f.write(f"# grille torique {rows}x{cols} -- {nv} sommets, {nf} triangles\n")
        chunk = []
        for i in range(rows):
            u = 2.0 * math.pi * i / rows
            cu, su = math.cos(u), math.sin(u)
            for j in range(cols):
                v = 2.0 * math.pi * j / cols
                cv, sv = math.cos(v), math.sin(v)
                x = (R + r * cv) * cu
                y = (R + r * cv) * su
                z = r * sv
                chunk.append(f"v {x:.5f} {y:.5f} {z:.5f}\n")
            if len(chunk) > 200000:
                f.writelines(chunk)
                chunk = []
        f.writelines(chunk)

        chunk = []
        for i in range(rows):
            i2 = (i + 1) % rows
            base, base2 = i * cols, i2 * cols
            for j in range(cols):
                j2 = (j + 1) % cols
                a = base + j + 1
                b = base + j2 + 1
                c = base2 + j2 + 1
                d = base2 + j + 1
                chunk.append(f"f {a} {b} {c}\nf {a} {c} {d}\n")
            if len(chunk) > 200000:
                f.writelines(chunk)
                chunk = []
        f.writelines(chunk)

    size = os.path.getsize(args.out)
    print(f"{args.out}  {nv} sommets  {nf} triangles  {size} octets")


if __name__ == "__main__":
    main()

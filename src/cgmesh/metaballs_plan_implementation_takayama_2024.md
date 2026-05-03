# Plan d'implémentation par étapes — Takayama 2024 (Tracery Designer)

## Préambule

Ce plan est reconstruit à partir du poster SIGGRAPH Asia 2024 de Joe Takayama et de la version étendue 2025 (Springer). Le code original n'étant pas public, certaines formulations mathématiques (notamment l'atténuation angulaire et l'union *stepwise*) sont des choix d'implémentation raisonnables qui peuvent diverger des choix exacts de l'auteur. Compter une à deux semaines pour un développeur familier avec WebGL et p5.js.

## Architecture générale

L'auteur a utilisé **p5.js + GLSL**. La logique : un fragment shader calcule le champ de densité (un pixel = un calcul de densité), tandis que p5.js gère l'UI, le placement des motifs et l'export. Suivre la même séparation : shader pour le calcul lourd, JavaScript pour l'orchestration.

## Étape 0 — Setup

p5.js en mode WEBGL pour avoir accès aux shaders. Un seul `createShader(vert, frag)` au démarrage. Quelques sliders pour les paramètres initiaux (threshold, rayon). À ce stade, le shader retourne juste une couleur unie : on vérifie que le pipeline marche.

## Étape 1 — Metaball unique, formulation classique

Dans le fragment shader, pour chaque pixel P et un centre C de rayon R :

```
d(P) = R² / |P - C|²
```

Couleur blanche si `d(P) > seuil`, noire sinon.

**Validation** : le résultat ressemble à un disque dont la taille varie continûment avec le seuil. C'est la fondation.

## Étape 2 — Metaballs multiples, somme classique

Ajouter un tableau de centres passés en uniform. Le shader somme les densités :

```
d_total(P) = Σ R_i² / |P - C_i|²
```

**Validation** : deux metaballs proches fusionnent en gouttelette de mercure (la signature visuelle des metaballs classiques).

## Étape 3 — Approche *stepwise* (point clé du papier)

C'est la nouveauté méthodologique de Takayama. Au lieu de sommer toutes les densités d'un coup, on les ajoute une par une et on prend l'iso-contour à chaque étape. Le résultat préserve partiellement l'apparence de chaque metaball individuel (Fig. 2b du poster).

Concrètement : *ping-pong rendering*. Un framebuffer A contient l'état actuel, on rend dans B en faisant `union(A, nouvelle_metaball_iso)`, on échange A et B, et ainsi de suite. L'union peut être binaire ou avec un blend doux sur les bords pour garder un aspect lisse.

**Validation** : avec 4 metaballs en ligne, on doit voir 4 lobes distincts reliés, au lieu d'une grosse gouttelette unique.

## Étape 4 — Foils par atténuation angulaire

LE motif central du gothique (trèfle, quadrilobe, cinquefoil). Pour une metaball centrée en C, on calcule l'angle :

```
θ = atan2(P.y - C.y, P.x - C.x)
```

On module la densité par une fonction qui s'annule à intervalles angulaires réguliers. Une formulation simple :

```
attenuation(θ, N) = max(0, cos(N·θ/2))^k
d(P) *= attenuation(θ, N)
```

N est le nombre de lobes (3 pour trèfle, 4 pour quadrilobe), k contrôle la finesse de l'atténuation.

**Validation** : N=3 donne un trèfle, N=4 un quadrilobe, N=5 un cinquefoil. La forme doit varier continûment quand on interpole N à des valeurs fractionnaires.

## Étape 5 — Distributions polygonales

Le poster mentionne que la méthode marche aussi avec des polygones réguliers comme distribution de base. Remplacer `|P-C|²` par une SDF de polygone (triangle, carré, hexagone) — formules classiques disponibles sur iquilezles.org/articles/distfunctions2d/. Combiner avec l'atténuation angulaire de l'étape 4.

**Validation** : on retrouve des motifs gothiques inscrits dans des polygones, typiques du remplage tardif.

## Étape 6 — Cusps (pointes)

Les cusps sont les pointes acérées entre les lobes des foils. Elles émergent naturellement quand l'atténuation est plus marquée (k plus grand dans la formule de l'étape 4). Ajouter un paramètre dédié et vérifier qu'on obtient les pointes caractéristiques du gothique flamboyant.

## Étape 7 — Mouchettes / daggers par mouvement

Les mouchettes (motifs en forme de poignard, signature du flamboyant) sont obtenues en suivant la trajectoire d'une metaball en mouvement. Définir une courbe (Bézier ou arc), échantillonner N positions le long, faire l'union pas à pas (étape 3) → la forme tracée a un profil effilé.

**Validation** : comparer visuellement avec des photos de mouchettes du XVe siècle (fenêtres de la Sainte-Chapelle par exemple).

## Étape 8 — Composition dans une structure parente

Construire un arc en tiers-point (*lancet arch*) classique : deux cercles tangents aux montants se coupent au sommet. Placer les motifs des étapes précédentes à l'intérieur. Implémenter les transformations annoncées dans le poster : duplication, miroir, symétrie radiale.

## Étape 9 — Interpolation entre patterns

Deux états A et B (positions de metaballs, paramètres N, k, etc.). Interpoler linéairement pour des frames intermédiaires. C'est ce qui permet l'animation *shape-shifting* du papier.

## Étape 10 — Export en heightmap

Rendre le champ de densité (avant seuil) dans un framebuffer offscreen, mapper sur [0, 255] en niveaux de gris, exporter en PNG. Cette image sert ensuite de displacement map dans Blender ou Substance pour fabriquer un relief 3D.

## Validation globale

À la fin, l'outil doit reproduire les figures du poster : trèfles, quadrilobes, mouchettes, cusps, motifs composés dans un arc, interpolation animée.

## Pièges connus

L'approche *stepwise* (étape 3) est la partie technique la plus délicate. Si elle ne fonctionne pas immédiatement en ping-pong, livrer d'abord une version "somme classique" et y revenir.

L'atténuation angulaire (étape 4) a des discontinuités à θ = ±π. Soit utiliser `atan2` et accepter la coupure (le shader la gère bien), soit passer par cos/sin pour rester continu.

GLSL ES (utilisé par WebGL 1) limite le nombre d'uniforms. Pour beaucoup de metaballs, passer les positions via une texture plutôt qu'un tableau d'uniforms.

## Découpage pratique en livrables

| Sprint | Étapes | Livrable |
|---|---|---|
| 1 | 0–2 | Metaballs classiques fonctionnels |
| 2 | 3 | Stepwise opérationnel (cœur méthodologique) |
| 3 | 4–6 | Foils, polygones, cusps |
| 4 | 7–8 | Mouchettes et composition dans un arc |
| 5 | 9–10 | Animation et export heightmap |

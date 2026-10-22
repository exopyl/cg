# État de l'art — Modélisation procédurale et paramétrique de l'architecture gothique

## Axe 1 — Procédural / grammatical (la lignée historique)

**Havemann & Fellner (2004)** — *Generative Parametric Design of Gothic Window Tracery* (VAST/SMI 2004). Le papier fondateur, basé sur le **Generative Modeling Language (GML)**, langage interprété à pile inspiré de PostScript. Pas de code accessible aujourd'hui ; le runtime distribué via `generative-modeling.org` n'est plus maintenu activement.

**Takayama (2013)** — *Computer-generated Gothic Tracery with a Motif-oriented Approach* (IASDR 2013). Approche par placement automatique de motifs (trèfles, quadrilobes, mouchettes) dans un espace fermé délimité par un arc. PDF en libre accès, pas de code public.

**Takayama (2024)** — *Tracery Designer: A Metaball-Based Interactive Design Tool for Gothic Ornaments* (SIGGRAPH Asia 2024 Posters). Évolution du précédent : approximation des motifs par des metaballs avec atténuation angulaire de la densité, ce qui permet une déformation continue impossible avec des arcs. Implémenté en **p5.js + GLSL**. Pas de code source publié, mais la stack est standard et le poster décrit la méthode de manière suffisamment précise pour réimplémenter.

**Takayama (2025)** — *Metaball-Based Design Support Tool for Dynamically Deformable Gothic Ornamentation* (Springer). Version étendue du poster 2024.

## Axe 2 — Le papier qui change tout (2025)

**Segall, Ren, Schwarz & Sorkine-Hornung (2025)** — *Computational Modeling of Gothic Microarchitecture* (SIGGRAPH 2025). Travail conjoint de l'équipe d'Olga Sorkine-Hornung à l'ETH Zurich avec Martin Schwarz, historien d'architecture à l'Université de Bâle.

Les auteurs critiquent explicitement Havemann & Fellner pour s'appuyer sur des dessins utilisateur sans ancrage dans la méthodologie historique, et observent qu'aucune recherche comparable n'existe pour la microarchitecture gothique (orfèvrerie, reliquaires, *Goldschmiedrisse*).

**Code public** : `llorz/SIG25_goldschmiedrisse` sur GitHub, licence Creative Commons BY-NC. C'est le seul papier moderne du domaine avec code disponible — la meilleure base pour reproduire ou étendre.

## Axe 3 — Lignée historico-procédurale (Bereczki)

**Zoltán Bereczki (Université de Debrecen)**, série d'articles 2020–2025 :

- **Bereczki (2020)** — *Generative Interpretations of Late Gothic Architectural Forms*. Symmetry: Culture and Science.
- **Bereczki (2022)** — *Order, Procedure, and Configuration in Gothic Architecture: A Case Study of the Avas Church, Miskolc, Hungary*. MDPI Architecture (open access).
- **Bereczki (2024)** — *Construction of a Gothic Church Tower: A 3D Visualisation Based on Drawn Sources and Contemporary Artefacts*. MDPI Heritage (open access).
- **Bereczki (2025)** — *The Procedural Nature of Gothic Architecture and Microarchitecture*. Open access sur Zenodo (DOI 10.33542/CAH2024-2-03).

Bereczki développe la méthode *Simulated Morphogenesis* qui relie les sources médiévales (notamment le *Fialenbüchlein* de Mathes Roriczer, 1486) aux techniques computationnelles contemporaines. Pour la reconstruction du vault de l'église Avas à Miskolc, les parties existantes servent de constantes et les analogies historiques fournissent les bornes des variables aléatoires. Pas de code publié, mais les méthodes sont décrites pas à pas et reproductibles en Grasshopper ou Python.

## Axe 4 — Contexte théorique plus large

### Shape grammars

Le cadre formel qui sous-tend toute la lignée procédurale, à connaître pour situer le sujet :

- **Stiny & Mitchell (1978)** — *The Palladian Grammar*. Modèle classique transposable.
- **Economou, Hong & Newton (2024)** — *Shape meets Euclid: Integrating shape computation with ruler and compass procedures* (Automation in Construction). Très pertinent : s'attaque précisément aux constructions à la règle et au compas dans les interpréteurs de grammaires de formes — exactement le problème du gothique.

### Modélisation procédurale architecturale moderne

- **Wonka et al. (2003)** — *Instant Architecture*. SIGGRAPH.
- **Müller et al. (2006)** — *Procedural Modeling of Buildings*. SIGGRAPH. A donné naissance à CGA Shape (Esri CityEngine).
- **Schwarz & Wonka (2015)** — *Practical Grammar-Based Procedural Modeling of Architecture*. SIGGRAPH Asia Courses. Excellent cours de synthèse.

## Axe 5 — Sources historiques et méthodologiques

**Robert Bork** — référence universitaire sur la géométrie de conception gothique :

- *The Geometry of Creation* (2011)
- *Late Gothic Architecture* (2018)
- *Geometry as a Resource for Renaissance Architecture* (2023)

Bork formule la phrase qui résume parfaitement pourquoi le gothique se prête si bien au procédural : *« Gothic design conventions governed the rules of the process more than the shape of the final product. »*

**Nick Webb & Alexandrina Buchanan** (Université de Liverpool) — reconstruction des méthodes médiévales à la règle et au compas.

**Andrew Tallon** (†2018) — pionnier du laser-scan appliqué aux cathédrales (Notre-Dame, Bourges). Données archivées au Vassar College, parfois utilisables.

**Mathes Roriczer (1486)** — *Büchlein von der Fialen Gerechtigkeit* / *Fialenbüchlein*. Source primaire absolue : manuel de tailleur de pierre du XVe siècle décrivant pas à pas la construction d'un pinacle gothique. Domaine public. Édition canonique : **Shelby (1977)** — *Gothic Design Techniques*. C'est la source dont s'inspire directement Bereczki.

## Recommandation finale — Base de travail

Si l'objectif est de **reproduire et étendre** un article du domaine, voici l'ordre conseillé :

**1. Cloner et faire tourner `llorz/SIG25_goldschmiedrisse`.** C'est la base de code la plus moderne, propre, et alignée avec la recherche actuelle. Lire le papier Segall et al. (2025) en parallèle. Point de départ technique solide, équipe (Segall & Ren à l'ETH) qui répond aux questions par mail d'après le README.

**2. Lire Bereczki (2025) en open access** et confronter sa méthodologie *Simulated Morphogenesis* à celle de Segall et al. Les deux approches sont complémentaires : Bereczki apporte la rigueur historico-architecturale, Segall apporte le cadre computationnel SIGGRAPH.

**3. Pour le 2D pur (remplages, pinacles à plat), réimplémenter Takayama 2024 en p5.js.** Le poster suffit à reconstruire la méthode metaballs avec atténuation angulaire. Projet de quelques jours qui aboutit à un outil interactif fonctionnel.

**4. Garder Roriczer (édition Shelby 1977) comme source primaire** pour valider la fidélité historique des constructions — c'est ce qui distingue les bons travaux de Bereczki et Segall des modélisations purement « cosmétiques ».

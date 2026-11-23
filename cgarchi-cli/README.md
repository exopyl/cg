# cgarchi-cli et serveur MCP `cgarchi`

Accès en ligne de commande, et pour un assistant via MCP, aux colonnes des cinq
ordres de Vignole que porte `cgarchi`, et aux données historiques embarquées
(`src/cgarchi/data/cinq_ordres.json`, `temple_hephaistos.json`).

## Construction

```
cmake -S . -B build/cgarchi-cli -G "Visual Studio 18 2026" -A x64 -DENABLE_CGARCHI_CLI=ON -DENABLE_SINAIA=OFF <EXTERN_*>
cmake --build build/cgarchi-cli --config Release --target cgarchi-cli
```

L'exécutable est `build/cgarchi-cli/cgarchi-cli/Release/cgarchi-cli.exe` ; les
DLL qu'importe cgmesh (OpenNURBS, lib3mf, OCCT) sont copiées à côté.

## Ligne de commande

| Commande | Effet |
|---|---|
| `defaults [--order <ordre>]` | Réglages par défaut, document `classical-column/v1` |
| `column --settings <fichier\|-> --out <chemin>` | Colonne en `.obj`, `.glb` ou `.stl`, plus ses cotes |
| `colonnade --settings <fichier\|-> --out <chemin> [--columns N] [--no-ornaments]` | Colonnade sous entablement filé |
| `levels --settings <fichier\|->` | Cotes z sans maillage |
| `data orders\|hephaistos` | JSON embarqué, tel quel |

Les réglages suivent `src/cgarchi/schemas/classical-column-v1.schema`. Toute
sortie (sauf `defaults` et `data`) est un JSON `{"ok": true, ...}` ou
`{"ok": false, "error": ..., "report": ...}`. Codes de retour : 0 succès, 1 refus
ou échec, 2 ligne de commande invalide.

Unités : `module` est le demi-diamètre inférieur du fût en unités de scène.
L'OBJ porte les coordonnées brutes, Z vers le haut. Le GLB de cgmesh suppose des
millimètres (nœud à l'échelle 0,001, Y vers le haut) : pour une scène en mètres,
préférer l'OBJ. Import Blender :
`bpy.ops.wm.obj_import(filepath=..., forward_axis='Y', up_axis='Z')`.

## Serveur MCP

`mcp/cgarchi_mcp.py`, bibliothèque standard Python seule (transport stdio,
JSON-RPC 2.0). Déclaré pour Claude Code dans `.mcp.json` à la racine du dépôt.
Il trouve l'exécutable par la variable `CGARCHI_CLI`, sinon dans
`build/cgarchi-cli/cgarchi-cli/<config>/`.

| Outil | Effet |
|---|---|
| `list_orders` | Ordres, proportions générales, éditions de référence |
| `get_data` | Sous-arbre des données par chemin pointé (`ordres.corinthien.blocs.chapiteau`) ; aperçu si trop gros |
| `build_column` | Colonne écrite sur disque, cotes en retour |
| `build_colonnade` | Colonnade de 1 à 12 colonnes |
| `column_levels` | Cotes seules |

Hors périmètre de cgarchi : acanthe, caulicoles, fleurons, volutes composites
(rendus par leur noyau ou leur enveloppe).

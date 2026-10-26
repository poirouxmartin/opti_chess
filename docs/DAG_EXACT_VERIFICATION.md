# Vérification du DAG exact sur main (2026-10-26)

Demande de Martin (via Octave) : 1) le travail DAG exact est-il entièrement dans `main` ?
2) tout fonctionne-t-il sur `main` ? 3) les positions cibles du chantier sont-elles résolues ?
Chaque affirmation est étiquetée **mesuré** ou **lu** (dans git ou le code).

## 1. Intégration dans main

- `main` local = `bc68977` « Merge dev/poiroux/dag-exact » (Martin, 2026-10-25 20:04).
  Ses parents sont `edc633d` (= `origin/main`) et `4cc9de5` (tête de `dev/poiroux/dag-exact`) — **lu**.
- `git log main..dev/poiroux/dag-exact` est vide : tous les commits de la branche sont dans
  `main` — **lu**.
- `git diff dev/poiroux/dag-exact main` ne touche que `README.md` (3+/2−). C'est exactement le
  changement d'`origin/main` depuis la base commune `0d18d42` (`edc633d`, docs README). Aucun
  fichier de code ne diffère : la fusion n'a rien perdu ni ajouté — **lu**.
- `git diff --stat origin/main main` = les 14 fichiers de la branche (1320+/93−), rien d'autre —
  **lu**.
- Le dépôt distant n'a rien de tout cela : `main` a 23 commits d'avance sur `origin/main` et 0 de
  retard. Les 5 derniers commits (`7e2a713`, `b6bb662`, `1d15ce4`, `8b6dfdb`, `4cc9de5`) ne sont
  sur aucune branche distante. Les commits plus anciens de la série (`6a72b92`…) sont sur
  `origin/dev/poiroux/fine70-exact-dag` — **lu**, après `git fetch`. Rien n'a été poussé.

## 2. Build et tests sur main (`bc68977`)

Build Release complet (`cmake --build … --clean-first`, tests + GUI + CLI) — **mesuré** :

| Étape | Résultat |
|---|---|
| Build | exit 0. Exe datés du 2026-10-26, 22:01:30 (tests), 22:01:54 (GUI) et 22:01:59 (CLI) |
| Perft suite full | `RESULT: 7/7 OK` (597 236 237 nœuds, 75,2 Mnps) |
| gtest (filtre CI du harness) | `[  PASSED  ] 156 tests.` sur 156 lancés, 0 échec (605 s) |
| dont `Endgame.Fine70ExactDagSolvesLadder` | OK. S4 e3f2 (12 500 it), S5 d3e3 (5 500), S6 d2d3 (6 000), S7 a1b1 (12 000) |
| dont `Endgame.Fine70LadderEasySteps` | OK |
| UCI | `bestmove d2d4`, `bestmove a5b4` (pas d'en passant illégal e5d6) : OK |
| `bench 2000` | 701 226 nœuds, signature `29ae53c1996ef1e6`. Identique à la branche le 2026-10-22 : même comportement de recherche. 58 789 nœuds/s (mesure seule, sans A/B) |
| `Puzzle.OFFvsDAG_Endgame` (exclu du harness, lancé à part) | OK, voir §3 |

Le binaire contient 196 tests, dont 40 sont exclus par le filtre CI : benchmarks longs
(`LichessBenchmark*`, `*TimeBenchmark*`, `*ParamSweep*`…), `*Debug*` et `*Perf*`. Ils n'ont pas
été relancés, hormis `OFFvsDAG_Endgame`. Le bench puzzles 5000 a été mesuré le 2026-10-22 sur la
branche (4024/5000, `BENCHMARKS.md`) ; la signature identique indique un comportement identique.

## 3. Positions cibles

Les cibles du chantier sont l'échelle de Fine #70 (Lasker-Reichhelm 1901, `Tests.txt:1822-1825`,
`901-904`) et les nulles par répétition du correctif du 2026-10-15 (`BENCHMARKS.md:240-246`).
Elles sont rejouées sur `main` avec le même binaire, en deux modes : DAG (défaut, DAG exact) et
arbre pur (`OPTI_NO_DAG=1`, l'avant-DAG). « Ancien DAG » renvoie aux mesures du 2026-10-20 avec la
même commande (`BENCHMARKS.md:394-401`, colonne « plain »). Il n'existe plus de binaire de
l'ancien DAG.

Commande : `opti_chess_cli solve 100000 <coups attendus> 50 <FEN>`. La position est **résolue**
si le coup le plus exploré est attendu, avec une valeur ≥ +50, à partir d'un bloc de 500
itérations et **jusqu'à 100 000**. Le temps est celui du premier bloc résolu. Entre parenthèses :
la valeur finale en centipions, du point de vue du trait — **mesuré**.

| Étape | FEN | Attendu | DAG exact (main) | Arbre (main, OPTI_NO_DAG) | Ancien DAG (2026-10-20) |
|---|---|---|---|---|---|
| S1 | `8/8/3p1k2/p2P1p2/P2P1P1K/8/8/8 w - - 20 11` | Kh5 | **500 it, 20 ms** (1866) | 500 it, 12 ms (652) | 500 it, 16 ms (485) |
| S2 | `8/4k3/3p4/p2P1p2/P2P1P2/6K1/8/8 w - - 18 10` | Kh4 | **1 000 it, 27 ms** (1777) | 1 500 it, 34 ms (479) | 1 000 it, 30 ms (1307) |
| S4 | `8/2k5/3p4/p2P1p2/P2P1P2/4K3/8/8 w - - 14 8` | Kf2/Kf3 | **non résolu** : correct de 9 000 à 95 000 it, puis Kd3 à 95 500 (1751) | non résolu, Ke2 (136) | non résolu, Kd3 (351) |
| S5 | `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` | Ke3/Ke2 | **6 000 it, 381 ms** (Ke2 1856) | 11 000 it, 283 ms (Ke3 169) | non résolu, Kd2 (336) |
| S6 | `8/2k5/3p4/p2P1p2/P2P1P2/8/3K4/8 w - - 10 6` | Kd3 | **5 000 it, 312 ms** (1296) | non résolu, Kc1 (190) | non résolu, Ke1 (393) |
| S7 (Fine #70) | `8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1` | Kb1 | **12 500 it, 1 492 ms** (1079) | non résolu, Ka2 (186) | non résolu, Kb2 (241) |

Bilan : le DAG exact résout 5 positions sur 6, l'arbre 3 sur 6 (S1, S2, S5) et l'ancien DAG 2 sur
6. La position de départ de Fine #70 (S7, seul coup gagnant 1.Kb1) n'est résolue que par le DAG
exact.

**S4.** Au rejeu de main, S4 est non résolu au sens strict. Le 2026-10-20, la version sous flag
`OPTI_DAG_EXACT=1` le résolvait (9 500 it, 643 ms, tenu jusqu'à 100 000).

Trace de S4 sur main (`OPTI_SOLVE_TRACE`), changements de coup — **mesuré** :

| Itération | Coup | Valeur |
|---|---|---|
| 500 | Kd2 | 136 |
| 1 000 | Ke2 | 97 |
| 9 000 | Kf2 | 486 |
| 19 500 | Kf3 | 1347 |
| 34 500 | Kf2 | 1698 |
| 95 500 | Kd3 | 1751 |

Le test gtest (budget 20 000) le résout à 12 500. Deux causes sont possibles, sans vérification
(**hypothèse**) :
- un effet des changements de `1d15ce4`, à savoir le recalcul du WDL et de `avg_score` ou la clé
  avec horloge ;
- l'arène, presque pleine à la fin (398 418 plateaux sur 500 000).

On ne sait pas non plus si Kd3 perd vraiment : la valeur DAG est de +1751. Aucun oracle n'a été
lancé.

**Coût en temps à 100 000 itérations** (mesuré) : 21 à 38 s en DAG contre 3 à 5 s en arbre sur
ces 6 positions. Le DAG garde beaucoup plus de plateaux : 325 000 à 420 000 contre environ
100 000. Le temps pour atteindre la solution reste court (≤ 1,5 s).

### Échelle chronométrée `Puzzle.OFFvsDAG_Endgame`

Le test compte 5 positions × 3 budgets, soit 15 essais par mode. Les budgets sont en itérations :
« 0,1 s » vaut 5 000 it, « 1 s » vaut 50 000 et « 5 s » vaut 250 000 — **mesuré** :

- DAG exact : **9/15** (0,1 s : 2/5 ; 1 s : 3/5 ; 5 s : 4/5).
- Arbre : **4/15**.
- Repère de l'ancien DAG : 2/15 (2026-10-22).

Mêmes chiffres que sur la branche le 2026-10-22.

Détail par position :

| Position | 0,1 s | 1 s | 5 s |
|---|---|---|---|
| Kb1 | DAG ✓, arbre ✗ (Kb2) | les deux ✓ | les deux ✓ |
| Kd3 | les deux ✗ | DAG ✓, arbre ✗ (Kc1) | DAG ✓, arbre ✗ (Kc1) |
| Ke3 | DAG ✓, arbre ✗ (Kd2) | les deux ✓ | les deux ✓ |
| Kf3 | les deux ✗ | les deux ✗ (DAG Kf2, pourtant gagnant) | DAG ✓, arbre ✗ (Ke2) |
| « White Kf3 » | les deux jouent Kh4 | les deux jouent Kh4 | les deux jouent Kh4 |

Trois défauts du test lui-même (**lu**, `unit_test.cpp:7061-7069`) :
- Pour « White Kf3 », le test attend Kf3, mais l'échelle documentée attend Kh4 (S2,
  `BENCHMARKS.md:323`). Les deux modes jouent Kh4 : le test les compte faux à tort.
- Pour Kf3, seul Kf3 est accepté, alors que Kf2 gagne aussi.
- Deux entrées sont ignorées faute de coup résolvable : « Black Kd4 » et « Rh5 ».

Le test n'a aucune assertion sur le résultat.

### Nulles par répétition

Commande : `opti_chess_cli search 100000 <FEN>`, valeur côté Blancs à 100 000 itérations.
Attendu : 0 — **mesuré**.

| Position | DAG exact | Arbre | Avant le correctif du 2026-10-15 (DAG) |
|---|---|---|---|
| `6k1/8/7P/7K/8/8/8/8 w - - 3 72` | 0 (Kg6), 15,7 s | 0, 6,3 s | +612 |
| `8/8/8/7k/8/qp6/p7/K5R1 w - - 0 1` (tour desperado) | 0 (Tg5), 23,0 s | 0, 9,7 s | 0 |
| `rn2k1r1/pppRpRpp/bb6/q7/6n1/8/PPPPPPPP/1NB1KBN1 w - - 0 1` (échec perpétuel) | 0 (Tfe7), 74,7 s | 0, 67,6 s | 0 |

## Ce qui reste

- **S4** n'est plus tenu jusqu'à 100 000 itérations sur `main` (bascule à 95 500). À comprendre
  avant toute affirmation « échelle entièrement résolue ».
- **Test `OFFvsDAG_Endgame`** : expectation fausse (Kf3 au lieu de Kh4), Kf2 non accepté, aucune
  assertion.
- **Vitesse** : 58 789 nœuds/s ici, contre 61 613 avant le changement le 2026-10-22. Ce sont des
  mesures seules, non entrelacées. L'A/B entrelacé (`scripts/perf_compare.ps1`) n'est toujours pas
  fait.
- **Push** : `main` local a 23 commits non poussés (24 avec ce rapport). La décision revient à Martin.

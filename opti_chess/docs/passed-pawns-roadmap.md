# Pions passés — formalisation + roadmap Grogros

Document de travail (papiers de Martin formalisés + roadmap priorisée).
Conventions : perspective Blancs (symétrique Noirs), valeurs en cp.

---

## PARTIE A — Modèle du pion passé

### A.1 Valeur par case `V(s)`

Soit un pion passé en `s0`, cases devant lui `s1..sk` (jusqu'à promotion).
Contrôles ennemis `C(s)` (avec valeur des pièces), contrôles alliés `F(s)`.

```
V(s) = V_base(rang)                       si aucun contrôle ennemi
V(s) = min(V_base,  min_{p ∈ C(s)} val(p)) si contrôles ennemis
```

- `V_base(rang)` : table par rangée, dans [400..800] (à calibrer : ex.
  2e→400, 3e→450, 4e→550, 5e→650, 6e→750, 7e→800).
- Logique du min : le bloqueur le moins cher fixe le plafond (on pourra
  toujours le sacrifier contre le pion à minima).
- Si contrôles ennemis > contrôles alliés sur `s` : division par pièce
  (mécanisme actuel conservé).
- Si un pion allié contrôle `s` : **la majoration saute** (V_base s'applique).

### A.2 Blocage (pièce sur la route)

Pion passé faible, **mais la pièce qui bloque est pénalisée aussi** :

```
V_passé_bloqué = V_base × petit-facteur(bloqueur)
malus_bloqueur = f(pièce)   // cavalier = excellent bloqueur (gros malus
                            // pour le camp adverse = le laisser vaut cher),
                            // dame = très mauvais bloqueur (elle ne doit
                            // pas babysitter : malus dissuasif sur la dame)
```

Autrement dit : le bloqueur idéal (cavalier) coûte peu à son camp pour un
gros effet ; y coller sa dame est puni. Le malus dépend de la pièce qui
bloque, pas du pion.

### A.3 Pions passés protégés / candidats

- **Protégé** (pion allié contrôle sa case ou la case devant) : pas de
  majoration (A.1), bonus de connexion standard.
- **Candidat** (à définir, ouvert) : pion qui DEVIENDRA passé si la majorité
  avance ou si la minorité force l'échange. Piste : bonus = V_base × 0.3 si
  la majorité de l'aile peut forcer sa création (comptage effectifs sur les
  colonnes adjacentes), 0 sinon. À trancher ensemble.

### A.4 Valeur globale : V0..V5

Question : `V = Vmin`, ou `V = Vmin − k·dist` ?

```
V = max_i ( Vi − k·i ),   k ≈ 75 (tunable)
```

- `Vmin` pur n'a **aucun gradient** : rien n'incite à avancer le pion.
- `Vmin − k·dist` crée l'incitation (chaque pas rapproche du max) tout en
  gardant le maillon faible comme plafond. Recommandé.
- Cas multi-cases protégées : on prend le **min** (weakest link).

### A.5 Chaînes de pions

- **Chaîne en A** (pointe vers l'ennemi, base derrière) : bout fort,
  **2 faiblesses** (trous de part et d'autre de la base).
  `score = +bonus_pointe − 2 × malus_trou_flanc`
- **Chaîne en V** (pointe d'un côté, éventail derrière) : **1 grosse
  faiblesse centrale**, mais si elle pète, **toute la chaîne casse des
  deux côtés** (facteur fragilité : si la pointe est contestée, toute la
  chaîne se dévalue).
  `score = +bonus_fer + fragilité × malus_central`
- **Règle du côté de jeu** : le jeu se produit où la chaîne pointe.
  Formalisation proposée : vecteur base→pointe (ou shift du centre de
  masse des pions), `wing_focus` = projection sur l'aile ; bonus
  `wing_focus × (espace + activité)` de ce côté, symétrique à tester
  sur la banque (corréler wing_focus avec le gap par aile).

### A.6 Majorités / minorités

C'est souvent ce qui décide la finale : **pousser sa majorité** (ça crée
un passé — cf. candidats A.3). Formalisation : score de majorité par aile
(effectifs pondérés par avancement) → bonus candidat (A.3) + bonus
espace du côté majoritaire. Minorité : levier d'échange créant un passé
(attaque de minorité) — même machinerie, signe inversé sur l'aile faible.

---

## PARTIE B — Roadmap Grogros (priorisée)

Légende : [FAIT] [EN COURS] [PROCHAIN] [PLUS TARD] [EXPÉRIMENTAL].
Règle : chaque changement = benchmark avant/après (éval + NODES + McNemar).

### P0 — En cours
- [EN COURS] **Pions passés** (ce document) : tempo + contrôles du carré,
  extension finales mineures, puis A.1→A.6 par morceaux benchmarkés.

### P1 — Méthode (infra déjà en place, à exploiter)
- [FAIT] Banques quiet + labels SF statique/dynamique, attribution par gap,
  screen de quiétude, tuning sans rebuild (`OPTI_KS_*`, `OPTI_MOB_RELIEF`).
- [PROCHAIN] Brancher chaque candidat A sur cette infra (slice flips pions,
  best-slice garde-fou, NODES-500 + McNemar).

### P2 — Recherche : sélectivité type Lc0
- [PROCHAIN] Énorme profondeur + sélection critique : à profondeur donnée,
  ne pas regarder TOUS les coups ; le coup principal à ~90%. Levier force
  le plus gros, mais design d'abord (benchmarks micro par testcase).
- [PROCHAIN] Killer bias + détection de menace dans l'ordonnancement.
- [PROCHAIN] Early quiescence cutoff non arbitraire : pré-éval ("−1500 →
  cut direct") plutôt que rang ; `q_depth` main suffisant.
- [PROCHAIN] Meilleur tri + prior (policy), testcases micro.

### P3 — Transpositions + exactitude
- [PROCHAIN] Audit transpositions : tout remettre propre + micro-benchmarks
  (prouver que ça marche, pas juste que ça ne crash pas).
- [PROCHAIN] Parallélisation : à réfléchir de nouveau (résultats mitigés).
- [PROCHAIN] `game_over()` à opti (déjà mémoïsé, reste du jus ?).

### P4 — Éval : finales + conditions de victoire
- [PROCHAIN] Endgame : proximité des rois dépendant des passés, waiting
  moves roi+pions, win conditions (potentiel de mat + passés).
- [PROCHAIN] Contre-jeu = incertitude+ (échecs, attaques, déséquilibres,
  activité) ; rebalance position ouverte (activité++/KS++) vs fermée
  (positionnement++/draw++) — à benchmarker et fine-tuner.
- [PROCHAIN] Overextension / V-structure (rien n'existe, TODO posé).

### P5 — Micro-perf (lot, quand le fonctionnel le demande)
Cache L1>L2>L3 (structures compactes), `if` prévisibles, flags compilateur,
`noexcept` partout, `+ > - > x > /` (opérations rapides), `constexpr` →
`consteval`/`constinit`, `const&`, robin_hash partout, magic bitboards
PARTOUT + BB dans l'éval, `get_moves` (séparé blancs/noirs) + sous-fonctions,
buffers coups/nœuds (nœuds : fait), structures compactes, réutilisation de
variables entre paramètres d'éval, évaluation incrémentale, `position_history`
en robin_hash (plus de vecteurs), cognitive complexity (warnings), `game_over`
opti, `opti get_moves`, `opti eval`, profiling global (hotpath surtout),
benchmarks eval/get_moves/sort/autres. Boost : à évaluer (pas de dépendance
sans preuve). UI async : fait (? à vérifier).

### P6 — Incertitude et long terme
- [PLUS TARD] Incertitude++ si position complexe en calcul — à évaluer.
- [EXPÉRIMENTAL] Rollout/simulation pour le long terme.
- [EXPÉRIMENTAL] Réseau de compensation : f(P) = delta(profonde − statique),
  entraîné sur lui-même pour prédire en statique son évaluation profonde.

### P7 — Tests (continu)
- [FAIT] Gate CI (suites rapides + GATE-500 seuil 400).
- [EN COURS] Bug Fegatello (quiescence aveugle aux compensations à 4 plis).
- Étendre : tout nouveau mécanisme = test de non-régression dédié.


#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "gui.h"
#include "windows_tests.h"


// *** Répertoire git ***
// Documents/Info/Echecs/opti_chess/c++_git


/* TODO


----- Documentation pour la suite -----

https://cs229.stanford.edu/proj2012/DeSa-ClassifyingChessPositions.pdf
https://www.wikiwand.com/en/Negamax
https://thesai.org/Downloads/Volume5No5/Paper_10-A_Comparative_Study_of_Game_Tree_Searching_Methods.pdf
https://www.chessprogramming.org/Evaluation_of_Pieces
https://www.chessprogramming.org/Evaluation
https://www.cs.cornell.edu/boom/2004sp/ProjectArch/Chess/algorithms.html#board
https://www.chessprogramming.org/Main_Page
https://towardsdatascience.com/building-a-chess-ai-that-learns-from-experience-5cff953b6784
https://arxiv.org/pdf/1711.08337.pdf
https://stackoverflow.com/questions/40137240/training-of-chess-evaluation-function
https://arxiv.org/pdf/2007.02130.pdf
https://www.chessprogramming.org/Move_Generation
https://www.chessprogramming.org/Checkmate
https://www.chessprogramming.org/Bishop_versus_Knight#WinningPercantages
https://www.chessprogramming.org/Sensor_Chess#MoveGeneration
https://hxim.github.io/Stockfish-Evaluation-Guide/
https://www.chessprogramming.org/Repetitions
https://en.wikipedia.org/wiki/Threefold_repetition
https://www.chessprogramming.org/Opening_Book
https://www.chessprogramming.org/Pawn_Structure
https://www.chessprogramming.org/Time_Management
https://www.chessprogramming.org/Encoding_Moves#MoveIndex
https://lichess.org/page/accuracy
http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&start=19
https://www.chessprogramming.org/UCT
https://www.codeproject.com/Articles/5313417/Worlds-Fastest-Bitboard-Chess-Movegenerator



----- Jeu -----

-> Promotions (de tous types plutôt que seulement dame)
-> Rajouter des tests de validité de FEN
-> Répétition de coups



----- Structure globale du projet -----

-> Séparer les fonctions du fichier opti_chess dans d'autres fichiers (GUI, IA...)
-> Faire du ménage dans les fonctions
-> Faire une classe pour les IA?



----- Optimisations à implémenter -----

-> Regarder quel compilateur est le plus rapide/opti
-> Algo Negamax -> copies des tableaux -> undo moves? ---> finalement plus lent?
-> Transposition tables
-> Algorithme MTD(f) <-> alphabeta with memory algorithm
-> stoi et to_string très lents?
-> Ajout de variables globales plutôt que les re définir lors des appels de fonction (valeur des pièces, positionnement...)
-> Parallélisation -> std::for_each avec policy parallel?
-> Au lieu de calculer l'évaluation à chaque coup, l'imcrémenter en fonction du coup
-> Dans le negamax, si y'a un mat, ne plus regarder les autres coups?
-> Optimiser le sort moves - l'améliorer (et l'évaluation?)
-> Remplacer des if par des &&
-> Vérifier que toutes les fonctions sont optimisées
-> Faire le triage des coups grâce aux itérations précédentes?
-> Fonction pour stocker facilement un noeud, ou savoir s'il est similaire à un autre? -> transposition tables
-> Incrémenter le game_over à chaque coup joué plutôt que de le regarder à chaque fois
-> Regarder si l'implémentation des échecs rend les calculs plus rapides
-> Ne plus jouer les échecs?
-> Copie des plateaux : tout copier? ou seulement quelques informations importantes?
-> Liste des coups légaux, et une autre liste pour les coups pseudo-légaux... pour éviter de les recalculer à chaque fois...
-> Utiliser les threads.. voir cours ProgrammationConcurrente
-> Checker SIMD code (pour optimiser)
-> Calcul de distance à un bord : simplement faire une matrice globale des distance pour chaque case, et regarder dedans -> https://www.chessprogramming.org/Center_Manhattan-Distance
-> Faut-il stocker les positions de certaines pièces (les rois par exemple), pour accélérer certains calculs?
-> Regarder dans les copies de tableau si on peut ne pas copier des choses, ou en copier plus...
-> Défense : bizarre parfois? Tours liées surestimées?


----- Intelligences artificielles -----


--- Nouveaux algorithmes ---

- Grogrofish_iterative_depth -
-> Changer Grogrosfish : depth -> temps (--> moteur d'analyse)
-> Faire une profodeur -> trier les coups du meilleur au pire, puis continuer la recherche plus loin

- Agent_new = Agent_old++ -
-> Faire un agent qui gagne toujours contre un autre : regarde tous les coups, et joue pour chacun la partie jusqu'au bout en utilisant agent_old -> puis joue les coups qui gagnent

- RE : multi_agents


--- Améliorations ---

-> Améliorer les heuristiques pour l'évaluation d'une position
-> https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
-> A ajouter :
    - Sécurité du roi (TRES IMPORTANT !) --> A améliorer (laaaargement améliorable), car là c'est pourri... comment calculer? !(pion protégeant le roi) *  pieces ennemies proches du roi = !king_safety ?  -> https://www.chessprogramming.org/King_Safety
    - Espace (dépend aussi du nombre de pièces restantes..)
    - Structures de pions (IMPORTANT) -> A améliorer
    - Diagonales ouvertes
    - Clouages
    - Cases noires/blanches -> faiblesse sur une couleur
    - Cases faibles
    - Pions arrierés/faibles
    - Initiative -> A améliorer : fort dans les positions d'attaque?
    - Fous de couleurs opposées : favorisent l'attaque, mais en finale -> draw
    - Harmonie des pièces (qui se défendent entre elles)
    - Pièces enfermées
    - Bon/Mauvais fou
    - Tours sur une même colonne qu'une dame ou un roi
    - Pions bloqués / Développement de pièces impossible
    - Fous/Paire de fou meilleurs en position ouverte (cavalier : inverse)
    - Tours liées
    - Garder matériel en position perdante?
    - Pièces défendues
    - Faiblesse sur une couleur
    - Ne pas trade les dames en déficit matériel?
    - Vis-à-vis
    - Focales
    - Cavaliers bloqueurs
    - "Mating nets" -> tables d'attaques / contrôles des pièces adverses?
    - Finales de pions : Roi dans le carré
    - Garder les tours pour faire nulle
    - Clouage infini
    - Pression sur les cases et points faibles
    - Occupation des diagonales
    - Colonnes occupées par une dame
    - Pions passés liés !
    - Tours liées
    - Outpost
    - Pawn push threat (on le pousse et ça attaque une pièce)
    - Passed block (si y'a qq chose qui bloque un pion passé)
-> Livres d'ouvertures, tables d'engame?
-> Tables de hachages, et apprentissage de l'IA? -> voir tp_jeux (UE IA/IRP)
-> Augmenter la profondeur pour les finales (GrogrosFish)
-> Système d'élo pour les tournois à fix (parfois elo négatif)
-> Améliorations pour trouver les mats les plus rapides... arrêter la recherche dans une branche finie...
-> Utiliser raylib pour le random? check la vitesse
-> Création d'une base de données contenant des positions et des évaluations? (qui se remplit au cours des parties...)
-> Allocations mémoires utilisant raylib?
-> Changer la structure de données des boards pour réduire leur taille
-> Comportements bizarres dans la scandi
-> Heuristiques : check -- r2q1rk1/1pp2pp1/p1p1b3/7p/4P3/4NP2/PPPPK1P1/RNB2Q2 w - - 3 15     (évalue comme pas ouf alors que c'est complètement gagnant)
-> Trouve pas forcément les mats les plus rapides (dès qu'il en a trouvé un, il cherche plus vraiment sur les autres coups...)
-> Mauvaises heuristiques : 4r1k1/3q1p2/pb1p2p1/1p1p4/1P3n2/5N1P/1BQN1PP1/4R1K1 b - - 1 32
-> Bitboard
-> Moves : plutôt que _moves[1000] -> *_moves + new _moves[_got_moves]. (avec les moves à 4 digits?). add_move utilise un array local (global?) de grande taille (stack) plutôt que les mettre direct dans _moves
-> TESTER : vecteur de plateaux (plutôt que un new???)
-> Compression des entier : 8 * a + b? sinon shift bit avec << (plus rapide)
-> Faire une nouvelle classe fille, pour optimiser et retirer les attributs inutiles, en fonction des algorithmes utilisés? (_moves_order est inutile si on utilise GrogrosZero)
-> Faut-il du coup utiliser des uint8_t dans les arguments de fonction? ou int est ok?
-> Dans le backpropagation de GrogrosZero... pour l'évaluation, prendre l'eval du meilleur fils seulement? ou faire une moyenne pondérée de tous les fils, avec leur policy/nodes?
-> Incrémentation de l'évaluation quand on joue les coups (make_move(keep))
-> Utiliser la librairie boost
-> Faire une liste séparée pour les legal et les pseudo-legal moves
-> Génération des coups de façon ordonnée? (captures en premier?)
-> Finir les undo
-> Mettre des options pour certaines fonctions pour ne pas faire les étapes inutiles (_last_move dans make_move??)
-> Faire une fonction pour initialiser un plateau (plutôt que le from_fen)
-> Promotions en autre chose que dame?
-> Faire que les undo gardent les calculs de GrogrosZero sur la position
-> Utiliser des shared pointers (pour qu'ils se détruisent automatiquement?)
-> Puzzle : 5kbK/1p1p1p1p/pPpPpPpP/P1P1P1P1/8/pppp4/8/1Q6 w - - 0 1, 6q1/8/4PPPP/8/1p1p1p1p/pPpPpPpP/P1P1P1P1/kBK5 b - - 0 1
-> 7K/8/7k/8/1p1p1p1p/pPpPpPpP/P1P1P1P1/8 w - - 0 1
-> Forteresses?
-> ATTENTION : quand il y'a des grosses évaluations, Grogros ne fait plus la différence dans les mauvaises évaluations... que ça soit un mauvais coup, et un coup qui donne mat en 1 (les deux coups sont à 0?) (à cause du k_add?)
-> Retirer les Agents (ou mettre des paramètres facultatifs?)
-> Fonction de king safety à améliorer grandement
-> Augmenter les poussées de pions en finale
-> Tester évaluation du parent = somme pondérée des évaluations des fils... (en fonctions des nombres de noeuds)?
-> Implémenter les triples répétitions !!
-> Ajouter une valeur de policy -> pas forcement reflechir sur les meilleurs coups, mais sur ceux qui sont les plus probables
-> Utiliser une db en ligne pour les livres d'ouverture et tables de finales
-> Faire une IA qui apprend tout seul? : update l'évaluation d'une position en fonction de la refléxion sur cette même position
-> En endgame il faut pousser !!
-> Parfois ne pas calculer les PGN dans les matches et tournois? pour aller plus vite
-> Faire les tournois avec une valeur de beta élevée et k_add faible? (pour une recherche restreinte et intuitive)
-> Générer un arbre d'ouvertures !! :DDDD
-> Evaluation des pièces : prendre en compte les pièces protégées / attaquées? Pièces prenables?
-> Certains coups restent trop sous-estimés par GrogrosZero
-> Ramener les pièces sur le roi adverse quand il est ouvert, et ne pas échanger les pièces
-> Tests : closed position (rnbqkbnr/8/p1p1p1p1/PpPpPpPp/1P1P1P1P/8/8/RNBQKBNR w KQkq - 1 13, r1bqkb1r/3nn3/p1p1p1p1/PpPpPpPp/1P1P1P1P/6N1/4B3/RNBQK2R b KQkq - 6 15)
-> Format du livre d'ouvertures : {(e4, fen, static_eval, dynamic_eval, nodes, {(e5, ...), (...), ...}), (d4, ...), ...}. où e4 = 1, 4, 3, 4
-> Pour l'utilisation du livre, re fabriquer un arbre?
-> Faire une table de hachage pour simplifier (et accélérer) la recherche des positions répétées
-> Pour l'historique des positions, on peut le reset à chaque coup de pion ou capture
-> Pour les transpositions, on peut peut-être renvoyer au même indice de plateau fils...?
-> Pour chaque plateau, générer et stocker la representation simpliste du plateau? Pour ensuite pouvoir aider les fils à comparer?
-> ATTENTION aux conversions int et float dans les calculs d'évaluations...
-> 3k3r/2p1b1pp/p1p2p2/3bp3/8/2P1BNP1/PPP2PKP/R7 w - - 3 16 -> +1.20?
-> Mettre les règles de parties nulles et mat en dehors de l'évaluation?
-> Ajouter les pièces protégées/attaquées lors de l'évaluation pour simplifier les calculs de l'IA
-> Carré du pion en finales
-> Ne comprend pas les finales de bases (du au fait qu'il répète les coups?)
-> Regarder dag chess?
-> Faire un readme pour les contrôles
-> Pour les finales : donner ce qui ne peut pas gagner (seulement une pièce...), les finales théoriques etc..
-> Est-ce normal que l'évaluation soit si instable?
-> Vérifier de partout que j'ai pas confondu les lignes et les colonnes
-> Faire une map des cases attaquées (ça peut rendre plus rapide les tests d'échecs)
-> 3q3k/2p4p/p2pB3/7P/1n1PPQP1/r1p5/8/1K1R2R1 b - - 0 2 -> Il ne faut plus regarder les coups qui donnent mat à l'adversaire...
-> Il faut accompagner les pions avec le roi
-> OpenAI propose un diviser pour reigner pour paralléliser GrogrosZero
-> Stocker les roques dans un tableau plutôt que 4 valeurs séparées
-> Est-ce plus rapide de mettre des boucles simples plutôt que double? while plutôt que for?
-> Endgame : la force des pièces dépend du potentiel des pions (pièces seules = bof)
-> Pourquoi parfois le regarde pas assez les bons coups?
-> GrogrosZero, développe tes pièces !!!
-> Ramener les pièces sur le roi pour l'attaque !! -> revoir king_safety()
-> à tester : 2kr4/2p2R2/2p5/1pq5/4P3/p1P2PP1/PP1B4/R3KB2 w Q - 0 14
-> 2k4r/2p5/2p2R2/1p6/4PB2/pPP2PP1/Pq6/2R1KB2 w - - 7 18 : c'est une nulle, et Grogros met +8. EDIT : sûrement car les perpet n'ont pas encore été implémentées
-> Refaire les game_over() de façon plus propre, et dire quand la partie est finie dans la GUI (+ son de fin)
-> Plein de calculs en double (voir appels de fonctions... is_mate()?)
-> Faire des tables d'attaque (par exemple entre roi et dame, cavalier...)
-> 6k1/5pp1/Q1p3q1/6B1/P6K/1p2r3/8/5R2 b - - 99 92
-> Refaire toute l'architecture avec les get_moves(), pour que ça prenne tout en compte (sans le faire dans l'évaluation)
-> 5rk1/p4qb1/1p1p3p/3Ppp2/PRP1P3/2N1BnPb/2Q4P/1R5K w - - 0 5 : noirs mieux, car roi très faible... 5rk1/p5b1/1p1p3p/1N1P3q/PRP1Pp2/5n1b/2Q4P/2BR3K b - - 3 8
-> r2qr1k1/ppp2pbp/1n6/3p1PPR/5P1Q/8/PpP1N1B1/1K1R4 w - - 2 22 : roi noir très faible, Grogros pense que les noirs sont mieux, alors que ça devrait être +15
-> Faire le compte du matériel dans une autre fonction que la position des pièces...
-> Pour negamax (pour GrogrosZero aussi?) continuer le calcul jusqu'à ce qu'il n y ait plus de pièces en prise pour le calcul??
-> Calculer l'avancement de la partie au fur et à mesure de la partie (au lieu de le faire à chaque fois)
-> Faire une fonction qui reset les paramètres d'évaluation (_material = false...)
-> Fonction pour le positionnement des pièces
-> Positionnement des pièces trop éxagéré?
-> Retirer tous les switch
-> Position des pièces : ajouter le middle game? sinon il mettra pas sa dame au milieu?
-> q5k1/2p2pp1/8/7p/2BRP3/5K2/P1P2PPP/8 w - - 0 4
-> r3r1k1/ppp4q/1n3Q2/3p1P2/5P2/2q5/PpPqN1B1/1K5R w - - 0 26
-> Voir si move_label ralentit tout... (en regardant des mats etc...)
-> r1b2kn1/1p1p1p1r/1p1P3p/1N1N1R2/6p1/8/PP4PP/5RK1 b - - 3 21 : +25, statiquement faut faire qq chose pour le comprendre dans l'évaluation de Grogros (+1.06)
-> GrogrosZero ralentit beaucoup quand les variantes deviennent longues
-> rn1r3k/pp4p1/1b2B3/5Qp1/3P4/P4b2/1P3PPP/6K1 b - - 1 3 : ici après Rxd4, tous les calculs sont perturbés par le fou en l'air en f3... ce qui ralentit la recherche de mat
-> ----> Faire une recherche spécialement de mat, où on prend plus en compte le matériel??
-> Au fil de la réflexion, retirer les coups pourris?? pour augmenter la capacité de stockage...
-> 8/7p/4k1p1/p2Nrp2/2P5/2KB3P/8/8 b - - 2 45
-> 8/8/7k/8/1p1p1p1p/pPpPpPpP/P1P1P1P1/1K6 w - - 0 1
-> Mettre une variable globale pour la règle des 50 coups (pour la passer à moins, si besoin)
-> Rechercher large au début, puis serré après??
-> 5rk1/pp4pp/2pb4/3p3q/B2Pp1bP/2N1B3/PPP3P1/R3Q1K1 b - - 0 2 : regarder les coups les plus offensifs, pas Fe6...
-> Re augmenter l'activité? car à elle seule, elle dépasse rarement 0.25...
-> Ajouter les colonnes ouvertes sur le roi... : attention aux fishing poles (r1bq1rk1/ppppnpp1/2n4p/2bNp1N1/2B1P2P/2P5/PP1P1PP1/R1BQK2R b KQ - 0 1)
-> r2qr1k1/ppp2ppp/1n1pb3/4P3/3P4/1BN5/PP3PPP/R2QR1K1 w - - 6 9 : d5 doit être évident
-> Sait pas faire les greek gifts : r1bq1rk1/ppppbppp/2n1p3/3nP3/3P3P/3B1N2/PPP2PP1/RNBQK2R w KQ - 1 7
-> Echanger les pièces quand on est matériellement gagnant
-> Echanger les pièces quand on a le roi faible, ou les pièces moins actives, ou moins d'espace
-> r4rk1/p2nbpp1/1qp4p/1p1pPB2/3P1B2/1P3P2/2Q2P1P/R4RK1 w - - 1 17 : pourquoi il regarde pas Fxd7?...
-> Changer un peu l'algo, pour que ça ne joue pas forcément le coup auquel il a le plus réflechi, mais un autre si il semble être meilleur (quand c'est mat, facile, mais sinon, comment savoir?)
-> switch() = lent : à changer
-> 2r2rk1/pp1q1pp1/2np1b1p/3NpN2/4P2P/P7/1PP2PP1/R2QK2R b KQ - 2 18 : Positionnement des pièces mieux pour les noirs??? ça va pas... (sûrement à cause du roi?)
-> rnbqkb1r/ppp2ppp/4p3/4P3/3Pp3/8/PPP2PPP/R1BQKBNR w KQkq - 0 6 : le pion e4 est pourri
-> Attaquer les faiblesses
-> 2r1r1k1/p2n1ppp/1p6/2pP4/1bP2P2/2N3N1/PB6/R4RK1 w - - 0 23 = égal????? un pièce de moins...
-> rnbq3r/pppp1kpp/5n2/6N1/3PP3/6b1/PPP4p/RNBQ1R1K b - - 2 10 : incompréhension de la position -> r4k1r/pbpp1Pp1/1p6/2nP4/2P3Q1/8/Pb1N3p/4RR1K w - - 0 22
-> Quand Grogros joue un coup auquel il n'avait pas pensé.. les évaluations déscendent pour les coups, 1 par 1... comment faire pour que tout descende en même temps?
-> r2qr1k1/1pp2ppp/pbnpbn2/4p3/3PP3/2P2N2/PPB2PPP/RNBQR1K1 w - - 1 13 : d5???!!
-> Pour king_safety, il faut absolument prendre en compte les pièces qui peuvent l'attaquer
-> Vérifier que le matériel fonctionne bien (pas d'erreurs de calcul??)
-> Regler tous les *100 et les /100 dans les évaluations
-> L'éval statique en position symétrique sera toujours nulle ?? (modulo le trait du joueur)
-> Distinguer positionnement (-> espace?) et développement des pièces (pour améliorer king_safety())
-> Puissance de la paire de fou qui dépend du moment de la partie? Qui dépend si y'a encore les dames pour compenser?
-> Découper la foncion draw en plein de sous-fonctions?
-> Quand Grogros est stoppé, je veux quand même voir l'affichage de l'analyse...
-> Chercher ce qui prend le plus de temps dans la GUI
-> Recherche de Grogros : utiliser UCT
-> Trouver un profiler pour VS code
-> Tester Grogros sur les leçons stratégiques de chess.com
-> r1b1k2r/pp1p1ppp/1qn2n2/2b1p3/2P1P3/2N4P/PP3PP1/R1BQKBNR w KQkq - 1 7 : arrête de bongcloud stp... y'a juste Dd2
-> Mettre des static const un peu partout pour éviter les re définitions inutiles
-> Comparer eval stockfish et Grogros sur : 1r4k1/5pp1/3p1q1p/1pb2P1P/2p1Q3/2P2N2/1P2RPP1/6K1 w - - 5 33
-> Si un tableau de valeurs est trop 'important', genre placement de la tour, demander à chatGPT de le re adapter
-> Utiliser CUDA (GPU) pour paralléliser des calculs
-> Faut-il prendre en compte le nombre de noeuds dans un fils pour déterminer s'il faut regarder dedans?
-> Ordonnencement des coups : checks/captures/attacks
-> Il faut dire que nulle, c'est environ 0% de chances de gain x) en gros
-> Vérifier que j'ai pas fait de connerie dans la backprogpagation? (ce qui expliquerait les grosses oscillations?)
-> Dans les positions ou des pièces peuvent être capturées, Grogros évalue très mal
-> Retirer les espace dans les parsing de FEN (ou autres caractères non désirés)
-> Dissocier les paramètres structure de pions et pions passés? (pour aider au debug)
-> Passer tous les commentaires en anglais et clean le code?
-> r1bq1rk1/p1pn1pbp/1p1p1np1/4p3/3PP1PP/2N1BP2/PPPQ4/2KR1BNR b - - 0 9 : différence d'évaluation entre stockfish et grogros : sous-estimation de l'attaque de pions sur le roi noir? activité des pièces aussi (mieux pour les noirs selon grogros...)
-> Mettre des proba de gain négatives sur les mats??
-> r1b1nrk1/p3npbp/2p3p1/4p3/4P1P1/2N1BP2/PPP4P/2KR1BNR b - - 2 13 : activité à revoir, ca c'est carrément mieux pour les blancs là (devrait rajouter 0.75, pas 0.15)
-> Evaluations en finales : un cavalier seul sans pions ni rien = nul
-> Mettre les noms de version sur le nom de Grogros
-> Utiliser toutes les améliorations/optimisations possibles sur VisualStudio
-> Mettre des uint_fast8_t partout
-> Negamax : utiliser les plateaux tout faits du buffer?
-> Mettre des variables globales partout !
-> Il faut peut-être supprimer les string?
-> Faire une classe "coup"? -> cela simplifierait sûrement beaucoup de choses...
-> Faire une fonction qui regarde si un coup est légal
-> Regarder et virer toutes les conversions de int à float, et opérations entre int et floats
-> Position à tester : GrogrosZero vs Grogrosfish : r1bqkb1r/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
-> Beaucoup d'informations sont inutiles dans les plateaux (genre le temps, ou infos de GUI)
-> Nouveau gros bug : quand on fait réfléchir grogros, qu'on supprime et qu'on lance grogrosfish, ça crash (ça peut crasher)
-> GrogrosFish a joué un coup après être mat... pourquoi?
-> Faire des pre-calculs dans des constantes (genre des divisions), pour éviter de les faire à chaque fois
-> Mettre des flags pour les coups (capture, check, promotion -> caval possible??)
-> Montrer le nombre de noeuds regardés par le quiescence search?
-> Mettre dans le nom de grogros les paramètres de recherche...
-> Descendre un peu le winrate par éval?
-> Quels paramètres sont meilleurs pour GrogrosZero? k_add? beta? quiescence_depth?
-> Utiliser std:sort pour trier les coups plus rapidement?
-> De la mémoire est allouée quelque part, et non désallouée... (elle ne fait pas partie du buffer) -> A cause d'initialisation de variables? faut-il les supprimer à la fin des fonctions?
-> Evaluation middle game? pour faire les ouvertures en développement, puis middle game en activité et placement...
-> Alignement de pièces (tour/roi, tour/dame...)
-> Taper les faiblesses (notions de pression)
-> Réduire la puissance des fous : un cavalier bien placé peut être tout aussi bon
-> Créer une map des contrôles des cases, puis l'utiliser pour les différentes composantes de l'évaluation
-> Quand l'arbre est trop plein : élaguer les branches pourries, pour se concentrer sur les plus prometteuses
-> Faut-il vraiment regarder tous les coups d'une position avant de chercher plus loin??
-> Changer l'aspect des flèches
-> Evaluer les finales différemment : faires des fonctions exprès pour (carré du pion, mat fou cavalier : il faut aller dans le bon coin)
-> Optimiser la mémoire de Grogros
-> Ajouter la nature de la position : ouverte, fermée... pour savoir à quel point les fous/cavaliers sont meilleurs, si y'a des chances de gain, si le développement des pièces compte beaucoup... l'espace aussi, la sécurité des rois...
-> 1qr3k1/5p2/6pQ/p1RpPpB1/P2n4/7P/5PP1/6K1 w - - 0 32 : il faudra combien de temps pour voir Ff6?...
-> Améliorer l'implémentation de l'activité des pièces : peuvent pas vraiment se déplacer sur des cases pourries...
-> Mettre des 'const' à la fin des nom de fonction? (ça peut peut-être les accélérer...)
-> Pour mieux évaluer la sécurité du roi, il faut regarder le surnombre de pièces sur le roi adverse (+2, c'est généralement mat)
-> Pourquoi dans les Caro-Kann, il fait Fd3 pour reprendre du pion c plutôt que de la dame?
-> Faire que Grogros reclique sur la fenêtre principale après jouer son coup sur chess.com pour reprendre la main? ou alors faire un focus sur la fenêtre?
-> Utiliser des static constexpr pour éviter des calculs redondants dans certaines fonctions
-> Il faut plus de g3 Fg2 -> revoir king safety?
-> Grogros préfère le pion e au c car il pense que son roi est plus safe avec
-> Re utiliser la réflexion de la quiescence search pour la recherche normale
-> Faire des groupes de variables pour savoir lesquelles réinitialiser lors d'un coup...
-> Grogros fait des Fxh7 -> g6/Rg7 et perd le fou... (à cause de king safety?)
-> Faire evaluation.cpp? gui.cpp?
-> Comme pour le roi, garder en mémoire l'emplacement des pièces (utiliserait 256 bytes)
-> Virer toutes les variables d'évaluation dans le plateau
-> Activité des pièces = contrôle des cases dans le camp adverse?? A revoir absolument
-> Afficher seulement les paramètres d'évaluation qui font sens? (par exemple king opposition seulement lorsque c'est une finale de pions)
-> Tout foutre dans la classe GUI? les variables globales et fonctions globales en particulier -> par exemple eval_components
-> Adapter les fonctions aux coups (make_move(Move)...)
-> Faire des méthodes utiles pour les coups
-> Verifier que quiescence depth 0 est aussi rapide qu'une simple évaluation
-> Flags pour les coups
-> Rajouter les noeuds du quiescence search dans le nombre de noeuds de grogros zero
-> Rendre from_fen plus tolérant
-> Améliorer l'évaluation en faisant jouer contre Leela, et regarder ce que Grogros évalue mal
-> Différencier mobilité et activité des pièces
-> Eviter les accès à _moves[i] trop souvent? -> copier le coup?
-> Get_control_map() + type control_map (à utiliser lors de l'évaluation pour la calculer seulement une fois)
-> Remplacer total_nodes() par _total_nodes, et faire comme _quiescence_nodes? Vérifier la vitesse des deux approches



----- Interface utilisateur -----

-> Amélioration des sons (en faire des maison?)
-> Undo move dans l'interface, avec les flèches (il faut donc stocker l'ensemble de la partie - à l'aide du PGN -> from_pgn?)
-> Nouveau sons/images
-> Dans le negamax, renvoyer le coup à chaque fois, pour noter la ligne que l'ordi regarde?
-> Faire des boutons pour faire des actions (ex copier ou coller le FEN/PGN, activer l'IA ou la changer...)
-> Options : désactivation son, ...
-> Chargement FEN -> "auto-complétion" si le FEN est incorrect
-> Régler le clic (quand IA va jouer), qui affiche mal la pièce
-> Modification du temps
-> Interface qui ne freeze plus quand l'IA réfléchit -> Paralléliser
-> Sons pour le temps
-> Premettre de modifier les paramètres de recherche de l'IA : beta, k_add... (d'une meilleure manière)
-> Musique de fond? (désactivable)
-> Ajouter un éditeur de positions (ajouter/supprimer les pièces)
-> Utiliser 1 thread pour gérer l'affichage tout seul
-> Undo doit retirer le coup du PGN aussi
-> Afficher les textes avec des différentes couleurs pour que ça soit plus facile à lire
-> Défiler la variante quand on met la souris dessus
-> Montrer sur l'échiquier quand la position est mate (ou pate, ou autre condition de fin de partie)
-> Faire un reconnaisseur de position automatique
-> Unload les images, textures etc... pour vider la RAM?
-> Pouvoir changer les paramètres de l'IA dans l'UI
-> Ajouter des options/menus
-> Pouvoir sauvegarder les parties entières dans un fichier (qui s'incrémente), pour garder une trace de toutes les parties jouées
-> Analyses de MC : montrer le chemin qui mène à la meilleure éval, puis celle qui mène au jeu qui va être joué
-> Importation de position / nouvelle position -> update les noms et temps
-> Passer le temps des joueurs dans la GUI plutôt que dans les plateaux?
-> Pouvoir grab le slider, ou cliquer pour changer sa place
-> Faire des batailles entre différents paramètres d'évaluation pour voir la meilleure config -> Retour des batailles de NN?
-> Importation depuis un PGN
-> Afficher sur le PGN la reflexion de GrogrosZero
-> Pouvoir changer le nombre de noeuds de l'IA dans la GUI... ou la profondeur de Grogrosfish
-> Pouvoir reset le temps
-> Problème avec les noms quand on les change : parfois ils ne s'affichent plus
-> Parfois l'utilisation des réseaux de neurones bug
-> Nd2f3 -> Ndf3? pas facile à faire
-> Ajouter la possibilité de faire plusieurs pre-move
-> Rajouter la date dans les PGN
-> Faire du smooth sur la barre d'évaluation
-> Faire un graphe d'éval en fin de partie?
-> Mettre des + sur les flèches (comme il y'a des -...)?
-> Faire un fonction pour tranformer une éval en son texte (mat ou non)
-> Certains calculs sont peut-être en double dans l'affichage
-> Echelle logarithmique pour la barre d'éval?
-> Gestion du temps bizarre? Car le temps affiché par GrogrosZero n'est pas vraiment le vrai (ni sa vitesse)
-> Clean l'implémentation de la GUI -> Faire des nouvelles fonctions pour tout simplifier
-> Faire un vecteur pour les pre moves et les flèches
-> Fins de parties : message + son
-> +M7 -> #-7 pour les noirs? .. bof
-> Barre d'éval : barre pour l'évaluation du coup le plus recherché par l'IA? ou éval du "meilleur coup"?
-> Mettre le screenshot dans le presse-papier?
-> Faire un readme
-> Faire un truc pour montrer la menace (changer le trait du joueur)
-> CtrlN doit effacer tout le PGN... parfois ça bug
-> Pouvoir éditer les positions
-> PARALLELISER L'AFFICHAGE !! ça lag beaucoup trop !!!
-> Refaire les pre-moves depuis zero (et ajouter la possibilité d'en faire plusieurs)
-> Mettre la couleur de la fenête en sombre
-> Faire un arbre pour la partie actuelle analysée, pour pouvoir avancer ou reculer dedans, faire une nouvelle variante...
-> Revoir les font_spacing etc... en faire des fonctions pour rendre le code plus lisible
-> Trop de flèches = crash
-> Lignes de bézier et cercles pas très beaux
-> Nouveaux bruits de pièces plus "soft" + bruit d'ambiance?
-> Montrer toute la variante calculée avec des flèches (d'une couleur spéciale)
-> Thread : bug... parfois les coups joués ne sont pas les bons
-> Re foncer le noir des pièces?
-> Ajout du titre BOT : [WhiteTitle "BOT"]
-> Afficher quand-même la barre d'éval même si GrogrosZero est arrêté?
-> Pourquoi dans certaines variantes, l'éval ne s'affiche pas à la fin??
-> Il doit sûrement manquer des delete quelque part?
-> Dans les .h, remetre les noms des arguments?
-> Rajouter des pre-moves pour Grogros si c'est un coup forcé en face
-> Pourquoi c'est lent de changer  la taille de la GUI?
-> Pour la barre d'éval, on utiise le winrate?
-> Faire des maps pour afficher les paramètres d'évaluation (genre afficher les cases controllées, avec une couleur plus ou moins prononcée...)
-> Pouvoir facilement changer les paramètres de Grogros (quiescence depth, beta, k_add, paramètres d'évaluation...)
-> Pouvoir changer l'affichage de la GUI (winrate/eval)
-> Faire des menus
-> Désélectionner les pièces lors d'un chargement de position
-> Mettre une évaluation de grogrosZero pour main_GUI._board pour pas que grogrosFish prenne le dessus
-> Utiliser des checkCollision de raylib pour la GUI
-> Afficher l'incrément de temps sur la GUI, et pouvoir le modifier
-> O-O+ dans les move label à prendre en compte
-> Revoir les update du temps quand on le change pendant que ça joue
-> Afficher des traits autour du plateau chess.com?
-> Affichage de la réflexion de Grogros sur le PGN : {N: 10.29% of 544}
-> Revoir le compare moves (sinon ça affiche pas toujours le meilleur coup au dessus)
-> Quand on arrive au mat dans le quiescence, l'affichage de l'éval bug (-99800000 au lieu de mat en 2)
-> Adapter nodes_per_frame en fonction du temps de réflexion de GrogrosZero (tant que la parallelisation n'est pas faite)
-> Montrer l'incrément sur la GUI




----- Réseaux de neurones -----

-> Faut t-il de la symétrie dans le réseau de neurone? (car sinon il évalue pas de la même manière les blancs et les noirs)



----- Evaluations incorrectes + corections à ajouter -----

-> 5rk1/r3npbp/2p2np1/2N1p3/2B1P1P1/1P2BP2/b1P4P/2KR2NR b - - 2 19 : mobility+ / outpost
-> 2rqr1k1/p2n1ppp/1p3n2/2pP4/1bP2Pb1/2NQ2PP/PB2N1B1/R4RK1 b - - 0 16 : préfère 2rqr1k1/p2n1ppp/1p6/2pP4/1bP2Pb1/2NQ4/PB2N1B1/R4RK1 w - - 0 19 que 2rqr1k1/p2n1ppp/1p3n2/2pP4/1bP2P2/3Q2PP/PB2N1B1/R4RK1 b - - 0 17
-> 8/pppbn2r/3p4/4k1p1/1P2P3/P1P1RP2/6P1/3R2K1 b - - 1 27
-> r1b2rk1/pp5p/4p2p/q2pQ3/8/P1p4R/2P1NPP1/R3K3 b Q - 1 18 : king safety++
-> r1b5/pp3k1p/4p3/3p2Q1/8/q1p5/2P1NPP1/3RK3 w - - 1 23 :  king safety++
-> r1b5/ppQ4p/4k3/3pp3/8/2R5/1qP2PP1/2NK4 w - - 2 30 : king safety++
-> r1b5/ppQ4p/4k3/3p4/4p3/2RN4/2PK1PP1/1q6 w - - 0 32 : king safety++
-> 6k1/3q1p2/3p2p1/bp1p4/5n2/7P/1BQN1PP1/4N1K1 w - - 0 35 : structure de pion--
-> r2qrbk1/5ppp/pn3n2/4N3/1ppP1P2/4PQ2/PB2N1PP/2R2RK1 b - - 1 20
-> 4rrk1/1pp1qnpp/p3b3/P3Pp2/1bP2B2/1N6/1P2B1PP/R2Q1RK1 b - - 0 20
-> r4rk1/ppp2ppp/2n5/1QQpp3/8/2NPbq1b/PPP2P1P/R3R1K1 b - - 3 18
-> 2kr2r1/ppp2p1p/5Q2/8/1b2q3/4BN1b/PP3PPP/R4K1R w - - 3 19
-> r1b1k2r/pp2qpp1/3p3p/2pPn3/2P1P3/4P1P1/PP2B2P/R1BQ1RK1 w kq - 1 16
-> 2r2rk1/pp1b1pp1/1q1p3p/3P4/1P2P3/5QP1/1B4KP/R1R5 w - - 1 25 -> square controls--
-> q4rk1/3bbppp/2n1pn2/1BPp4/P7/1Q2P3/3N1PPP/2R2RK1 w - - 3 17 -> passed pawn-- (squares are controlled)
-> q1r3k1/3bbppp/2n1pn2/1BP5/P7/4PN2/2Q3PP/1R3RK1 b - - 0 20 -> passed pawn--, center control-, positioning-
-> q1r3k1/4bpp1/2b1pn1p/nBP5/P7/3QPN2/6PP/2R2RK1 w - - 4 23 -> 'slider on queen'+, imbalance+?
-> 1r4k1/4bpp1/2n1pn1p/2P5/q7/3QPN2/6PP/2R2RK1 w - - 0 26 -> (material + imbalance)+, passed pawn--, connected pawns (defense)+
-> 2rr2k1/pp3ppp/5b2/n7/2p2P2/P3P1P1/1PbNBK1P/R1B2R2 w - - 0 20 -> piece mobility+
-> r7/1pp2kpp/5p2/1Pbr4/4p3/2B1P1P1/5P1P/1R3RK1 w - - 1 25 -> king safety... ???
-> 6k1/1p3ppp/r7/3p1b2/1P6/2BpPP2/3P1KPP/5R2 w - - 5 29 -> king safety...
-> 1r2kb1r/1q3pp1/p2p1n2/Pp1Pn1Bp/2bNP3/Q4B2/1P2N1PP/2R2RK1 b k - 2 24 : Stockfish dit +1.... Grogros +4
-> 1r2k2r/1q2b1p1/p2p1p2/Pp1Pn3/2bNP1p1/4B1Q1/1P2N1PP/2R2RK1 w k - 4 30
-> r3r1k1/ppp3p1/3q1n1p/3P4/2P5/1P3P2/P2Q2PP/4RRK1 w - - 1 27 : king danger ??
-> r6r/ppq2pk1/2p1bN1p/2P1p3/1P2P3/P2P3P/3QN1P1/5R1K w - - 3 6 : ici il se sent plus safe en g1... changer king safety : mur = ionp++
-> q4rk1/p3bppp/1p6/8/b1PP4/3QPP2/P4KPP/1R5R b - - 2 22
-> 8/2p1k1pp/p1Qb4/3P3q/4p3/N1P1BnPb/P4P2/5R1K w - - 1 25 : king danger
-> r4r1k/2p2ppB/pb6/n3R1B1/Pp1q4/5Q2/1P3PPP/RN4K1 w - - 1 20 :  king danger - (fou seul...)
-> 2k2r2/pp1n1N2/2nPppQ1/2Pb4/8/4B1P1/P4P1P/r4BK1 w - - 3 26 : Grogros dit +3, et Leela -1 (et seulement après Fh6, Fc4, Grogros voit la douille)
-> 2k2r2/pp1n1N2/3PppQB/2P5/2bn4/6PP/P4P2/r4BK1 w - - 1 28 : Grogros met beaucoup de temps à comprendre
-> r3rn2/b1q2ppk/3p1n1p/pp3P2/P2Pp3/4B2P/1PBQ1PPN/3RR1K1 b - - 1 22 : king safety : le roi noir n'est pas vraiment en danger ici
-> r1bqkb1r/ppp2pp1/5n2/3pn2P/8/4P3/PPP2PBP/RNBQK1NR w KQkq - 0 7 : king danger + psqt
-> 8/4k2p/2pNb3/2P1P1pP/r7/8/1P3r2/1K1R3R w - - 0 35 : king safety ??
-> 2b2rk1/1p4p1/2n4p/2P1p3/8/2PP1pPq/3B1P2/3Q1RKB b - - 1 23
-> r4rk1/1b4p1/pqp4p/bp6/P1pPN2Q/4BP2/1n4PP/RB3RK1 w - - 2 26
-> rnb2bnr/ppp4k/3p3p/6BQ/2BPP3/2N5/PPP2K1P/q7 w - - 0 15
-> r1bqkb1r/1p3pp1/p1np3p/3Np3/4P3/N7/PPP2PPP/R2QKB1R w KQkq - 2 11 : gros trou en d5
-> 2rq1rk1/1p2bpp1/p1np3p/3Np2Q/4P1BP/2P5/PP3PP1/R4RK1 b - - 4 20
-> 1r1q1rk1/4bpp1/2np3p/pp1NpQ2/P3P1BP/2P3P1/1P3P2/R4RK1 b - - 1 23
-> r4b1r/pp3k1p/2p1bp2/3n4/2BP1B2/P7/1PP2PPP/R4RK1 w - - 0 17 : king safety??
-> r1bq1rk1/ppp2ppp/2np4/2b5/2PNPPn1/2N5/PP4PP/R1BQKB1R w KQ - 1 9 : king safety??
-> r3r1k1/p1p2ppp/2p5/2b1P3/2Pq2b1/2N3Q1/PP3nPP/R1B1KBR1 w Q - 1 15 : king safety -> -11 selon stockfish
-> r1b1k2r/pp1n3p/4p3/3pP1Q1/q2p4/3B4/P2N2PP/R4RK1 b - - 2 19 : king safety..............
-> b1rr2k1/4bpp1/pq3n2/1p3P1p/3P4/P1NQB1P1/4B2P/R1R3K1 w - - 0 24 -> slider on queen
-> b2r2k1/5pp1/p7/1p3P2/3Pq1p1/P1Q3P1/5B1P/R5K1 w - - 1 31 : king safety
-> r1b1kb1r/pp1pnpp1/8/1Np4p/4P3/6Q1/q1PB1PPP/3RKB1R b Kkq - 1 13 : le roi noir est en danger. Stockfish +2, Grogros -2
-> r1b1kb1r/ppBp1pp1/q5n1/1Np5/2B1P2p/2Q5/2P2PPP/3R1RK1 b - - 9 18 : king danger+++
-> r1b1kb1r/ppBp1pp1/8/1Np1P3/2B4p/2Q5/2P3PP/5K2 b - - 0 23
-> 5r1k/pR2p1bp/q1P3p1/5b2/8/1B2BN2/5PPP/2R3K1 b - - 4 20
-> 2r1kr2/pb3ppp/4pn2/2P5/pP6/4P3/PP3PPP/2KR1B1R b - - 0 15 : structure de pions surestimée?
-> 2r2r2/4kpRp/4pn2/2Pb4/pP6/4P3/1P2BP1P/2KR4 b - - 0 20 : pawn structure + king safety.. ???
-> 1k6/p1p5/8/2K5/6Bp/8/8/8 b - - 0 7 : ça c'est ingagnable aux blancs, y'a plus qu'un fou... faire qq chose pour l'eval d'engames
-> 2kr2nr/2p2ppp/2Pb4/5q2/2Pp1B2/7P/RP3PP1/R5K1 b - - 0 2 : ici faut que l'eval statique comprenne que le roi est bloqué
-> 2b1qk2/r3np2/4pBp1/p2pP3/1ppP4/2P5/PPB2PP1/2KR3R w - - 2 4 : pareil ici : réseau de mat
-> rnb2b1r/p3kPpp/2p5/3nP3/2p5/2N2N2/PP3PPP/R1BR2K1 w - - 1 13 : ...........
-> 2r3k1/p4pbp/q3p1p1/8/8/1Q2P3/P4PPP/3K2RR w - - 0 27 : king danger+++++++
-> r1bq1rk1/ppppnpp1/2n5/2bNp1P1/2B1P3/2P5/PP1P1PP1/R1BQK2R b KQ - 0 2
-> rnbqk2r/pppp1ppp/8/4P3/2B1n3/5N2/PPP1KbPP/RNBQ3R b kq - 1 6 : king safety -820 mdr
-> rnbq1k1r/pppp2pp/8/2bQP2B/8/5N2/PPP3PP/RNB1K2n b Q - 1 8
-> 5rk1/p4pp1/7p/Q1p4b/2P1n3/P5nP/1PB3P1/RNr1N1K1 b - - 1 7
-> 4k2r/1b1n1ppp/p4n2/1p1P2N1/1P6/P1r1P1P1/2P1B2P/R4RK1 b k - 0 20 : ça c'est déjà foutu pour les blancs
-> rn3r2/pbppq1p1/1p2pN2/8/3P2NP/3B1kP1/PPP2P2/R3K2R w KQ - 1 6 : king safety... faut du +20
-> 5rk1/pp4pp/2pb4/3p3q/B2P3P/2N1Bp2/PPP5/R3Q1K1 w - - 0 4
-> r1b1kb1r/ppp1q1pp/8/5pN1/2Qp1P2/8/PP1N2PP/R3R1K1 b kq - 0 13
-> 7k/1pR4p/p4p2/r4p2/4r3/4P2R/PP4PP/7K b - - 1 38 : +6 selon Grogros xD


----- Problèmes -----

r3k2r/ppp2pp1/2p1bq2/2b4p/3PP1n1/2P2BP1/PP3P1P/RNBQK2R w KQkq - 3 11 : h3 (+4.50)
3rk3/ppp2pp1/2p2q1r/2P1n3/4P2p/1QP3Pb/PP1NBP1P/R1B1R1K1 b - - 5 16
3rk2r/ppp2pp1/2p1bq2/2P4p/4P1B1/2P3P1/PP3P1P/RNBQK2R b KQk - 0 12
1rkb4/pNp5/8/2N4p/8/5B2/4K3/8 w - - 0 1
2k1r1r1/p1p4p/1qpb4/3PN3/2Q3b1/4B3/PP3PP1/2R2RK1 b - - 0 1
2kr2nr/1pp2ppp/3b4/1P3q2/2Pp1B2/5Q1P/RP3PP1/R5K1 w - - 0 1
4r1k1/5pP1/2q2Q1p/1p1RP3/p7/5RP1/5P1K/7r w - - 0 1
6bk/7p/2q1PP2/2Pp4/3R4/5K2/8/B7 w - - 0 1
8/6Q1/2r1p3/2rkr3/2rrr2Q/8/7K/8 w - - 0 1
5rk1/pp4pp/4p2r/4R1Q1/3n4/2q4B/P1P2PPP/5RK1 b - - 0 1
2b1q1r1/r3np1k/4pQpP/p2pP1B1/1ppP4/2P5/PPB2PP1/2KR3R w - - 0 1
1r1r3k/2q1b2p/p2pB1pB/3Pp2n/1pn5/1N3P2/PPP4Q/1K4RR w - - 0 1
bK3kq1/PPNppp1p/1b1n3P/n1R5/1p1Q3r/8/2R4p/r7 w - - 0 1
r3Rk2/1pB2ppp/6b1/1P6/p1nN4/2P4P/1P3PP1/4R1K1 b - - 0 1
rnbqkbnr/pppppppp/8/PPPPPPPP/PPPPPPPP/PPPPPPPP/PPPPPPPP/4K3 w kq - 0 1
r1b1k2r/pp1nqpp1/4p2p/3pP1N1/8/3BQ3/PP3PPP/2R2RK1 w kq - 0 1
1B3B1B/2B5/p6B/8/8/8/8/1k1K4 w - - 0 1
r1n2b1r/pN2kppp/1pQ1pq2/3p4/5B2/5B2/P4PPP/1R3RK1 w - - 0 1
3rr1k1/1p1b1pnp/pqpPn1p1/8/8/1PQ2N2/PBB2PP1/3RR1K1 w - - 0 1
Q7/4pP1p/7k/4q2b/1B1r2PK/7n/8/6r1 w - - 0 1
2q5/ppr3k1/3p4/P1pPp1bB/2P4n/1P5P/3Q1P1K/6R1 b - - 0 1
8/8/4p3/8/8/1P6/7P/2K3k1 w - - 0 1
1RR3Q1/8/5p2/4p3/P3B3/5ppp/K3prqk/4Nbrn w - - 0 1
8/3pp1p1/4B3/1N1Pp3/2k5/5p1p/4prpr/4Kbnq w - - 0 1
7k/4Q3/8/4p3/4p3/4pppp/4prpr/4Kbnq w - - 26 27
8/8/5Q2/4p3/4p1k1/4pppp/4prpr/4Kbnq w - - 40 34
8/7k/8/4p3/4p1Q1/4pppp/4prpr/4Kbnq w - - 48 38
rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1 : Mat de Lasker
8/5k2/8/6Kp/6P1/8/8/8 w - - 40 69
8/8/5p1P/8/4Pk2/2bP4/2B5/5K2 w - - 0 1
K1k5/P1Pp4/1p1P4/8/p7/P2P4/8/8 w - - 0 1
4rk2/1rR2ppp/pq6/8/Q7/8/PP3PPP/5RK1 w - - 0 1
5k1r/pR3p2/5P1p/3bq3/7Q/P7/5PPP/6K1 w - - 0 1
kb5Q/p7/Pp6/1P6/4p3/4R3/4P1p1/6K1 w - - 0 1
2kr4/1pp1R3/p1b2Q1p/7B/8/P7/5PrP/2B4K b - - 0 1
r4rk1/pp1qb1pp/2p1b3/1B3p2/3Pp3/8/PPP1Q1PP/RNB2RK1 w - - 0 1
5rk1/pp4pp/2pb4/3p1q2/B2Pp1b1/2N1B3/PPP3PP/R3Q1K1 b - - 0 1
3brrk1/p2q3p/2pnb3/1p1pBp2/3P1Pp1/2PBN3/PP2Q1PP/3R1RK1 w - - 0 1
3brrk1/p2q4/2p1b3/4Bp2/2pP1Ppp/P1P1N3/2Q3PP/3R1RK1 w - - 0 1
3k4/8/7p/2p1p1pP/1pPpPpP1/1P1P1P2/N7/2K5 w - - 0 1
r3r1k1/pp3pbp/1qp3p1/2B5/2BP2b1/Q1n2N2/P4PPP/3R1K1R b - - 0 1
r1bq1rk1/ppppnpp1/2n4p/2bNp1N1/2B1P2P/2P5/PP1P1PP1/R1BQK2R b KQ - 0 1
r4rk1/pnpb2p1/3p3q/1pbP1p1Q/7P/2B2N2/PpB2PP1/4RRK1 w - - 0 1
6k1/5p2/2Qp2p1/1p3b1p/2nB1P2/2P3PP/q3r1BK/2R5 w - - 0 1
1Q6/p1p1kp2/3q1b2/2p1p3/1B6/8/PP3r1P/K2R4 w - - 0 1
8/1rpK2k1/7p/3P2pP/8/6P1/1p6/1R6 b - - 0 1
8/6k1/7p/2p1p1pP/1pPpPpP1/1P1P1P1K/6N1/8 w - - 52 27
8/1r4k1/2K4p/2pP2pP/8/6P1/1p6/1R6 b - - 1 2
3r1r1k/pb1pq1pp/1pnR1pn1/4p1BQ/2B1P3/2N5/PPP2PPP/3R2K1 w - - 0 1
r2r1knR/6p1/1p1q2P1/2b3Q1/5pP1/pP2pP1R/7P/1K6 w - - 0 1
5rk1/RP1R4/8/8/8/8/5pr1/5K2 w - - 0 1
8/3k4/5P1p/1p1B1P2/2p2n2/8/1P1K3P/8 w - - 0 1
r1b2k1N/pppp1B1Q/2n2qp1/4p3/4n3/3P4/PPP2bPP/RNB2K1R b - - 0 1
8/8/2RQP3/2PKB3/3RP3/8/1q6/5k2 b - - 0 1
6rk/3PbRp1/6Qp/8/3p4/q2P4/6KP/8 w - - 0 1
2r3k1/5pp1/4p1P1/p2n4/q2P4/Pp1B1P2/1PrQ2P1/1K1R3R w - - 0 1
1b6/4P3/1P2PN2/8/8/P1k5/P1p2P2/K5B1 w - - 0 1
r3rqk1/pp3p1p/5Pp1/4p1N1/1P2P1bP/P2Q4/1K3P2/3R3R w - - 0 1
4r1k1/ppp2ppp/2n1qp2/2b5/7P/2P1NQ2/PBPP1PP1/R5K1 w - - 0 1
3q1b2/7k/5p1p/1Q3N2/5Pp1/3p2P1/P1r4P/2R2RK1 b - - 0 1
1rQ2b1k/3PBq1p/5N2/p5p1/R5P1/7P/5PK1/1q6 w - - 0 1
r5r1/p4p2/2ppb1kp/7N/qp2RPB1/7P/PPnQ1B2/R5K1 w - - 0 1
6k1/p1p2p1p/4q1p1/8/P4P2/2Q4b/1P2rP1P/2R3KR b - - 0 1
3rr1k1/1pq1bppp/p4n2/2P1n2b/PP1N4/BBN1PP2/4Q2P/R4RK1 b - - 0 1
8/8/6k1/1p5p/5K2/5P2/PPP5/8 w - - 0 1
8/5pk1/3p1n1p/R1r4P/p5p1/6P1/1B3P1K/8 w - - 0 4
8/3k4/6pP/2p5/rpP5/8/PK6/4R3 w - - 0 1
7k/6p1/5p1p/8/2p1r3/1P1p2P1/P2R2KP/8 b - - 1 1
8/8/7p/p4B1P/PbK2p2/1P2kPp1/6P1/8 w - - 77 112
8/8/7p/p1p4P/P3Kp2/1P3PpB/3b1kP1/8 w - - 63 66
8/8/6Kp/p1p4P/P4p2/1Pb1kPpB/6P1/8 w - - 67 68 vs 8/8/7K/p1p4P
1q5k/5K2/2p4b/8/8/8/1n4p1/RQ6 w - - 0 1
1k6/2q5/8/7p/5Q1P/5PK1/8/8 b - - 0 1
7k/7P/6P1/2b1PK2/8/p7/8/8 w - - 0 1
r3r1k1/3b1p2/p1pp3p/3Pp1q1/1p1bQP2/3P2B1/PPP1N1P1/2KR3R b - - 0 1
8/8/pp6/kp6/1R6/PK6/8/8 w - - 0 1
6q1/3r1p2/2N1nk1K/3rp3/8/5PP1/2Q5/3N4 w - - 0 1
8/8/8/3k4/8/1PK5/8/8 w - - 5 17 : finales à revoir
7Q/8/B4pp1/p5k1/3P2r1/q1PKP3/1r6/6R1 w - - 4 52
rn2k1nr/pp3ppp/2p5/3pN3/1b1P4/2NQP1P1/PP1B1PqP/R3K2R w KQkq - 7 12
8/8/5p2/5P2/k1B5/2K5/8/2n5 w - - 7 72
3r4/bP5p/5kp1/2N2p2/4pP2/1R4PP/6K1/8 w - - 5 45
r6k/p1p3pp/6n1/3Bp3/4P3/5r1q/PB1PNP2/R3QRK1 b - - 2 15 : ... mat en 3 pour les noirs
2k2r2/pp1n1NQ1/2nPpp2/2Pb4/1r6/4B1P1/P4P1P/5BK1 b - - 0 24
1r6/4P1P1/8/7k/8/7N/8/4K3 w - - 0 1
r4qk1/pp6/3ppBBp/8/1n5Q/8/PPP2PPP/2KR4 w - - 0 1
1k3q2/3r1p2/Kb6/p7/2N1Q3/1P6/8/8 w - - 0 1
6k1/p4pp1/5r1p/Q1p4b/B1P1n1P1/P2Nn2P/1P5K/RNr5 b - - 2 13

*/





// Test de fonction
void testA() {
    int x = 0;
    for (int index = 0; index < 64; index++) {
        const uint_fast8_t i = index / 8;
        const uint_fast8_t j = index % 8;
        x += main_GUI._board._array[i][j];
    }
    cout << x << endl;
}

void testB() {
    int x = 0;
    for (uint_fast8_t i = 0; i < 8; i++) {
        for (uint_fast8_t j = 0; j < 8; j++) {
            x += main_GUI._board._array[i][j];
        }
    }
    cout << x << endl;
}

void testC() {
    int x = 0;
    for (auto& i : main_GUI._board._array)
    {
        for (const unsigned char j : i)
        {
            x += j;
        }
    }
    cout << x << endl;
}








// Fonction qui fait le dessin de la GUI
void gui_draw() {
    if (!main_GUI._draw)
        return;
    BeginDrawing();
    main_GUI._board.draw();
    EndDrawing();
}



// Main
int main() {


    // Faire une fonction Init pour raylib?

    // Fenêtre resizable
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
    SetConfigFlags(FLAG_VSYNC_HINT);

    // Pour ne pas afficher toutes les infos (on peut mettre le log level de 0 à 7 -> 7 = rien)
    SetTraceLogLevel(LOG_WARNING);
      

    // Initialisation de la fenêtre
    InitWindow(main_GUI._screen_width, main_GUI._screen_height, "Grogros Chess");

    // Initialisation de l'audio
    InitAudioDevice();
    SetMasterVolume(1.0f);

    // Nombre d'images par secondes
    SetTargetFPS(fps);

    // Curseur
    //HideCursor();
    SetMouseCursor(3);


    // Variables
    // Board t;
    all_positions[0] = main_GUI._board.simple_position();
    total_positions = 1;


    // Evaluateur de position
    Evaluator eval_white;
    Evaluator eval_black;

    // Evaluateur pour Monte Carlo
    Evaluator monte_evaluator;

    // Nombre de noeuds max pour le jeu automatique de GrogrosZero
    int grogros_nodes = 3000000;

    // Nombre de noeuds calculés par frame
    // Si c'est sur son tour
    int nodes_per_frame = 500;

    // Sur le tour de l'autre (pour que ça plante moins)
    int nodes_per_user_frame = 100;

    // Valeurs à 0 pour augmenter la vitesse de calcul. A tester vs grogrosfish avec tout d'activé
    eval_white._piece_mobility = 0.0f;
    eval_white._piece_positioning = 0.0f;
    eval_white._attacks = 0.0f;
    eval_white._defenses = 0.0f;
    eval_white._king_safety = 0.0f;
    eval_white._kings_opposition = 0.0f;
    eval_white._pawn_structure = 0.0f;
    eval_white._player_trait = 0.0f;
    eval_white._push = 0.0f;
    eval_white._rook_open = 0.0f;
    eval_white._castling_rights = 0.0f;
    eval_white._square_controls = 0.0f;
    eval_white._space_advantage = 0.0f;
    eval_white._alignments = 0.0f;
    eval_white._piece_activity = 0.0f;

    // Paramètres pour l'IA
    int search_depth = 8;
    search_depth = 7;


    // Fin de partie
    bool main_game_over = false;

    //// Réseau de neurones
    //Network grogros_network;
    //grogros_network.generate_random_weights();
    //bool use_neural_network = false;


    //// Liste de réseaux de neurones pour les tournois
    //int n_networks = 5;
    //Evaluator **evaluators = new Evaluator*[n_networks];
    //for (int i = 0; i < n_networks; i++)
    //    evaluators[i] = nullptr;
    //Network **neural_networks = new Network*[n_networks];
    //Network *neural_networks_test = new Network[n_networks];
    //for (int i = 0; i < n_networks; i++) {
    //    neural_networks[i] = &neural_networks_test[i];
    //    neural_networks[i]->generate_random_weights();
    //}

    //neural_networks[0] = nullptr;
    //evaluators[0] = &monte_evaluator;


    // Met les timers en place
    main_GUI._board.reset_timers();


    //printAttributeSizes(main_GUI._board);
    //testFunc(main_GUI._board);


    // Nombre de threads pour la parallélisation
    int n_threads = 1;


    // Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
    while (!WindowShouldClose()) {


        // INPUTS      


        // Full screen
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_F11)) {
            if (!IsWindowMaximized())
                SetWindowState(FLAG_WINDOW_MAXIMIZED);
            else
                ClearWindowState(FLAG_WINDOW_MAXIMIZED);
            if (!IsWindowFullscreen())
                SetWindowState(FLAG_FULLSCREEN_MODE);
            else
                ClearWindowState(FLAG_FULLSCREEN_MODE);
        }



        // T - Test de thread
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {

            // Vecteur de threads
            //vector<thread> threads;
            //mutex boardMutex; // Mutex for synchronizing access to main_GUI._board

            //for (int i = 0; i < n_threads; i++) {
            //    //threads.emplace_back([&]() {
            //    //    // Lock the mutex before modifying main_GUI._board
            //    //    lock_guard<mutex> lock(boardMutex);
            //    //    main_GUI._board.grogros_zero(&monte_evaluator, 50000, true, main_GUI._beta, main_GUI._k_add, false, 0, nullptr, 4);
            //    //    });

            //    threads.push_back(thread(&Board::grogros_zero, &main_GUI._board, &monte_evaluator, 50000, true, main_GUI._beta, main_GUI._k_add, false, 0, nullptr, 4));
            //}

            //for (auto& thread : threads) {
            //    //thread.join();
            //    thread.detach();
            //}


            
            //cout << main_GUI._board.in_check() << endl;
            //main_GUI._board.quiescence(&eval_white);
            //cout << main_GUI._board._quiescence_nodes << endl;

            /*Array test_array;
            cout << sizeof(test_array) << endl;
            cout << test_array.pieces[0][0].type << endl;*/

            //main_GUI._board.grogros_zero(&monte_evaluator, 1, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);

            locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);
            main_GUI.new_bind_game();

            // Nombre de noeuds de la recherche quiescence
            //main_GUI._board.quiescence(&eval_white);
            //cout << main_GUI._board._quiescence_nodes << endl;
            
        }

        // CTRL-T - Cherche le plateau de chess.com sur l'écran
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
            cout << "looking for chess.com chessboard..." << endl;
            locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);
            printf("Top-Left: (%d, %d)\n", main_GUI._binding_left, main_GUI._binding_top);
            printf("Bottom-Right: (%d, %d)\n", main_GUI._binding_right, main_GUI._binding_bottom);
            cout << "chess.com chessboard has been located" << endl;
        }

        // LCTRL-A - BInding full (binding chess.com)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
            main_GUI._binding_full = !main_GUI._binding_full;
        }

        // LCTRL-Q - Mode de jeu automatique (binding chess.com) -> Check le binding seulement sur les coups de l'adversaire
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
            main_GUI._binding_solo = !main_GUI._binding_solo;
            main_GUI._click_bind = !main_GUI._click_bind;
        }


        // Changements de la taille de la fenêtre
        if (IsWindowResized()) {
            get_window_size();
            load_resources(); // Sinon ça devient flou
            resize_GUI();
        }

        // S - Save FEN dans data/text.txt
        if (IsKeyPressed(KEY_S)) {
            SaveFileText("data/test.txt", const_cast<char*>(main_GUI._current_fen.c_str()));
            cout << "saved FEN : " << main_GUI._current_fen << endl;
        }

        // L - Load FEN dans data/text.txt
        if (IsKeyPressed(KEY_L)) {
            string fen = LoadFileText("data/test.txt");
            main_GUI._board.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // F - Retourne le plateau
        if (IsKeyPressed(KEY_F)) {
            switch_orientation();
        }

        // LCTRL-N - Recommencer une partie
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
            //monte_buffer.reset();
            //main_GUI._board = monte_buffer._heap_boards[monte_buffer.get_first_free_index()];
            main_GUI.reset_pgn();
            main_GUI._board.restart();
        }

        // Utilisation du réseau de neurones
        // if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
        //     use_neural_network = !use_neural_network;
        // }

        // C - Copie dans le clipboard du PGN
        if (IsKeyPressed(KEY_C)) {
            SetClipboardText(main_GUI._pgn.c_str());
            cout << "copied PGN : \n" << main_GUI._pgn << endl;
        }

        // X - Copie dans le clipboard du FEN
        if (IsKeyPressed(KEY_X)) {
            SetClipboardText(main_GUI._current_fen.c_str());
            cout << "copied FEN : " << main_GUI._current_fen << endl;
        }

        // V - Colle le FEN du clipboard (le charge)
        if (IsKeyPressed(KEY_V)) {
            string fen = GetClipboardText();
            main_GUI._board.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // // Colle le PGN du clipboard (le charge)
        // if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        //     string pgn = GetClipboardText();
        //     main_GUI._board.from_pgn(pgn);
        //     cout << "loaded PGN : " << pgn << endl;
        // }

        // A - Analyse de partie sur chess.com (A)
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
            OpenURL("https://www.chess.com/analysis");
        }

        // A - Analyse de partie sur chess.com en direct, par GrogrosZero
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
            main_GUI.new_bind_analysis();
        }
            
        // TAB - Screenshot
        if (IsKeyPressed(KEY_TAB)) {
            string screenshot_name = "resources/screenshots/" + to_string(time(nullptr)) + ".png";
            cout << "Screenshot : " << screenshot_name << endl;
            TakeScreenshot(screenshot_name.c_str());

            // Mettre le screenshot dans le presse-papier?
        }

        // D - Dessine ou non
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
            main_GUI._draw = false;
        }
            
        // B - Création du buffer
        if (IsKeyPressed(KEY_B)) {
            cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
            monte_buffer.init();
        }
        
        // G - GrogrosZero
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
            if (!monte_buffer._init)
                monte_buffer.init();
            // ALT - Sans le calcul des mats/pats
            if (IsKeyDown(KEY_LEFT_ALT))
                main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, false, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);
            else {
                // LSHIFT - Utilisation du réseau de neurones
                if (IsKeyDown(KEY_LEFT_SHIFT))
                    //main_GUI._board.grogros_zero(nullptr, nodes_per_frame, true, main_GUI._beta, main_GUI._k_add, false, 0, &grogros_network);
                    false;
                else
                    main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);
            }
                
        }

        // LCTRL-G - Lancement de GrogrosZero en recherche automatique
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
            if (!monte_buffer._init)
                monte_buffer.init();
            main_GUI._grogros_analysis = true;
        }

        // Espace - GrogrosZero 1 noeud : DEBUG
        if (IsKeyPressed(KEY_SPACE)) {
            /*if (!monte_buffer._init)
                monte_buffer.init();
            main_GUI._board.grogros_zero(&monte_evaluator, 1, true, main_GUI._beta, main_GUI._k_add);*/
        }

        // LCTRL-H - Arrêt de la recherche automatique de GrogrosZero 
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
            main_GUI._grogros_analysis = false;
        }

        // H - Déffichage/Affichage des flèches, Affichage/Désaffichage des contrôles
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
            switch_arrow_drawing();
        }

        // R - Réinitialisation des timers
        if (IsKeyPressed(KEY_R)) {
            main_GUI._board.reset_timers();
            main_GUI._time = false;
        }

        // Suppr. - Supprime les reflexions de GrogrosZero
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
            main_GUI._board.reset_all(true, true);
        }

        // CTRL - Suppr. - Supprime le buffer de Monte-Carlo
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
            main_GUI._board.reset_all(true, true);
            monte_buffer.remove();
        }

        // D - Affichage dans la console de tous les coups légaux de la position
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
            main_GUI._draw = true;
            main_GUI._board.display_moves(true);
        }

        // T - Fonction de test
        if (IsKeyPressed(KEY_T)) {
        }

        // E - Évalue la position et renvoie les composantes dans la console
        if (IsKeyPressed(KEY_E)) {
            main_GUI._board.evaluate_int(&monte_evaluator, true, true);
            cout << "Evaluation : \n" << eval_components << endl;
        }
            
        // U - Undo de dernier coup joué
        IsKeyPressed(KEY_U) && main_GUI._board.undo();

        // Modification des paramètres de recherche de GrogrosZero
        IsKeyPressed(KEY_KP_ADD) && (main_GUI._beta *= 1.1f);
        IsKeyPressed(KEY_KP_SUBTRACT) && (main_GUI._beta /= 1.1f);
        IsKeyPressed(KEY_KP_MULTIPLY) && (main_GUI._k_add *= 1.25f);
        IsKeyPressed(KEY_KP_DIVIDE) && (main_GUI._k_add /= 1.25f);

        // R-Return - Reset aux valeurs initiales
        if (IsKeyPressed(KEY_KP_ENTER)) {
            main_GUI._beta = 0.05f;
            main_GUI._k_add = 25.0f;
        }

        // 1 - Recherche en profondeur extrême
        if (IsKeyPressed(KEY_ONE)) {
            main_GUI._beta = 0.5f;
            main_GUI._k_add = 0.0f;
        }

        // 2 - Recherche en profondeur
        if (IsKeyPressed(KEY_TWO)) {
            main_GUI._beta = 0.2f;
            main_GUI._k_add = 10.0f;
        }

        // 3 - Recherche large
        if (IsKeyPressed(KEY_THREE)) {
            main_GUI._beta = 0.01f;
            main_GUI._k_add = 100.0f;
        }

        // 4 - Recherche de mat
        if (IsKeyPressed(KEY_FOUR)) {
            main_GUI._beta = 0.005f;
            main_GUI._k_add = 2500.0f;
        }

        // 5 - Recherche de victoire en endgame
        if (IsKeyPressed(KEY_FIVE)) {
            main_GUI._beta = 0.05f;
            main_GUI._k_add = 5000.0f;
        }

        // P - Joue le coup recommandé par l'algorithme de GrogrosZero
        if (!IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P)) {
            if (main_GUI._board._tested_moves > 0)
                ((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board.best_monte_carlo_move(), true, true, false, false);
            else
                cout << "no more moves are in memory" << endl;
        }

        // LShift-P - Joue les coups recommandés par l'algorithme de GrogrosZero
        if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_P)) {
            if (main_GUI._board._tested_moves > 0)
                ((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board.best_monte_carlo_move(), true, true, false, false);
            else
                cout << "no more moves are in memory" << endl;
        }
        
        // Return - Lancement et arrêt du temps
        if (IsKeyPressed(KEY_SPACE)) {
            if (main_GUI._time)
                main_GUI.stop_time();
            else
                main_GUI.start_time();
        }
        

        // UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces blanches
        if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
            if (main_GUI._white_player.substr(0, 11) == "GrogrosFish")
                main_GUI._white_player = "White";
            else
                main_GUI._white_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
        }

        // UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces noires
        if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
            if (main_GUI._black_player.substr(0, 11) == "GrogrosFish")
                main_GUI._black_player = "Black";
            else
                main_GUI._black_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
        }

        // CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces blanches
        if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
            if (main_GUI._white_player.substr(0, 11) == "GrogrosZero")
                main_GUI._white_player = "White";
            else
                main_GUI._white_player = "GrogrosZero";
        }

        // CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces noires
        if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
            if (main_GUI._black_player.substr(0, 11) == "GrogrosZero")
                main_GUI._black_player = "Black";
            else
                main_GUI._black_player = "GrogrosZero";
        }


        // Fin de partie (à reset aussi...) (le son ne se lance pas...)
        // Calculer la fin de la partie ici une fois, pour éviter de la refaire?

        // Plus de temps... (en faire une fonction)
        // if (main_GUI._board._time) {


        //     if (main_GUI._board._time_black < 0) {
        //         main_GUI._board._time = false;
        //         main_GUI._board._time_black = 0;
        //         play_end_sound();
        //         main_GUI._board._is_game_over = true;
        //         cout << "White won on time" << endl; // Pas toujours vrai car il peut y avoir des manques de matériel
        //     }
        //     if (main_GUI._board._time_white < 0) {
        //         main_GUI._board._time = false;
        //         main_GUI._board._time_white = 0;
        //         play_end_sound();
        //         main_GUI._board._is_game_over = true;
        //         cout << "Black won on time" << endl;
        //     }

           
        // }


        // Jeu des IA

        // Fait jouer l'IA automatiquement en fonction des paramètres
        if (!main_GUI._board._is_game_over && main_GUI._board.is_mate() == -1 && main_GUI._board.game_over() == 0) {

            // GrogrosZero

            // Quand c'est son tour
            if ((main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosZero") || (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosZero")) {
                if (!monte_buffer._init)
                    monte_buffer.init();

                // Grogros doit gérer son temps
                if (main_GUI._time) {
                    // Nombre de noeuds que Grogros doit calculer (en fonction des contraintes de temps)
                    static constexpr int supposed_grogros_speed = 5000; // En supposant que Grogros va à plus de 20k noeuds par seconde
                    int tot_nodes = main_GUI._board.total_nodes();
                    float best_move_percentage = tot_nodes == 0 ? 0.05f : static_cast<float>(main_GUI._board._nodes_children[main_GUI._board.best_monte_carlo_move()]) / static_cast<float>(main_GUI._board.total_nodes());
                    int max_move_time = main_GUI._board._player ? 
                        time_to_play_move(main_GUI._time_white, main_GUI._time_black, 0.05f * (1.0f - best_move_percentage)) :
                        time_to_play_move(main_GUI._time_black, main_GUI._time_white, 0.05f * (1.0f - best_move_percentage));
                    int grogros_timed_nodes = min(nodes_per_frame, supposed_grogros_speed * max_move_time / 1000);
                    main_GUI._board.grogros_zero(&monte_evaluator, min(!main_GUI._time ? nodes_per_frame : grogros_timed_nodes, grogros_nodes - main_GUI._board.total_nodes()), true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);
                    if (main_GUI._board._time_monte_carlo >= max_move_time)
                        ((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board.best_monte_carlo_move(), true, true, false, false);
                
                
                }
                else
                    main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);
            }

            // Quand c'est pas son tour
            if ((!main_GUI._board._player && main_GUI._white_player.substr(0, 12) == "GrogrosZero") || (main_GUI._board._player && main_GUI._black_player.substr(0, 12) == "GrogrosZero")) {
                if (!monte_buffer._init)
                    monte_buffer.init();
                main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);
            }

            // Mode analyse
            if (main_GUI._grogros_analysis) {
                if (!monte_buffer._init)
                    monte_buffer.init();

                if (!is_playing()) 
                    main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);
                else
                    main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks); // Pour que ça ne lag pas pour l'utilisateur
            }

            if (main_GUI._board._is_game_over || main_GUI._board.is_mate() != -1 || main_GUI._board.game_over() != 0)
                goto game_over;

            // GrogrosFish (seulement lorsque c'est son tour)
            if (main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosFish")
                main_GUI._board.grogrosfish(search_depth, &eval_white, true);

            if (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosFish")
                main_GUI._board.grogrosfish(search_depth, &eval_black, true);

        }

        // Si la partie est terminée
        else {
            game_over:

            if (!main_game_over) {
                main_GUI._time = false;
                main_GUI._board.display_pgn();
                main_game_over = true;
            }
            
        }


        // Jeu automatique sur chess.com
        if (main_GUI._binding_full || (main_GUI._binding_solo && get_board_orientation() != main_GUI._board._player)) {

            // Le fait à chaque intervalle de temps 'binding_interval_check'
            if (clock() - main_GUI._last_binding_check > main_GUI._binding_interval_check) {

                // Coup joué sur le plateau
                main_GUI._binding_move = get_board_move(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, get_board_orientation());

                // Vérifie que le coup est légal avant de le jouer
                for (int i = 0; i < main_GUI._board._got_moves; i++) {
                    if (main_GUI._board._moves[i].i1 == main_GUI._binding_move[0] && main_GUI._board._moves[i].j1 == main_GUI._binding_move[1] && main_GUI._board._moves[i].i2 == main_GUI._binding_move[2] && main_GUI._board._moves[i].j2 == main_GUI._binding_move[3]) {
                    	main_GUI._board.play_move_sound(main_GUI._binding_move[0], main_GUI._binding_move[1], main_GUI._binding_move[2], main_GUI._binding_move[3]);
                        main_GUI._board.play_monte_carlo_move_keep(i, true, true, true, true);
                        break;
                    }
                }

                main_GUI._last_binding_check = clock();
            }

            
        }

        
        gui_draw();



    }

    // Fermeture de la fenêtre
    CloseWindow();


    return 0;

}
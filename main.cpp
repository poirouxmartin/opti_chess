#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"
#include <thread>
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
-> GrogrosZero casse Grogrosfish... ça fait jouer des coups bizarres à grogrosfish


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
-> Vérifier l'en passant sur le FEN : on dirait que les colonnes sont inversées
-> Pouvoir changer le temps des joueurs à volonté
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
-> Extension TODO
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
-> Bug d'affichage sur les mats





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
-> Interface qui ne freeze plus quand l'IA réfléchit
-> Sons pour le temps
-> Premettre de modifier les paramètres de recherche de l'IA : beta, k_add... (d'une meilleure manière)
-> Musique de fond? (désactivable)
-> (changer épaisseur des flèches en fonction de leur importance?)
-> Ordonner l'affichage des flèches (pour un fou, mettre le coup le plus court en dernier) (pour deux coups qui vont au même endroit, mettre le meilleur en dernier)
-> Ajouter un éditeur de positions (ajouter/supprimer les pièces)
-> Binding pour jouer tout seul en ligne
-> Problème au niveau des temps (dans grogrosfish : premier coup à temps max -> le temps n'est actualisé que après son coup...)
-> Quand on ferme la fenêtre, GrogrosZero arrête de réflechir... (voir application en arrière plan)
-> Utiliser 1 thread pour gérer l'affichage tout seul
-> Undo doit retirer le coup du PGN aussi
-> Afficher les textes avec des différentes couleurs pour que ça soit plus facile à lire
-> Défiler la variante quand on met la souris dessus
-> Eviter de recalculer les flèches à chaque fois (et les paramètres de Monte-Carlo)
-> Afficher quand Grogros est lancé dans sa réflexion
-> Comme Nibbler, quand on clique sur le bout d'une flèche de Monte-Carlo, joue le coup
-> Montrer sur l'échiquier quand la position est mate (ou pate, ou autre condition de fin de partie)
-> Faire un reconnaisseur de position automatique
-> Unload les images, textures etc... pour vider la RAM?
-> Pouvoir changer les paramètres de l'IA dans l'UI
-> Ajouter des options/menus
-> Pouvoir changer le temps des joueurs
-> Pouvoir sauvegarder les parties entières dans un fichier (qui s'incrémente), pour garder une trace de toutes les parties jouées
-> Analyses de MC : montrer le chemin qui mène à la meilleure éval, puis celle qui mène au jeu qui va être joué
-> Importation de position / nouvelle position -> update les noms et temps
-> Passer le temps des joueurs dans la GUI plutôt que dans les plateaux?
-> Pouvoir grab le slider, ou cliquer pour changer sa place
-> Faire des batailles entre différents paramètres d'évaluation pour voir la meilleure config -> Retour des batailles de NN?
-> Importation depuis un PGN
-> Afficher pour chaque coup auquel l'ordi réfléchit : la ligne correspondante, ainsi que la position finale avec son évaluation
-> Afficher sur le PGN la reflexion de GrogrosZero
-> Pouvoir changer le nombre de noeuds de l'IA dans la GUI... ou la profondeur de Grogrosfish
-> Pouvoir reset le temps
-> Problème avec les noms quand on les change : parfois ils ne s'affichent plus
-> Parfois l'utilisation des réseaux de neurones bug
-> Nd2f3 -> Ndf3? pas facile à faire
-> Quand les flèches ne sont pas affichées, afficher les touches?
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
-> +/- mats : en fonction des couleurs, ou du joueur qui joue??
-> Trouver une meilleure police de texte, qui prenne en compte les minuscules et majuscules (et soit un peu plus petite)
-> Fins de parties : message + son
-> +M7 -> #-7 pour les noirs? .. bof
-> Barre d'éval : barre pour l'évaluation du coup le plus recherché par l'IA? ou éval du "meilleur coup"?
-> Mettre le screenshot dans le presse-papier?
-> Faire un readme
-> Faire un truc pour montrer la menace (changer le trait du joueur)
-> CtrlN doit effacer tout le PGN... parfois ça bug
-> Mieux voir les mini-pièces...
-> Pouvoir éditer les positions
-> PARALLELISER L'AFFICHAGE !! ça lag beaucoup trop !!!
-> Refaire les pre-moves depuis zero (et ajouter la possibilité d'en faire plusieurs)
-> Mettre la couleur de la fenête en sombre
-> Faire un arbre pour la partie actuelle analysée, pour pouvoir avancer ou reculer dedans, faire une nouvelle variante...
-> Revoir les font_spacing etc... en faire des fonctions pour rendre le code plus lisible
-> Trop de flèches = crash
-> On ne peut pas retirer les flèches
-> Lignes de bézier et cercles pas très beaux
-> Nouveaux bruits de pièces plus "soft" + bruit d'ambiance?
-> Montrer toute la variante calculée avec des flèches (d'une couleur spéciale)
-> Temps buggé? parfois lancé que d'un côté? ou alors c'est juste GrogrosFish qui bugge
-> Thread : bug... parfois les coups joués ne sont pas les bons
-> Re foncer le noir des pièces?
-> Ajout du titre BOT : [WhiteTitle "BOT"]
-> Afficher quand-même la barre d'éval même si GrogrosZero est arrêté?
-> Pourquoi dans certaines variantes, l'éval ne s'affiche pas à la fin??
-> Faut-il être plus sûr sur les re-captures?
-> Il va sûrement manquer des delete quelque part?
-> Dans les .h, remetre les noms des arguments?
-> Rajouter des pre-moves pour Grogros si c'est un coup forcé en face
-> Pourquoi c'est lent de changer  la taille de la GUI?


----- Réseaux de neurones -----

-> Faut t-il de la symétrie dans le réseau de neurone? (car sinon il évalue pas de la même manière les blancs et les noirs)



----- Evaluations incorrectes + corections à ajouter -----

-> 5rk1/r3npbp/2p2np1/2N1p3/2B1P1P1/1P2BP2/b1P4P/2KR2NR b - - 2 19 : mobility+ / outpost
-> 2rqr1k1/p2n1ppp/1p3n2/2pP4/1bP2Pb1/2NQ2PP/PB2N1B1/R4RK1 b - - 0 16 : préfère 2rqr1k1/p2n1ppp/1p6/2pP4/1bP2Pb1/2NQ4/PB2N1B1/R4RK1 w - - 0 19 que 2rqr1k1/p2n1ppp/1p3n2/2pP4/1bP2P2/3Q2PP/PB2N1B1/R4RK1 b - - 0 17
-> 8/pppbn2r/3p4/4k1p1/1P2P3/P1P1RP2/6P1/3R2K1 b - - 1 27


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
rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
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


*/




// Fonction qui permet de tester le temps que prend une fonction
void testA() {
    for (int i = 0; i < 10000; i++)
        cout << "test" << i << endl;
}


// Fonction qui permet de tester le temps que prend une fonction
void testB() {
    cout << "2" << endl;
    for (int i = 0; i < 10000; i++)
        cout << "testB" << i << endl;
}


// Fonction qui fait le dessin de la GUI
void gui_draw() {
    if (!_GUI._draw)
        return;
    BeginDrawing();
    _GUI._board.draw();
    EndDrawing();
}



// Main
int main() {


    // Faire une fonction Init pour raylib?

    // Fenêtre resizable
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);

    // Pour ne pas afficher toutes les infos (on peut mettre le log level de 0 à 7 -> 7 = rien)
    SetTraceLogLevel(LOG_WARNING);
      

    // Initialisation de la fenêtre
    InitWindow(_GUI._screen_width, _GUI._screen_height, "Grogros Chess");

    // Initialisation de l'audio
    InitAudioDevice();
    SetMasterVolume(1.0);

    // Nombre d'images par secondes
    SetTargetFPS(fps);

    // Curseur
    //HideCursor();
    SetMouseCursor(3);


    // Variables
    // Board t;
    _all_positions[0] = _GUI._board.simple_position();
    _total_positions = 1;


    // Evaluateur de position
    Evaluator eval_white;
    Evaluator eval_black;

    // Evaluateur pour Monte Carlo
    Evaluator monte_evaluator;

    // Nombre de noeuds max pour le jeu automatique de GrogrosZero
    int grogros_nodes = 3000000;

    // Nombre de noeuds calculés par frame
    // Si c'est sur son tour
    int nodes_per_frame = 250;

    // Sur le tour de l'autre (pour que ça plante moins)
    int nodes_per_user_frame = 50;

    // Valeurs à 0 pour augmenter la vitesse de calcul. A tester vs grogrosfish avec tout d'activé
    eval_white._piece_activity = 0.0f;
    eval_white._attacks = 0.0f;
    eval_white._defenses = 0.0f;
    eval_white._king_safety = 0.0f;
    eval_white._kings_opposition = 0.0f;
    eval_white._pawn_structure = 0.0f;
    eval_white._player_trait = 0.0f;
    eval_white._push = 0.0f;
    eval_white._rook_open = 0.0f;
    eval_white._rook_semi = 0.0f;
    eval_white._piece_positioning = 0.0f;
    eval_white._castling_rights = 0.0f;
    eval_white._square_controls = 0.0f;

    // Paramètres pour l'IA
    int search_depth = 8;
    search_depth = 1;


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
    _GUI._board.reset_timers();
    _GUI._board._pgn = "[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n\n";
    _GUI._board._fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    //printAttributeSizes(_GUI._board);
    //testFunc(_GUI._board);


    // Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
    while (!WindowShouldClose()) {


        // INPUTS      



        // T - Test de thread
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
            // thread th_test(&Board::grogros_zero, &t, &monte_evaluator, 5000000, true, _beta, _k_add, false, 0, nullptr); // Marche presque... !
            // th_test.detach();

            _GUI.new_bind_game();
            /*cout << _GUI._board.quiescence(&monte_evaluator, -10000000, +10000000) * _GUI._board._color << endl;
            cout << _GUI._board._evaluation << endl;*/
        }

        // CTRL-T - Cherche le plateau de chess.com sur l'écran
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
            cout << "looking for chess.com chessboard..." << endl;
            locate_chessboard(_GUI._binding_left, _GUI._binding_top, _GUI._binding_right, _GUI._binding_bottom);
            printf("Top-Left: (%d, %d)\n", _GUI._binding_left, _GUI._binding_top);
            printf("Bottom-Right: (%d, %d)\n", _GUI._binding_right, _GUI._binding_bottom);
        }

        // LCTRL-A - BInding full (binding chess.com)
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
            _GUI._binding_full = !_GUI._binding_full;
        }

        // LCTRL-Q - Mode de jeu automatique (binding chess.com) -> Check le binding seulement sur les coups de l'adversaire
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
            _GUI._binding_solo = !_GUI._binding_solo;
            _GUI._click_bind = !_GUI._click_bind;
        }


        // Changements de la taille de la fenêtre
        if (IsWindowResized()) {
            get_window_size();
            load_resources(); // Sinon ça devient flou
            resize_gui();
        }

        // S - Save FEN dans data/text.txt
        if (IsKeyPressed(KEY_S)) {
            _GUI._board.to_fen();
            SaveFileText("data/test.txt", (char*)_GUI._board._fen.c_str());
            cout << "saved FEN : " << _GUI._board._fen << endl;
        }

        // L - Load FEN dans data/text.txt
        if (IsKeyPressed(KEY_L)) {
            string fen = LoadFileText("data/test.txt");
            _GUI._board.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // F - Retourne le plateau
        if (IsKeyPressed(KEY_F)) {
            switch_orientation();
        }

        // LCTRL-N - Recommencer une partie
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
            _GUI._board.restart();
        }

        // Utilisation du réseau de neurones
        // if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
        //     use_neural_network = !use_neural_network;
        // }

        // C - Copie dans le clipboard du PGN
        if (IsKeyPressed(KEY_C)) {
            SetClipboardText(_GUI._board._pgn.c_str());
            cout << "copied PGN : \n" << _GUI._board._pgn << endl;
        }

        // X - Copie dans le clipboard du FEN
        if (IsKeyPressed(KEY_X)) {
            _GUI._board.to_fen();
            SetClipboardText(_GUI._board._fen.c_str());
            cout << "copied FEN : " << _GUI._board._fen << endl;
        }

        // V - Colle le FEN du clipboard (le charge)
        if (IsKeyPressed(KEY_V)) {
            string fen = GetClipboardText();
            _GUI._board.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // // Colle le PGN du clipboard (le charge)
        // if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        //     string pgn = GetClipboardText();
        //     _GUI._board.from_pgn(pgn);
        //     cout << "loaded PGN : " << pgn << endl;
        // }

        // A - Analyse de partie sur chess.com (A)
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
            OpenURL("https://www.chess.com/analysis");
        }

        // A - Analyse de partie sur chess.com en direct, par GrogrosZero
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
            _GUI.new_bind_analysis();
        }
            
        // TAB - Screenshot
        if (IsKeyPressed(KEY_TAB)) {
            string screenshot_name = "resources/screenshots/" + to_string(time(0)) + ".png";
            cout << "Screenshot : " << screenshot_name << endl;
            TakeScreenshot(screenshot_name.c_str());

            // Mettre le screenshot dans le presse-papier?
        }

        // D - Dessine ou non
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
            _GUI._draw = false;
        }
            
        // B - Création du buffer
        if (IsKeyPressed(KEY_B)) {
            cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
            _monte_buffer.init();
        }
        
        // G - GrogrosZero
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            // ALT - Sans le calcul des mats/pats
            if (IsKeyDown(KEY_LEFT_ALT))
                _GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, false, _beta, _k_add);
            else {
                // LSHIFT - Utilisation du réseau de neurones
                if (IsKeyDown(KEY_LEFT_SHIFT))
                    //_GUI._board.grogros_zero(nullptr, nodes_per_frame, true, _beta, _k_add, false, 0, &grogros_network);
                    false;
                else
                    _GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, true, _beta, _k_add);
            }
                
        }

        // LCTRL-G - Lancement de GrogrosZero en recherche automatique
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            _GUI._grogros_analysis = true;
        }

        // Espace - GrogrosZero 1 noeud : DEBUG
        if (IsKeyPressed(KEY_SPACE)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            _GUI._board.grogros_zero(&monte_evaluator, 1, true, _beta, _k_add);
        }

        // LCTRL-H - Arrêt de la recherche automatique de GrogrosZero 
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
            _GUI._grogros_analysis = false;
        }

        // H - Déffichage/Affichage des flèches, Affichage/Désaffichage des contrôles
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
            switch_arrow_drawing();
        }

        // R - Réinitialisation des timers
        if (IsKeyPressed(KEY_R)) {
            _GUI._board.reset_timers();
            _GUI._time = false;
        }

        // Suppr. - Supprime les reflexions de GrogrosZero
        if (IsKeyPressed(KEY_DELETE)) {
            _GUI._board.reset_all(true, true);
        }

        // D - Affichage dans la console de tous les coups légaux de la position
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
            _GUI._draw = true;
            _GUI._board.display_moves(true);
        }

        // T - Fonction de test
        if (IsKeyPressed(KEY_T)) {
        }

        // E - Évalue la position et renvoie les composantes dans la console
        if (IsKeyPressed(KEY_E)) {
            _GUI._board.evaluate_int(&monte_evaluator, true, true);
            cout << "Evaluation : \n" << eval_components << endl;
        }
            
        // U - Undo de dernier coup joué
        IsKeyPressed(KEY_U) && _GUI._board.undo();

        // Modification des paramètres de recherche de GrogrosZero
        IsKeyPressed(KEY_KP_ADD) && (_beta *= 1.1f);
        IsKeyPressed(KEY_KP_SUBTRACT) && (_beta /= 1.1f);
        IsKeyPressed(KEY_KP_MULTIPLY) && (_k_add *= 1.25f);
        IsKeyPressed(KEY_KP_DIVIDE) && (_k_add /= 1.25f);

        // R-Return - Reset aux valeurs initiales
        if (IsKeyPressed(KEY_KP_ENTER)) {
            _beta = 0.05f;
            _k_add = 25.0f;
        }

        // 1 - Recherche en profondeur extrême
        if (IsKeyPressed(KEY_ONE)) {
            _beta = 0.5f;
            _k_add = 0.0f;
        }

        // 2 - Recherche en profondeur
        if (IsKeyPressed(KEY_TWO)) {
            _beta = 0.2f;
            _k_add = 10.0f;
        }

        // 3 - Recherche large
        if (IsKeyPressed(KEY_THREE)) {
            _beta = 0.01f;
            _k_add = 100.0f;
        }

        // 4 - Recherche de mat
        if (IsKeyPressed(KEY_FOUR)) {
            _beta = 0.005f;
            _k_add = 2500.0f;
        }

        // 5 - Recherche de victoire en endgame
        if (IsKeyPressed(KEY_FIVE)) {
            _beta = 0.05f;
            _k_add = 5000.0f;
        }

        // P - Joue le coup recommandé par l'algorithme de GrogrosZero
        if (!IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P)) {
            if (_GUI._board._tested_moves > 0)
                ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && _GUI._board.play_monte_carlo_move_keep(_GUI._board.best_monte_carlo_move(), true, true, false, false);
            else
                cout << "no more moves are in memory" << endl;
        }

        // LShift-P - Joue les coups recommandés par l'algorithme de GrogrosZero
        if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_P)) {
            if (_GUI._board._tested_moves > 0)
                ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && _GUI._board.play_monte_carlo_move_keep(_GUI._board.best_monte_carlo_move(), true, true, false, false);
            else
                cout << "no more moves are in memory" << endl;
        }
        
        // Return - Lancement et arrêt du temps
        if (IsKeyPressed(KEY_ENTER)) {
            if (_GUI._time)
                _GUI._board.stop_time();
            else
                _GUI._board.start_time();
        }
        

        // UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces blanches
        if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
            if (_GUI._white_player.substr(0, 11) == "GrogrosFish")
                _GUI._white_player = "White";
            else
                _GUI._white_player = "GrogrosFish (depth " + to_string(search_depth) + ")";

            _GUI._board.add_names_to_pgn();  
        }

        // UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces noires
        if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
            if (_GUI._black_player.substr(0, 11) == "GrogrosFish")
                _GUI._black_player = "Black";
            else
                _GUI._black_player = "GrogrosFish (depth " + to_string(search_depth) + ")";

            _GUI._board.add_names_to_pgn();     
        }

        // CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces blanches
        if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
            if (_GUI._white_player.substr(0, 11) == "GrogrosZero")
                _GUI._white_player = "White";
            else
                _GUI._white_player = "GrogrosZero";

            _GUI._board.add_names_to_pgn();
        }

        // CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces noires
        if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
            if (_GUI._black_player.substr(0, 11) == "GrogrosZero")
                _GUI._black_player = "Black";
            else
                _GUI._black_player = "GrogrosZero";

            _GUI._board.add_names_to_pgn();
        }


        // Fin de partie (à reset aussi...) (le son ne se lance pas...)
        // Calculer la fin de la partie ici une fois, pour éviter de la refaire?

        // Plus de temps... (en faire une fonction)
        // if (_GUI._board._time) {


        //     if (_GUI._board._time_black < 0) {
        //         _GUI._board._time = false;
        //         _GUI._board._time_black = 0;
        //         play_end_sound();
        //         _GUI._board._is_game_over = true;
        //         cout << "White won on time" << endl; // Pas toujours vrai car il peut y avoir des manques de matériel
        //     }
        //     if (_GUI._board._time_white < 0) {
        //         _GUI._board._time = false;
        //         _GUI._board._time_white = 0;
        //         play_end_sound();
        //         _GUI._board._is_game_over = true;
        //         cout << "Black won on time" << endl;
        //     }

           
        // }


        // Jeu des IA

        // Fait jouer l'IA automatiquement en fonction des paramètres
        if (!_GUI._board._is_game_over && _GUI._board.is_mate() == -1 && _GUI._board.game_over() == 0) {

            // GrogrosZero

            // Quand c'est son tour
            if ((_GUI._board._player && _GUI._white_player.substr(0, 11) == "GrogrosZero") || (!_GUI._board._player && _GUI._black_player.substr(0, 11) == "GrogrosZero")) {
                if (!_monte_buffer._init)
                    _monte_buffer.init();

                // Grogros doit gérer son temps
                if (_GUI._time) {
                    // Nombre de noeuds que Grogros doit calculer (en fonction des contraintes de temps)
                    static const int supposed_grogros_speed = 2500; // En supposant que Grogros va à plus de 20k noeuds par seconde
                    int tot_nodes = _GUI._board.total_nodes();
                    float best_move_percentage = tot_nodes == 0 ? 0.05f : (float)_GUI._board._nodes_children[_GUI._board.best_monte_carlo_move()] / (float)_GUI._board.total_nodes();
                    int max_move_time = _GUI._board._player ? 
                        time_to_play_move(_GUI._time_white, _GUI._time_black, 0.05f * (1.0f - best_move_percentage)) :
                        time_to_play_move(_GUI._time_black, _GUI._time_white, 0.05f * (1.0f - best_move_percentage));
                    int grogros_timed_nodes = min(nodes_per_frame, supposed_grogros_speed * max_move_time / 1000);
                    _GUI._board.grogros_zero(&monte_evaluator, min(!_GUI._time ? nodes_per_frame : grogros_timed_nodes, grogros_nodes - _GUI._board.total_nodes()), true, _beta, _k_add);
                    if (_GUI._board._time_monte_carlo >= max_move_time)
                        ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && _GUI._board.play_monte_carlo_move_keep(_GUI._board.best_monte_carlo_move(), true, true, false, false);
                
                
                }
                else
                    _GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, true, _beta, _k_add);
            }

            // Quand c'est pas son tour
            if ((!_GUI._board._player && _GUI._white_player.substr(0, 12) == "GrogrosZero") || (_GUI._board._player && _GUI._black_player.substr(0, 12) == "GrogrosZero")) {
                if (!_monte_buffer._init)
                    _monte_buffer.init();
                _GUI._board.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, _beta, _k_add);
            }

            // Mode analyse
            if (_GUI._grogros_analysis) {
                if (!_monte_buffer._init)
                    _monte_buffer.init();

                if (!is_playing()) 
                    _GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, true, _beta, _k_add);
                else
                    _GUI._board.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, _beta, _k_add); // Pour que ça ne lag pas pour l'utilisateur
            }


            // GrogrosFish (seulement lorsque c'est son tour)
            if (_GUI._board._player && _GUI._white_player.substr(0, 11) == "GrogrosFish")
                _GUI._board.grogrosfish(search_depth, &eval_white, true);

            if (!_GUI._board._player && _GUI._black_player.substr(0, 11) == "GrogrosFish")
                _GUI._board.grogrosfish(search_depth, &eval_black, true);

        }

        // Si la partie est terminée
        else {
            if (!main_game_over) {
                _GUI._time = false;
                _GUI._board.display_pgn();
                main_game_over = true;
            }
            
        }


        // Jeu automatique sur chess.com
        if (_GUI._binding_full || (_GUI._binding_solo && get_board_orientation() != _GUI._board._player)) {

            // Le fait à chaque intervalle de temps 'binding_interval_check'
            if (clock() - _GUI._last_binding_check > _GUI._binding_interval_check) {

                // Coup joué sur le plateau
                _GUI._binding_move = get_board_move(_GUI._binding_left, _GUI._binding_top, _GUI._binding_right, _GUI._binding_bottom, get_board_orientation());

                // Vérifie que le coup est légal avant de le jouer
                for (int i = 0; i < _GUI._board._got_moves; i++) {
                    if (_GUI._board._moves[4 * i] == _GUI._binding_move[0] && _GUI._board._moves[4 * i + 1] == _GUI._binding_move[1] && _GUI._board._moves[4 * i + 2] == _GUI._binding_move[2] && _GUI._board._moves[4 * i + 3] == _GUI._binding_move[3]) {
                        _GUI._board.play_move_sound(_GUI._binding_move[0], _GUI._binding_move[1], _GUI._binding_move[2], _GUI._binding_move[3]);
                        _GUI._board.play_monte_carlo_move_keep(i, true, true, true, true);
                        break;
                    }
                }

                _GUI._last_binding_check = clock();
            }

            
        }

        
        gui_draw();



    }

    // Fermeture de la fenêtre
    CloseWindow();


    return 0;

}
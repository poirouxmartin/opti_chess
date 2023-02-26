#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"
#include <thread>
// #include <Windows.h>


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
    - Cases noires/blanches
    - Contrôle des cases
    - Cases faibles
    - Pions arrierés/faibles
    - Initiative -> A améliorer : fort dans les positions d'attaque?
    - Fous de couleurs opposées : favorisent l'attaque, mais en finale -> draw
    - Contrôle du centre
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
-> 3k3r/2p1b1pp/p1p2p2/3bp3/8/2P1BNP1/PPP2PKP/R7 w - - 3 16 -> +1.15?
-> Mettre les règles de parties nulles et mat en dehors de l'évaluation?
-> Ajouter les pièces protégées/attaquées lors de l'évaluation pour simplifier les calculs de l'IA
-> Gestion du temps : faire en fonction des lignes montantes? Si ça stagne, jouer vite? Si y'a un seul coup -> Jouer instant?
-> Carré du pion en finales
-> Ne comprend pas les finales de bases (du au fait qu'il répète les coups?)
-> Regarder dag chess?
-> Faire un readme pour les contrôles
-> Pour les finales : donner ce qui ne peut pas gagner (seulement une pièce...), les finales théoriques etc..
-> Est-ce normal que l'évaluation soit si instable?
-> Vérifier de partout que j'ai pas confondu les lignes et les colonnes
-> Faire une map des cases attaquées (ça peut rendre plus rapide les tests d'échecs)
-> Tester de re augmenter l'activité des pièces?
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
-> Améliorer la recherche des coups... quand c'est dans les coups mauvais, ça ne fait plus la différence... recherche -> 0% rapidement, et le k_add prend le dessus
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
-> r3r1k1/ppp4q/1n3Q2/3p1P2/5P2/2q5/PpPqN1B1/1K5R w - - 0 26 : NE PLUS REGARDER LES COUPS POURRIS
-> Voir si move_label ralentit tout... (en regardant des mats etc...)
-> r1b2kn1/1p1p1p1r/1p1P3p/1N1N1R2/6p1/8/PP4PP/5RK1 b - - 3 21 : +25, statiquement faut faire qq chose pour le comprendre dans l'évaluation de Grogros (+1.38)
-> GrogrosZero ralentit beaucoup quand les variantes deviennent longues
-> rn1r3k/pp4p1/1b2B3/5Qp1/3P4/P4b2/1P3PPP/6K1 b - - 1 3 : ici après Rxd4, tous les calculs sont perturbés par le fou en l'air en f3... ce qui ralentit la recherche de mat
-> ----> Faire une recherche spécialement de mat, où on prend plus en compte le matériel??
-> Au fil de la réflexion, retirer les coups pourris?? pour augmenter la capacité de stockage...
-> 8/7p/4k1p1/p2Nrp2/2P5/2KB3P/8/8 b - - 2 45
-> utiliser half_moves_count dans l'évaluation pour la faire tendre vers la nulle quand ça augmente??
-> 8/8/7k/8/1p1p1p1p/pPpPpPpP/P1P1P1P1/1K6 w - - 0 1
-> Mettre une variable globale pour la règle des 50 coups (pour la passer à moins, si besoin)
-> Rechercher large au début, puis serré après??
-> 5rk1/pp4pp/2pb4/3p3q/B2Pp1bP/2N1B3/PPP3P1/R3Q1K1 b - - 0 2 : regarder les coups les plus offensifs, pas Fe6...
-> Re augmenter l'activité? car à elle seule, elle dépasse rarement 0.25...
-> Ajouter les colonnes ouvertes sur le roi... : attention aux fishing poles (r1bq1rk1/ppppnpp1/2n4p/2bNp1N1/2B1P2P/2P5/PP1P1PP1/R1BQK2R b KQ - 0 1)
-> r2qr1k1/ppp2ppp/1n1pb3/4P3/3P4/1BN5/PP3PPP/R2QR1K1 w - - 6 9 : d5 doit être évident
-> Sait pas faire les greek gifts : r1bq1rk1/ppppbppp/2n1p3/3nP3/3P3P/3B1N2/PPP2PP1/RNBQK2R w KQ - 1 7
-> Echanger les pièces quand on est matériellement gagnant
-> Echanger les pièces quand on a le roi faible, ou les pièces moins actives
-> r4rk1/p2nbpp1/1qp4p/1p1pPB2/3P1B2/1P3P2/2Q2P1P/R4RK1 w - - 1 17 : pourquoi il regarde pas Fxd7?...
-> Changer un peu l'algo, pour que ça ne joue pas forcément le coup auquel il a le plus réflechi, mais un autre si il semble être meilleur (quand c'est mat, facile, mais sinon, comment savoir?)
-> switch() = lent : à changer
-> 2r2rk1/pp1q1pp1/2np1b1p/3NpN2/4P2P/P7/1PP2PP1/R2QK2R b KQ - 2 18 : Positionnement des pièces mieux pour les noirs??? ça va pas... (sûrement à cause du roi?)
-> rnbqkb1r/ppp2ppp/4p3/4P3/3Pp3/8/PPP2PPP/R1BQKBNR w KQkq - 0 6 : le pion e4 est pourri
-> Attaquer les faiblesses
-> 2r1r1k1/p2n1ppp/1p6/2pP4/1bP2P2/2N3N1/PB6/R4RK1 w - - 0 23 = égal????? un pièce de moins...
-> rnbq3r/pppp1kpp/5n2/6N1/3PP3/6b1/PPP4p/RNBQ1R1K b - - 2 10 : incompréhension de la position
-> Quand Grogros joue un coup auquel il n'avait pas pensé.. les évaluations déscendent pour les coups, 1 par 1... comment faire pour que tout descende en même temps?
-> Equilibrer les tours sur les colonnes ouvertes. Moins fortes en endgame? Plus forte si y'a qu'une seule colonne ouverte
-> GrogrosZero sait plus mater roi + tour vs roi, ou roi + dame vs roi
-> 8/6R1/8/8/8/8/5K1k/8 w - - 38 20 : roi noir + safe? XDD ----> Beaucoup de choses à revoir dans cette fonction get_king_safety()...
-> r2qr1k1/1pp2ppp/pbnpbn2/4p3/3PP3/2P2N2/PPB2PPP/RNBQR1K1 w - - 1 13 : d5???!!
-> Pour king_safety, il faut absolument prendre en compte les pièces qui peuvent l'attaquer, sinon... r1bq4/ppp2kpp/2np1n2/2b1p3/4P3/3P4/PPP2PPP/RNBQ1RK1 w - - 0 9 = +1.1 en king_safety
-> Vérifier que le matériel fonctionne bien (pas d'erreurs de calcul??)
-> 5r2/3R1k1b/1Bp4p/1p2p2P/7P/b1N2KN1/8/8 b - - 7 66 : ça n'évalue pas la position???
-> Regler tous les *100 et les /100 dans les évaluations
-> L'éval statique en position symétrique sera toujours nulle ?? (modulo le trait du joueur)
-> Distinguer positionnement (-> espace?) et développement des pièces (pour améliorer king_safety())
-> Puissance de la paire de fou qui dépend du moment de la partie? Qui dépend si y'a encore les dames pour compenser?


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
-> Ne plus afficher "INFO:" de raylib dans la console
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
-> Afficher les composantes de l'évaluation sur la GUI
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
-> Police avec des minuscules...
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
-> La GUI lag à cause du calcul de variantes??
-> Thread : bug... parfois les coups joués ne sont pas les bons
-> Re foncer le noir des pièces?


----- Réseaux de neurones -----

-> Faut t-il de la symétrie dans le réseau de neurone? (car sinon il évalue pas de la même manière les blancs et les noirs)

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
void gui_draw(bool d) {
    BeginDrawing();
    if (d)
        t.draw();
    EndDrawing();
}



// Main
int main() {


    // Faire une fonction Init pour raylib?

    // Fenêtre resizable
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
      

    // Initialisation de la fenêtre
    InitWindow(screen_width, screen_height, "Grogros Chess");

    // Initialisation de l'audio
    InitAudioDevice();
    SetMasterVolume(1.0);

    // Nombre d'images par secondes
    SetTargetFPS(fps);

    // Curseur
    // HideCursor();
    SetMouseCursor(3);


    // Variables
    // Board t;
    _all_positions[0] = t.simple_position();
    _total_positions = 1;


    // Evaluateur de position
    Evaluator eval_white;
    Evaluator eval_black;

    // Evaluateur pour Monte Carlo
    Evaluator monte_evaluator;
    monte_evaluator._piece_activity = 0.05; // 0.04
    monte_evaluator._piece_positioning = 0.007; // beta = 0.035 // Pos = 0.013
    // monte_evaluator._piece_positioning = 0.01; // Pour tester http://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&start=19
    monte_evaluator._king_safety = 0.004; // Il faut régler la fonction... avec les pièces autour, s'il est au milieu du plateau...
    monte_evaluator._castling_rights = 0.3;
    // monte_evaluator._attacks = 0.0;
    // monte_evaluator._piece_activity = 0.10; // En test pour la NJV

    // Nombre de noeuds max pour le jeu automatique de GrogrosZero
    int grogros_nodes = 3000000;

    // Nombre de noeuds calculés par frame
    // Si c'est sur son tour
    int nodes_per_frame = 2500;

    // Sur le tour de l'autre (pour que ça plante moins)
    int nodes_per_user_frame = 250;

    // Valeurs à 0 pour augmenter la vitesse de calcul. A tester vs grogrosfish avec tout d'activé
    eval_white._piece_activity = 0;
    eval_white._attacks = 0;
    eval_white._defenses = 0;
    eval_white._king_safety = 0;
    eval_white._kings_opposition = 0;
    eval_white._pawn_structure = 0;
    eval_white._player_trait = 0;
    eval_white._push = 0;
    eval_white._rook_open = 0;
    eval_white._rook_semi = 0;
    eval_white._piece_positioning = 0;
    eval_white._castling_rights = 0;

    eval_black._piece_activity = 0.03;

    // IA self play
    bool grogrosfish_play_white = false;
    bool grogrosfish_play_black = false;

    // Paramètres pour l'IA
    int search_depth = 8;
    search_depth = 7;


    // Fin de partie
    bool main_game_over = false;

    // Réseau de neurones
    Network grogros_network;
    grogros_network.generate_random_weights();
    bool use_neural_network = false;


    // Liste de réseaux de neurones pour les tournois
    int n_networks = 5;
    Evaluator **evaluators = new Evaluator*[n_networks];
    for (int i = 0; i < n_networks; i++)
        evaluators[i] = nullptr;
    Network **neural_networks = new Network*[n_networks];
    Network *neural_networks_test = new Network[n_networks];
    for (int i = 0; i < n_networks; i++) {
        neural_networks[i] = &neural_networks_test[i];
        neural_networks[i]->generate_random_weights();
    }

    neural_networks[0] = nullptr;
    evaluators[0] = &monte_evaluator;


    // Met les timers en place
    t.reset_timers();

    t._pgn = "[FEN \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\"]\n\n";


    // Test des agents GrogrosZero

    // Liste d'agents
    const int n_agents = 25;
    Agent l_agents[n_agents];
    int generation = 0;
    int max_generations = 100;
    Agent final_agent;

    // Initialisation des agents
    for (int i = 0; i < n_agents; i++) {
        Agent a;
        l_agents[i] = a;
    }

    // Liste de scores pour les tournois
    int *l_scores;


    bool draws = true;



    // Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
    while (!WindowShouldClose()) {


        // INPUTS


        // Test de thread
        if (IsKeyPressed(KEY_T)) {
            thread th_test(&Board::grogros_zero, &t, &monte_evaluator, 5000000, true, _beta, _k_add, false, 0, nullptr); // Marche presque... !
            th_test.detach();
        }


        // Changements de la taille de la fenêtre
        if (IsWindowResized()) {
            get_window_size();
            load_resources(); // Sinon ça devient flou
            resize_gui();
        }

        // S - Save FEN dans data/text.txt
        if (IsKeyPressed(KEY_S)) {
            t.to_fen();
            SaveFileText("data/test.txt", (char*)t._fen.c_str());
            cout << "saved FEN : " << t._fen << endl;
        }

        // L - Load FEN dans data/text.txt
        if (IsKeyPressed(KEY_L)) {
            string fen = LoadFileText("data/test.txt");
            t.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // F - Retourne le plateau
        if (IsKeyPressed(KEY_F)) {
            switch_orientation();
        }

        // LCTRL-N - Recommencer une partie
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
            t.restart();
        }

        // Utilisation du réseau de neurones
        // if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
        //     use_neural_network = !use_neural_network;
        // }

        // C - Copie dans le clipboard du PGN
        if (IsKeyPressed(KEY_C)) {
            SetClipboardText(t._pgn.c_str());
            cout << "copied PGN : \n" << t._pgn << endl;
        }

        // X - Copie dans le clipboard du FEN
        if (IsKeyPressed(KEY_X)) {
            t.to_fen();
            SetClipboardText(t._fen.c_str());
            cout << "copied FEN : " << t._fen << endl;
        }

        // V - Colle le FEN du clipboard (le charge)
        if (IsKeyPressed(KEY_V)) {
            string fen = GetClipboardText();
            t.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // // Colle le PGN du clipboard (le charge)
        // if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        //     string pgn = GetClipboardText();
        //     t.from_pgn(pgn);
        //     cout << "loaded PGN : " << pgn << endl;
        // }

        // A - Analyse de partie sur chess.com (A)
        if (IsKeyPressed(KEY_Q)) {
            OpenURL("https://www.chess.com/analysis");
        }
            
        // TAB - Screenshot
        if (IsKeyPressed(KEY_TAB)) {
            string screenshot_name = "../resources/screenshots/" + to_string(time(0)) + ".png";
            cout << "Screenshot : " << screenshot_name << endl;
            TakeScreenshot(screenshot_name.c_str());

            // Mettre le screenshot dans le presse-papier?
        }

        // D - Dessine ou non
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
            draws = false;
        }
            
        // B - Création du buffer
        if (IsKeyPressed(KEY_B)) {
            cout << "Available memory : " << getTotalSystemMemory() << endl;
            cout << "generating new buffer" << endl;
            _monte_buffer.init();
            // N'affiche pas la bonne taille
            cout << "new buffer, size : "  << sizeof(_monte_buffer) << " bits, length : " << _monte_buffer._length << endl;
            cout << "first free index : " << _monte_buffer.get_first_free_index() << endl;
        }
        
        // G - GrogrosZero
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            // ALT - Sans le calcul des mats/pats
            if (IsKeyDown(KEY_LEFT_ALT))
                t.grogros_zero(&monte_evaluator, nodes_per_frame, false, _beta, _k_add);
            else {
                // LSHIFT - Utilisation du réseau de neurones
                if (IsKeyDown(KEY_LEFT_SHIFT))
                    t.grogros_zero(nullptr, nodes_per_frame, true, _beta, _k_add, false, 0, &grogros_network);
                else
                    t.grogros_zero(&monte_evaluator, nodes_per_frame, true, _beta, _k_add);
            }
                
        }

        // LCTRL-G - Lancement de GrogrosZero en recherche automatique
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            grogros_auto = set_grogros_auto(true); // C'est vraiment immonde, mais j'ai pas trouvé comment mieux faire... 
        }

        // LCTRL-H - Arrêt de la recherche automatique de GrogrosZero 
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
            grogros_auto = set_grogros_auto(false);         
        }

        // H - Déffichage/Affichage des flèches, Affichage/Désaffichage des contrôles
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
            switch_arrow_drawing();
        }

        // R - Réinitialisation des timers
        if (IsKeyPressed(KEY_R)) {
            t.reset_timers();
            t._time = false;
        }

        // Suppr. - Supprime les reflexions de GrogrosZero
        if (IsKeyPressed(KEY_DELETE)) {
            t.reset_all(true, true);
        }

        // D - Affichage dans la console de tous les coups légaux de la position
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
            draws = true;
            t.display_moves(true);
        }

        // T - Fonction de test
        if (IsKeyPressed(KEY_T)) {
        }

        // E - Évalue la position et renvoie les composantes dans la console
        if (IsKeyPressed(KEY_E)) {
            t.evaluate_int(&monte_evaluator, true, true);
            cout << "Evaluation : \n" << eval_components << endl;
        }
            
        // U - Undo de dernier coup joué
        IsKeyPressed(KEY_U) && t.undo(); 

        // Modification des paramètres de recherche de GrogrosZero
        IsKeyPressed(KEY_KP_ADD) && (_beta *= 1.1);
        IsKeyPressed(KEY_KP_SUBTRACT) && (_beta /= 1.1);
        IsKeyPressed(KEY_KP_MULTIPLY) && (_k_add *= 1.25);
        IsKeyPressed(KEY_KP_DIVIDE) && (_k_add /= 1.25);

        // R-Return - Reset aux valeurs initiales
        if (IsKeyPressed(KEY_KP_ENTER)) {
            _beta = 0.05;
            _k_add = 25;
        }

        // 1 - Recherche en profondeur extrême
        if (IsKeyPressed(KEY_ONE)) {
            _beta = 0.5;
            _k_add = 0;
        }

        // 2 - Recherche en profondeur
        if (IsKeyPressed(KEY_TWO)) {
            _beta = 0.2;
            _k_add = 10;
        }

        // 3 - Recherche large
        if (IsKeyPressed(KEY_THREE)) {
            _beta = 0.01;
            _k_add = 100;
        }

        // 4 - Recherche de mat
        if (IsKeyPressed(KEY_FOUR)) {
            _beta = 0.005;
            _k_add = 2500;
        }

        // 5 - Recherche de victoire en endgame
        if (IsKeyPressed(KEY_FIVE)) {
            _beta = 0.05;
            _k_add = 5000;
        }

        // P - Joue le coup recommandé par l'algorithme de GrogrosZero
        if (IsKeyPressed(KEY_P)) {
            if (t._tested_moves > 0)
                t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true, true);
            else
                cout << "no more moves are in memory" << endl;
        }
        
        // Return - Lancement et arrêt du temps
        if (IsKeyPressed(KEY_ENTER)) {
            if (t._time)
                t.stop_time();
            else
                t.start_time();
        }
        

        // UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces blanches
        if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
            grogrosfish_play_white = !grogrosfish_play_white;
            grogroszero_play_white = false;
            if (grogrosfish_play_white) {
                t._white_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
                t.add_names_to_pgn();
            }  
            else {
                t._white_player = (char*)"White";
                t.add_names_to_pgn();
            }
                
        }

        // UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces noires
        if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
            grogrosfish_play_black = !grogrosfish_play_black;
            grogroszero_play_black = false;
            if (grogrosfish_play_black) {
                t._black_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
                t.add_names_to_pgn();
            }        
            else {
                t._black_player = (char*)"Black";
                t.add_names_to_pgn();
            }
                
        }

        // CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces blanches
        if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
            grogroszero_play_white = !grogroszero_play_white;
            grogrosfish_play_white = false;
            if (grogroszero_play_white) {
                t._white_player = "GrogrosZero (max " + int_to_round_string(grogros_nodes) + " nodes)";
                t.add_names_to_pgn();
            }
            else {
                t._white_player = (char*)"White";
                t.add_names_to_pgn();
            }
                
        }

        // CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces noires
        if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
            grogroszero_play_black = !grogroszero_play_black;
            grogrosfish_play_black = false;
            if (grogroszero_play_black) {
                t._black_player = "GrogrosZero (max " + int_to_round_string(grogros_nodes) + " nodes)";
                t.add_names_to_pgn();
            }
                
            else {
                t._black_player = (char*)"Black";
                t.add_names_to_pgn();
            }

        }


        // Fin de partie (à reset aussi...) (le son ne se lance pas...)
        // Calculer la fin de la partie ici une fois, pour éviter de la refaire?

        // Plus de temps... (en faire une fonction)
        if (t._time) {


            if (t._time_black < 0) {
                t._time = false;
                t._time_black = 0;
                play_end_sound();
                t._is_game_over = true;
                cout << "White won on time" << endl; // Pas toujours vrai car il peut y avoir des manques de matériel
            }
            if (t._time_white < 0) {
                t._time = false;
                t._time_white = 0;
                play_end_sound();
                t._is_game_over = true;
                cout << "Black won on time" << endl;
            }

           
        }


        // Calcul en mode auto
        if (grogros_auto && !t._is_game_over && t.is_mate() == -1 && t.game_over() == 0) {
            if (!_monte_buffer._init)
                _monte_buffer.init();

            if (!is_playing()) // Pour que ça ne lag pas pour l'utilisateur
                t.grogros_zero(&monte_evaluator, nodes_per_frame, true, _beta, _k_add);
            else
                t.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, _beta, _k_add);
        }

        

        // Calcul pour les pièces blanches

        // Nombre de noeuds que Grogros doit calculer (en fonction des contraintes de temps)
        // En supposant que Grogros va à plus de 10k noeuds par seconde
        int supposed_grogros_speed = 10000;
        
        
        if (grogroszero_play_white && !t._is_game_over && t.is_mate() == -1 && t.game_over() == 0) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            if (t._player) {
                float tot_nodes = t.total_nodes();
                float best_move_percentage_white = tot_nodes == 0 ? 0.05 :(float)t._nodes_children[t.best_monte_carlo_move()] / (float)t.total_nodes();
                int max_move_time_white = time_to_play_move(t._time_white, t._time_black, 0.05 * (1 - best_move_percentage_white));
                int grogros_timed_nodes_white = min(nodes_per_frame, supposed_grogros_speed * max_move_time_white / 1000);
                t.grogros_zero(&monte_evaluator, min(!t._time ? nodes_per_frame : grogros_timed_nodes_white, grogros_nodes - t.total_nodes()), true, _beta, _k_add);
            }
                
            else
                if (!is_playing() || true) // Pour que ça ne lag pas pour l'utilisateur 
                    t.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, _beta, _k_add);     
        }

        

        // Calcul pour les pièces noires
        if (grogroszero_play_black && !t._is_game_over && t.is_mate() == -1 && t.game_over() == 0) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            if (!t._player) {
                float tot_nodes = t.total_nodes();
                float best_move_percentage_black = tot_nodes == 0 ? 0.05 :(float)t._nodes_children[t.best_monte_carlo_move()] / (float)t.total_nodes();
                int max_move_time_black = time_to_play_move(t._time_black, t._time_white, 0.05 * (1 - best_move_percentage_black));
                int grogros_timed_nodes_black = min(nodes_per_frame, supposed_grogros_speed * max_move_time_black / 1000);
                t.grogros_zero(&monte_evaluator, min(!t._time ? nodes_per_frame : grogros_timed_nodes_black, grogros_nodes - t.total_nodes()), true, _beta, _k_add);
            }
                
            else
                if (!is_playing() || true) // Pour que ça ne lag pas pour l'utilisateur
                    t.grogros_zero(&monte_evaluator, nodes_per_user_frame, true, _beta, _k_add);     
        }


        // Joue les coups selon grogros en fonction de la reflexion actuelle
        if (!t._is_game_over && t.is_mate() == -1 && t.game_over() == 0 && ((grogroszero_play_white && t._player) || (grogroszero_play_black && !t._player))) {
            if (t._time) {
                int max_move_time;
                float best_move_percentage = (float)t._nodes_children[t.best_monte_carlo_move()] / (float)t.total_nodes();
                if (t._player)
                    max_move_time = time_to_play_move(t._time_white, t._time_black, 0.05 * (1 - best_move_percentage));
                else
                    max_move_time = time_to_play_move(t._time_black, t._time_white, 0.05 * (1 - best_move_percentage));
                // Temps que Grogros a pris pour ce tour là
                // clock_t grogros_time = clock();
                if (t._time_monte_carlo >= max_move_time)
                    t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true, true);
            }
                
            if (t.total_nodes() >= grogros_nodes)
                t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true, true);

        }
            

        
        // Fait jouer l'IA automatiquement en fonction des paramètres
        if (!t._is_game_over && t.is_mate() == -1 && t.game_over() == 0) {
            if (t._player) {
                if (grogrosfish_play_white)
                    t.grogrosfish(search_depth, &eval_white, true);
            } 
            else {
                if (grogrosfish_play_black)
                    t.grogrosfish(search_depth, &eval_black, true);
            }
                
        }

        // Si la partie est terminée
        else {
            if (!main_game_over) {
                t._time = false;
                t.display_pgn();
                main_game_over = true;
            }
            
        }

        // // Dessins
        // BeginDrawing();

        // // Dessin du plateau
        // t.draw();
            
        // // Fin de la zone de dessin
        // EndDrawing();

        gui_draw(draws);
        // cout << GetFPS() << endl;



    }

    // Fermeture de la fenêtre
    CloseWindow();


    return 0;

}
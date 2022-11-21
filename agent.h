class Agent {
    public:

        // Attributs



        // Réseau de neurones

        // Middle layers à ajouter


        // Input :
        // 1-768 -> 0/1 = piece (1-12) sur case (0-64)
        // ... à ajouter
        // 769-772 -> roques.
        // 773 -> en passant?
        int _input_size = 768;
        int _input_layer[768];

        // Output
        int _output = 0;


        // Scores pour les tournois
        int _score = 0;

        int _victories = 0;
        int _draws = 0;
        int _losses = 0;


        // 100 de base, de façon arbitraire
        int _elo = 500;
        int _games = 0;

        int _generation = 0;


        // Constructeur par défaut
        Agent();

        // Constructeur par copie
        Agent(Agent&);
        


        // Fonctions


        // Nouvel agent par défaut
        void init();

        // Copie les données d'un agent
        void copy_data(Agent&, bool);

        // Fonction qui applique une mutation à un agent
        void mutation(float);

        // Fonction qui fait un crossover entre deux agents pour former cet agent
        void crossover(Agent, Agent);


};



// Fonction qui permet de calculer l'élo résultant des deux joueurs après une partie
void calc_elo(Agent&, Agent&, float);


// Fonction qui renvoie parmi une liste d'entiers, renvoie deux index aléatoires, avec des probabilité variantes, en fonction de la grandeur des nombre correspondant à ces index
void pick_random_parents(int*, int, int[]);


// Fonction qui crée la prochaine génération d'agents, en fonctions de leurs scores, un taux de nouveau random, un taux de mutation, un taux de crossover... (et on garde le premier agent)
void next_generation(Agent*, int, float, float, float);


#include "agent.h"
#include "raylib.h"
#include <iostream>
#include <algorithm>
#include "math.h"
using namespace std;

// Constructeur par défaut
Agent::Agent() {

    // Paramètres aléatoires pour les input
    for (int i = 0; i < 768; i++)
        _input_layer[i] = GetRandomValue(-1000, 1000);

}



// Constructeur par copie
Agent::Agent(Agent &a) {

    // Copie de la couche d'entrée
    for (int i = 0; i < 768; i++)
        _input_layer[i] = a._input_layer[i];

    _generation = a._generation;

}


// Nouvel agent par défaut
void Agent::init() {
    // Paramètres aléatoires pour les input
    for (int i = 0; i < 768; i++)
        _input_layer[i] = GetRandomValue(-1000, 1000);

    // Output
    _output = 0;


    // Scores pour les tournois
    _score = 0;

    _victories = 0;
    _draws = 0;
    _losses = 0;


}



// Copie les données d'un agent
void Agent::copy_data(Agent &a, bool self = false) {

    // Copie de la couche d'entrée
    for (int i = 0; i < 768; i++)
        _input_layer[i] = a._input_layer[i];

    _generation = a._generation;

    if (self) {

        // Scores pour les tournois
        _score = a._score;

        _victories = a._victories;
        _draws = a._draws;
        _losses = a._losses;


        // 100 de base, de façon arbitraire
        _elo = a._elo;
        _games = a._games;

    }

}




// Fonction qui applique une mutation à un agent
void Agent::mutation(float rate) {
    int mutation_size = _input_size * rate;
    int place = GetRandomValue(0, _input_size - mutation_size);

    int* values = new int[mutation_size];
    values[0] = 1;

    for (int i = 0; i < mutation_size; i++) {
        values[i] = _input_layer[i + place];
    } 

    // Mutation d'inversion
    for (int i = 0; i < mutation_size; i++) {
        _input_layer[place + i] = values[mutation_size - i - 1];
    }

}



// Fonction qui fait un crossover entre deux agents pour former cet agent
void Agent::crossover(Agent parent_a, Agent parent_b) {

    int crossover_point = GetRandomValue(0, 768);
    for (int i = 0; i < crossover_point; i++)
        _input_layer[i] = parent_a._input_layer[i];
    for (int i = crossover_point; i < 768; i++)
        _input_layer[i] = parent_b._input_layer[i];

    _generation = max(parent_a._generation, parent_b._generation);

}


// Fonction qui permet de calculer l'élo résultant des deux joueurs après une partie
void calc_elo(Agent &agent_a, Agent &agent_b, float result) {
    // Calcul des coefficiants K (calcul de façon arbitraire... peut-être fait d'une différente façon)
    int ka = 5 + 75 / log2(agent_a._games + 2);
    int kb = 5 + 75 / log2(agent_b._games + 2);

    // Difference d'elo entre les deux joueurs
    int D = agent_a._elo - agent_b._elo;

    // Probabilité de gain de agent_a sur agent_b
    float p = 1 / (1 + pow(10, -D / 400));

    int elo_a = agent_a._elo + ka * (result - p);
    int elo_b = agent_b._elo + kb * (1 - result - (1 - p));

    //cout << "a : " << agent_a._elo << ", b : " << agent_b._elo << ", result : " << result << ", a : " << elo_a << ", b : " << elo_b << endl;

    agent_a._elo = elo_a;
    agent_b._elo = elo_b;

    return;
}



// Fonction qui renvoie parmi une liste d'entiers, renvoie deux index aléatoires, avec des probabilité variantes, en fonction de la grandeur des nombre correspondant à ces index
void pick_random_parents(int* l, int n, int parents[]) {

    // Faire une selection parmi un pourcentage des meilleurs?

    int sum = 0;
    int pa;
    int pb;
    int min = 100000;


    for (int i = 0; i < n; i++) {
        if (l[i] < min)
            min = l[i];
        sum += l[i];
    }

    sum -= n * min;

    int rand_val = GetRandomValue(1, sum);
    int cumul = 0;

    for (int i = 0; i < n; i++) {
        cumul += (l[i] - min);
        if (cumul >= rand_val) {
            pa = i;
            sum -= l[i] - min;
            break;
        }
    }

    rand_val = GetRandomValue(1, sum);
    cumul = 0;

    for (int i = 0; i < n; i++) {
        if (i != pa) {
            cumul += (l[i] - min);
            if (cumul >= rand_val) {
                pb = i;
                break;
            }
        }   
    }
    if (cumul < rand_val)
        cout << "test : " << cumul << ", " << rand_val << endl;

    parents[0] = pa;
    parents[1] = pb;

    return;

}




// Fonction qui crée la prochaine génération d'agents, en fonctions de leurs scores, un taux de nouveau random, un taux de mutation, un taux de crossover... (et on garde le premier agent)
void next_generation(Agent *agents, const int n_agents, float mutation_rate, float randoms_rate, float parents_rate) {
    
    int l_scores[1000];

    // Nombre de parents pour la prochaine génération
    int n_parents = n_agents * parents_rate;

    // Nombre d'agents random
    int n_random = n_agents * randoms_rate;


    // Calcul du meilleur agent de la génération (à faire en elo ou en score?) -> revoir l'initialisation de l'elo...
    int best_score = -100000;
    int best_agent = 0;

    for (int i = 0; i < n_agents; i++) {
        l_scores[i] = agents[i]._score;
        if (agents[i]._score > best_score) {
            best_agent = i;
            best_score = agents[i]._score;
            //best_score = agents[i]._elo;
        }
    }

    cout << "Best agent : " << best_agent << ", Generation : " << agents[best_agent]._generation << ", Score : " << agents[best_agent]._score << ", Ratio (V/D/L): " << agents[best_agent]._victories << "/" << agents[best_agent]._draws << "/" << agents[best_agent]._losses << ", Elo : " << agents[best_agent]._elo << endl;



    // Création de la nouvelle génération d'agents
    cout << "\nGenerating the next generation..." << endl;
    cout << "best agents of this generation (Agent 0-" << n_parents - 1 << "), new random agents (Agent " << n_parents << "-" << n_parents + n_random - 1 << "), new generated agents from the previous bests (Agent " <<  n_parents + n_random << "-" << n_agents - 1 << "), with a mutation rate of " << mutation_rate << endl;

    // Meilleurs agents de la génération précédente (à faire en elo ou en score?) -> revoir l'initialisation de l'elo...
    int best_agents[500];
    int best_scores[500];


    // Initialisation des listes pour trouver les meilleurs agents
    for (int i = 0; i < n_agents; i++) {
        best_scores[i] = -100000;
        best_agents[i] = 0;
    }

    int min;
    int min_index;

    // Pour chaque agent
    for (int i = 0; i < n_agents; i++) {
        min = 100000;
        min_index = 0;

        // Regarde s'il a un elo plus grand que le minimum des elo des parents actuels
        for (int j = 0; j < n_parents; j++) {
            if (best_scores[j] < min) {
                min = best_scores[j];
                min_index = j;
            }
        }

        // cout << "min : " << min << endl;

        // Si c'est le cas, l'ajoute en tant que parent dans la liste
        if (agents[i]._score > min) {
            best_agents[min_index] = i;
            best_scores[min_index] = agents[i]._score;
        }

        // cout << "[";
        // for (int i = 0; i < n_parents; i++)
        //     cout << best_agents[i] << "->" << best_scores[i] << "| ";
        // cout << "]" << endl;
    }

    // Copie des meilleurs agents en début de liste
    for (int i = 0; i < n_parents; i++) {
        agents[i].copy_data(agents[best_agents[i]]);
    }


    // Génération des agents random 
    for (int i = n_parents; i < n_parents + n_random; i++)
        agents[i].init();

    // Crossover utilisant les meilleurs parents
    int parents[2];
    
    for (int i = n_parents + n_random; i < n_agents; i++) {

        // Choix des parents parmi les agents eligibles (dans le meilleur pourcentage des agents)
        pick_random_parents(l_scores, n_parents, parents);
        // cout << "parents used : " << parents[0] << ", " << parents[1] << endl;

        agents[i].crossover(agents[parents[0]], agents[parents[1]]);
        agents[i].mutation(mutation_rate);
        agents[i]._generation++;
        
    }


    return;
}
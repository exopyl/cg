#pragma once
#include "mesh_half_edge.h"

class Context;

//
//
//
class MeshAlgoSmoothingLaplacian
{
public:
	MeshAlgoSmoothingLaplacian () {};
	~MeshAlgoSmoothingLaplacian () {};

	// UNE iteration : chaque sommet manifold non frontiere prend la moyenne de
	// ses voisins. Un lissage a n passes se boucle par l'appelant.
	//
	// ctx optionnel : quand il porte l'annulation, l'algorithme rend false sans
	// rien avoir ecrit dans le maillage. False ne distingue donc pas l'echec de
	// l'annulation -- c'est le contexte que l'appelant interroge.
	bool Apply (Mesh_half_edge *model, const Context *ctx = nullptr);
};

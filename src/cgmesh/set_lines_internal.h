#pragma once
#include <cassert>
#include <cstdlib>
#include <vector>

#include "extracted_line.h"

// Details partages entre les implementations de Cset_lines (set_lines_cantzler.cpp,
// set_lines_new_method.cpp). Rien ici n'est destine aux consommateurs de la classe.
namespace set_lines_detail {

// Recopie les lignes retenues dans le tableau brut attendu par le membre
// `extracted_lines` : le destructeur de Cset_lines le libere par free(), donc le
// stockage doit rester malloc'e meme si le calcul se fait sur un vector.
//
// La taille minimale de 1 evite malloc(0), dont la valeur de retour n'est pas
// specifiee et qui aurait ete passe a free() plus tard.
inline Cextracted_line **to_raw_array (const std::vector<Cextracted_line*>& kept)
{
	// Taille lue UNE fois : deux appels separes a kept.size() ne se prouvent pas
	// egaux pour un analyseur, d'ou le faux positif cpp:S3519 sur ce bloc
	// (set_lines_internal.h:24). Un seul temoin rend l'invariant explicite pour
	// l'outil comme pour le lecteur.
	const size_t n = kept.size();
	Cextracted_line **out = (Cextracted_line**)
		malloc ((n ? n : 1) * sizeof(Cextracted_line*));
	assert (out);
	for (size_t k=0; k<n; k++)
		out[k] = kept[k];
	return out;
}

} // namespace set_lines_detail

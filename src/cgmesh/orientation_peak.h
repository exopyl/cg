#pragma once
#include <cmath>
#include <cstdio>
#include <vector>

// ===========================================================================
//  Recherche du pic dans un accumulateur (phi, theta)
// ===========================================================================
//
// `find_orientation (int id)` etait ecrit TROIS fois : orientation_curvatures.cpp,
// orientation_edges.cpp et orientation_edges2.cpp. Les versions edges et edges2
// etaient identiques au caractere pres ; celle de curvatures n'en differait que
// par le type de l'accumulateur -- `int` au lieu de `float` -- ce qui changeait
// aussi le type des accumulateurs internes des methodes 2 et 3, et donc la
// division entiere du mean shift. Un seul template parametre sur T reproduit
// exactement les trois comportements.
//
// La deduplication ne change PAS ce que le code calcule, y compris le pas
// d'indexation `w*j+i` et les traces printf. Deux ecarts assumes, tous deux sur
// des chemins qui plantaient ou lisaient hors limites :
//
//   - le voisinage (methode 2) ramene son offset dans [0, w*h) par un modulo
//     positif ; l'expression d'origine `(w*(j+k)+(i+l)+w*h)%(w*h)` restait
//     negative des que w*h <= 5*w+5, donc indexait avant le debut du tableau ;
//   - le mean shift (methode 3) ne divise plus par n_points quand il est nul --
//     division entiere par zero en T=int, NaN converti en int en T=float.
//
// Dans les deux cas la valeur produite est identique partout ou l'ancienne
// version etait definie.

namespace orientation_peak {

enum Method
{
	Max             = 0,
	Projection      = 1,
	MaxNeighborhood = 2,
	MeanShift       = 3
};

// Indices du pic dans l'accumulateur. Le mean shift RAFFINE une estimation
// existante : il lit la valeur d'entree autant qu'il l'ecrit, d'ou le passage
// par valeur/retour plutot qu'un simple resultat.
struct Peak
{
	int iphi   = 0;
	int itheta = 0;
};

namespace detail {

// Modulo toujours positif. `%` en C++ garde le signe du dividende, donc
// l'original pouvait produire un index negatif.
inline int wrap (int x, int n)
{
	if (n <= 0)
		return 0;
	const int r = x % n;
	return (r < 0) ? r + n : r;
}

} // namespace detail

template <class T>
Peak find (const std::vector<T>& accumulator, int w, int h, int method,
	   Peak seed = Peak())
{
	Peak peak = seed;
	if (w <= 0 || h <= 0 || (int)accumulator.size() < w*h)
		return peak;

	switch (method)
	{
	case Max:
	{
		// `float` et non `T` : les trois versions d'origine comparaient a un
		// `float max = 0`, y compris celle a accumulateur entier.
		float max = 0;
		for (int i=0; i<w; i++)
			for (int j=0; j<h; j++)
				if (accumulator[w*j+i] > max)
				{
					peak.iphi   = i;
					peak.itheta = j;
					max = (float)accumulator[w*j+i];
				}
		printf ("max -> %d %d\n", peak.itheta, peak.iphi);
	}
	break;

	case Projection:
	{
		/* look at the maximum of each projection */
		// Les deux tableaux etaient des malloc jamais liberes -- les six issues
		// cpp:S3584 de cette famille (orientation_curvatures:154,163 ;
		// orientation_edges:183,192 ; orientation_edges2:185,194).
		std::vector<T> projection_phi (w, T(0));
		for (int i=0; i<w; i++)
			for (int j=0; j<h; j++)
				projection_phi[i] += accumulator[w*j+i];
		peak.iphi = 0;
		for (int i=1; i<w; i++)
			if (projection_phi[i] > projection_phi[peak.iphi])
				peak.iphi = i;

		std::vector<T> projection_theta (h, T(0));
		for (int j=0; j<h; j++)
			for (int i=0; i<w; i++)
				projection_theta[j] += accumulator[w*j+i];
		peak.itheta = 0;
		for (int i=1; i<h; i++)
			if (projection_theta[i] > projection_theta[peak.itheta])
				peak.itheta = i;
		printf ("projection -> %d %d\n", peak.itheta, peak.iphi);
	}
	break;

	case MaxNeighborhood:
	{
		/* look at the maximum in a small neighborough */
		T max_global = T(0);
		for (int i=0; i<w; i++)
			for (int j=0; j<h; j++)
			{
				T max_local = T(0);
				for (int k=-5; k<6; k++)
					for (int l=-5; l<6; l++)
						max_local += accumulator[detail::wrap (w*(j+k)+(i+l), w*h)];
				if (max_local > max_global)
				{
					max_global  = max_local;
					peak.iphi   = i;
					peak.itheta = j;
				}
			}
		printf ("max neighborough -> %d %d\n", peak.itheta, peak.iphi);
	}
	break;

	case MeanShift:
	{
		/* mean shift to have a better estimation */
		const int n_iterations = 5;
		const int r = 5;
		for (int i=0; i<n_iterations; i++)
		{
			T phi_acc   = T(0);
			T theta_acc = T(0);
			T n_points  = T(0);
			for (int j=peak.itheta-r; j<=peak.itheta+r; j++)
				for (int k=peak.iphi-r; k<=peak.iphi+r; k++)
				{
					if ((float)sqrt ((float)((peak.iphi-k)*(peak.iphi-k)
							       + (peak.itheta-j)*(peak.itheta-j))) > r)
						continue;
					const T bin = accumulator[w*detail::wrap (j, h) + detail::wrap (k, w)];
					phi_acc   += bin * k;
					theta_acc += bin * j;
					n_points  += bin;
				}
			// Voisinage vide : on garde l'estimation courante plutot que de
			// diviser par zero.
			if (n_points == T(0))
				break;
			peak.iphi   = (int)(phi_acc / n_points);
			peak.itheta = (int)(theta_acc / n_points);
		}
		printf ("mean shift -> %d %d\n", peak.itheta, peak.iphi);
	}
	break;

	default:
		break;
	}

	return peak;
}

} // namespace orientation_peak

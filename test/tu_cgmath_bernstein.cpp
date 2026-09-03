#include <gtest/gtest.h>

#include "../src/cgmath/bernstein.h"

// ===========================================================================
//  binomialCoefficient : la table est bornee, et le refus le dit
// ===========================================================================
//
// La table interne est [21][21]. Avant correction, `binomialCoefficient` y
// indexait sans verifier : tout degre au-dela de 20 lisait HORS de la table et
// rendait une valeur quelconque, sans le moindre signal. Ce n'etait pas un cas
// theorique — une courbe de Bezier a 22 points de controle y arrive.
//
// La convention de refus n'est pas inventee ici : la table donne deja -1 aux
// couples invalides (n < k), et le bornage s'exprime dans la meme.

TEST (TEST_cgmath_bernstein, the_table_is_read_correctly_inside_its_bounds)
{
	EXPECT_EQ (binomialCoefficient (0, 0), 1);
	EXPECT_EQ (binomialCoefficient (5, 2), 10);
	EXPECT_EQ (binomialCoefficient (10, 5), 252);
	// Le dernier couple valide, celui qui touche le bord sans le franchir.
	EXPECT_EQ (binomialCoefficient (20, 10), 184756);
	EXPECT_EQ (binomialCoefficient (20, 20), 1);
}

TEST (TEST_cgmath_bernstein, an_invalid_pair_below_the_diagonal_is_refused)
{
	// Comportement d'origine, conserve : n < k n'a pas de coefficient.
	EXPECT_EQ (binomialCoefficient (2, 5), -1);
	EXPECT_EQ (binomialCoefficient (0, 1), -1);
}

TEST (TEST_cgmath_bernstein, a_degree_past_the_table_is_refused_instead_of_read_outside)
{
	// LE CAS QUI MOTIVE LE CORRECTIF. 21 est le premier degre hors table ; c'est
	// aussi celui d'une courbe a 22 points de controle.
	EXPECT_EQ (binomialCoefficient (21, 0), -1);
	EXPECT_EQ (binomialCoefficient (21, 21), -1);
	EXPECT_EQ (binomialCoefficient (100, 50), -1);

	// Et les negatifs, qui indexaient AVANT le debut de la table.
	EXPECT_EQ (binomialCoefficient (-1, 0), -1);
	EXPECT_EQ (binomialCoefficient (5, -2), -1);
}

TEST (TEST_cgmath_bernstein, the_polynomial_partitions_unity_on_a_valid_degree)
{
	// Controle de sens sur bernsteinPolynomial, seul appelant du coefficient :
	// pour un degre donne, la somme des polynomes vaut 1 en tout point. Sans lui,
	// les cas ci-dessus ne diraient rien de l'usage reel de la table.
	for (float t : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
	{
		double sum = 0.0;
		for (int k = 0; k <= 6; ++k)
			sum += bernsteinPolynomial (6, k, t);
		EXPECT_NEAR (sum, 1.0, 1e-9) << "t = " << t;
	}
}

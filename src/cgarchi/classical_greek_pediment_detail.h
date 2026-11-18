#pragma once

// En-tete interne du fronton grec, hors de l'API publique : preuve de validation
// des champs propres d'un fronton contre un entablement valide, et corps des
// constructions publiques sans leur validation.

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include <cgarchi/classical_greek_entablature_detail.h>
#include <cgarchi/classical_greek_pediment.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{

struct RefinementStations;   // classical_temple_refinement.h

namespace detail
{

class CheckedPediment;
CheckedPediment CheckPediment (const GreekPedimentSettings &p, const CheckedEntablature &e);

// Preuve de validation des champs propres d'un fronton contre un entablement
// valide : seul CheckPediment en construit une. Elle ne garde que les adresses
// des reglages du fronton et de l'entablement de la preuve d'entablement recue.
// Precondition de l'appelant : ces reglages vivent plus longtemps que la preuve
// et que toutes ses copies.
class CheckedPediment
{
public:
	const GreekPedimentSettings &Settings () const noexcept { return *settings_; }
	const GreekEntablatureSettings &Entablature () const noexcept { return *entablature_; }

private:
	CheckedPediment (const GreekPedimentSettings &p, const GreekEntablatureSettings &e) noexcept
	    : settings_ (&p), entablature_ (&e)
	{
	}
	friend CheckedPediment CheckPediment (const GreekPedimentSettings &p, const CheckedEntablature &e);

	const GreekPedimentSettings *settings_ = nullptr;
	const GreekEntablatureSettings *entablature_ = nullptr;
};

// Valide les champs propres du fronton (refus "Fronton : " de
// ValidateGreekPedimentSettings apres l'entablement), puis rend la preuve.
CheckedPediment CheckPediment (const GreekPedimentSettings &p, const CheckedEntablature &e);
void CheckPediment (const GreekPedimentSettings &&, const CheckedEntablature &) = delete;

// Contrats des deux BuildGreekPedimentPieceMeshes, sans la validation. Leve
// std::logic_error si `p` n'a pas ete valide contre l'objet de reglages
// d'entablement de `e` : l'appariement compare des adresses, pas des valeurs ;
// une copie egale des reglages est refusee.
std::vector<std::unique_ptr<Mesh>> BuildPedimentPieces (const CheckedPediment &p, const CheckedEntablature &e,
                                                        const PeristyleLayout &peristyle, double zFoot,
                                                        const std::function<void (const Mesh &)> &afterEach,
                                                        const std::vector<RoofPocket> &pockets = {});
std::vector<std::unique_ptr<Mesh>> BuildPedimentPieces (const CheckedPediment &p, const CheckedEntablature &e,
                                                        const PeristyleLayout &peristyle, double zFoot,
                                                        const RefinementStations &stations,
                                                        const std::function<void (const Mesh &)> &afterEach,
                                                        const std::vector<RoofPocket> &pockets = {});

// Critere de poche des BuildGreekPedimentPieceMeshes (emprise, plafond au-dessus
// de la base, bords de la base et tympans, coins du plafond strictement sous les
// pans), en double puis sur les coordonnees arrondies en float, sur le repere du
// fronton : vrai si `pocket` serait refusee contre le comble. Ne leve pas.
bool PocketMeetsRoof (const CheckedPediment &p, const CheckedEntablature &e, const PeristyleLayout &peristyle, double zFoot,
                      const RoofPocket &pocket);

// Pieds foot[k] = r (H_b + h_k s) des sommets de la section, dans son ordre :
// les valeurs du repere des constructions, par la meme fonction.
std::vector<double> PedimentFeet (const CheckedPediment &p);

// Refus 'pediment.rakingRun' de BuildGreekPedimentPieceMeshes (pied du haut,
// PedimentFeet (p).back (), au-dela de halfWidth + bands.back ().topProjection),
// meme message, pour la mise en place du temple.
void RequirePedimentTopWithinCrown (const CheckedPediment &p, double halfWidth);

// Nombre de validations des champs propres d'un fronton (l'entablement etant
// valide) sur le thread appelant : croissant, jamais remis a zero ; un test lit
// une difference avant et apres.
std::size_t PedimentOwnValidations () noexcept;

} // namespace detail
} // namespace classical
} // namespace cgarchi

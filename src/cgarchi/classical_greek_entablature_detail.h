#pragma once

// En-tete interne de l'entablement grec, hors de l'API publique : preuve de
// validation des reglages, cotes des bandes, et corps des contrats publics sans
// leur validation, pour un appelant qui detient la preuve (le temple).

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <cgarchi/classical_greek_entablature.h>
#include <cgarchi/classical_peristyle.h>

class Mesh;

namespace cgarchi
{
namespace classical
{
namespace detail
{

class CheckedEntablature;
class MeasuredEntablature;
CheckedEntablature CheckEntablature (const GreekEntablatureSettings &s);
MeasuredEntablature MeasureEntablature (const CheckedEntablature &c);

// Preuve de validation : seul CheckEntablature en construit une, apres
// ValidateGreekEntablatureSettings. Elle ne garde que l'adresse des reglages.
// Precondition de l'appelant : les reglages passes a CheckEntablature vivent
// plus longtemps que la preuve et que toutes ses copies (la surcharge sur
// temporaire est supprimee, ce qui n'ecarte que le cas le plus direct).
class CheckedEntablature
{
public:
	const GreekEntablatureSettings &Settings () const noexcept { return *settings_; }

private:
	explicit CheckedEntablature (const GreekEntablatureSettings &s) noexcept : settings_ (&s) {}
	friend CheckedEntablature CheckEntablature (const GreekEntablatureSettings &s);

	const GreekEntablatureSettings *settings_ = nullptr;
};

// Valide les reglages (refus de ValidateGreekEntablatureSettings), puis rend la
// preuve.
CheckedEntablature CheckEntablature (const GreekEntablatureSettings &s);
void CheckEntablature (const GreekEntablatureSettings &&) = delete;

// Cotes des jonctions de bandes d'un entablement valide : seul
// MeasureEntablature en construit une ; ses refus "Partition : 'bands'" sont
// ceux de GreekEntablatureBandBounds apres la validation.
class MeasuredEntablature
{
public:
	const CheckedEntablature &Checked () const noexcept { return checked_; }
	const GreekEntablatureSettings &Settings () const noexcept { return checked_.Settings (); }
	const std::vector<double> &Bounds () const noexcept { return bounds_; }

private:
	MeasuredEntablature (const CheckedEntablature &c, std::vector<double> bounds) noexcept
	    : checked_ (c), bounds_ (std::move (bounds))
	{
	}
	friend MeasuredEntablature MeasureEntablature (const CheckedEntablature &c);

	CheckedEntablature checked_;
	std::vector<double> bounds_;
};

// Cotes des bandes (contrat de GreekEntablatureBandBounds, sans la validation).
MeasuredEntablature MeasureEntablature (const CheckedEntablature &c);

// Contrat de LayoutGreekFrieze, sans la validation ni les cotes.
FriezeLayout PlaceFrieze (const MeasuredEntablature &e, const PeristyleLayout &p);

// Contrat de LayoutGreekOrnaments sur une frise deja placee par PlaceFrieze sur
// les memes reglages et la meme ligne des axes.
OrnamentLayout PlaceOrnaments (const MeasuredEntablature &e, const PeristyleLayout &p, const FriezeLayout &frieze);

// Contrat de GreekEntablatureSection, sans la validation ni les cotes.
std::vector<RingSectionPoint> EntablatureSection (const MeasuredEntablature &e);

// Contrat de BuildEntablaturePieceMeshes, sans la validation.
std::vector<std::unique_ptr<Mesh>> BuildEntablaturePieces (const CheckedEntablature &e, const PeristyleLayout &p,
                                                           const FriezeLayout &frieze, const OrnamentLayout &ornaments,
                                                           double zFoot, const std::function<void (const Mesh &)> &afterEach);

// Nombre d'appels de ValidateGreekEntablatureSettings sur le thread appelant :
// croissant, jamais remis a zero ; un test lit une difference avant et apres.
std::size_t EntablatureValidations () noexcept;

} // namespace detail
} // namespace classical
} // namespace cgarchi

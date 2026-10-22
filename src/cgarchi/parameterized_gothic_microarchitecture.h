#pragma once

// Adaptateur Maker de la microarchitecture gothique (decision M27). Le RESULTAT
// est le reseau de courbes ; le maillage (GetMesh) n'est que sa visualisation
// tubulaire, relative a MeshOrigin(). Hors du catalogue parametrique : l'objet n'est
// cree qu'a partir d'une specification JSON (createMicroarchFromJson dans Maker).
// Inclus seulement par son .cpp, son test et maker/wasm_api.cpp.

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "parameterized_shapes.h"
#include <cgarchi/gothic_microarchitecture.h>
#include <cgarchi/gothic_microarchitecture_solid.h>
#include <cgarchi/gothic_microarchitecture_tubes.h>

namespace cgarchi
{

// Plafonds du maillage (M20) et des exports OBJ (M27), reglables. Au niveau
// du namespace : un argument par defaut `= Limits{}` d'une structure imbriquee a
// initialiseurs de membres est refuse par GCC et Clang dans sa classe englobante.
struct MicroarchMakerLimits
{
	microarch::TubeLimits tubes;
	std::size_t           max_obj_points    = std::size_t(1) << 21;   // OBJ des courbes
	std::size_t           max_obj_triangles = std::size_t(1) << 21;   // OBJ des tubes
	// Solide imprimable (J7-d) : plafonds de BuildSolidMesh (M29), et seuil de verification
	// (M33, M36) au-dela duquel le STL est exporte sans verdict, marque non verifie. 2^22, le
	// plafond de triangles de M29 : tout solide accepte est verifie, le chemin non verifie ne
	// sert plus qu'a un seuil abaisse. Pic WASM mesure de l'export verifie au plafond, tubes
	// d'affichage au plafond de M20, tampons de meshData et un ancien export de 100 Mio
	// vivants : 1 597 et 1 600 Mio sur deux reseaux, sous les 2 Gio du tas (plan
	// d'execution, §15.9, A5).
	microarch::SolidLimits solid;
	std::size_t            max_verified_triangles = std::size_t(1) << 22;
};

// Resultat d'un export du solide imprimable.
struct MicroarchSolidExport
{
	enum class Verdict { Refused, Printable, NotPrintable, Unverified };
	Verdict                    verdict = Verdict::Refused;
	std::string                stl;                         // STL binaire en mm ; vide si refuse
	microarch::MicroarchReport report;                      // construction, puis verdict
	std::size_t                triangles = 0;
	double                     extent_mm[3] = {0.0, 0.0, 0.0};   // boite du STL
};

// "refused", "printable", "not_printable", "unverified".
const char *SolidExportVerdictName (MicroarchSolidExport::Verdict verdict);

class ParameterizedGothicMicroarchitecture : public ParameterizedMesh
{
public:
	explicit ParameterizedGothicMicroarchitecture (const MicroarchMakerLimits &limits = MicroarchMakerLimits{});

	// Trois parametres d'AFFICHAGE, en unites dont les bornes sont exactes en float puis
	// en double apres conversion :
	//   "Tube radius (permil of D)"  FLOAT [0.1, 100]   radius_ratio = v / 1000
	//   "Tube sides"                 INT   [3, 64]
	//   "Sampling angle (deg)"       FLOAT [0.01, 22.5] max_angle = v * pi / 180
	// Une valeur hors bornes n'est pas ramenee : BuildTubeMesh la refuse
	// (tube_invalid_parameter dans TubeReport()).
	std::vector<Parameter> GetParameters () override;

	// Reconstruit les TUBES seulement, et seulement si les parametres ont change depuis
	// la derniere tentative (reussie ou refusee). L'ancien maillage est libere AVANT la
	// construction du nouveau. Sans reseau : ne fait rien.
	void Regenerate () override;
	std::string GetName () const override { return "Gothic Microarchitecture"; }
	// Cede le maillage et oublie la cle de la derniere tentative.
	Mesh *TakeMesh () override;

	using NetworkJsonSink = std::function<void (const std::string &)>;
	// Ordre : lecture (LoadSpecFromJson), reseau (BuildNetwork), `sink` appele UNE fois
	// avec NetworkToJson, chaine liberee au retour de `sink`, puis tubes. Refus de la
	// lecture ou du reseau : faux, `sink` non appele, ni reseau ni maillage, motif dans
	// LoadReport(). Un refus des tubes n'est pas un refus du chargement : vrai, reseau
	// garde, maillage nul, motif dans TubeReport().
	bool Load (const std::string &specText, const NetworkJsonSink &sink);

	bool                              HasNetwork () const { return m_network.has_value(); }
	const microarch::MicroarchReport &LoadReport () const { return m_loadReport; }   // lecture et reseau
	const microarch::MicroarchReport &TubeReport () const { return m_tubeReport; }   // derniere tentative
	// TubeMesh::origin du maillage courant : point du dessin = sommet + origine.
	const Vector3d                   &MeshOrigin () const { return m_origin; }

	// Polylignes du reseau au pas angulaire COURANT (NetworkToObj). "" sans reseau, ou
	// au-dela de max_obj_points points (sample_limit_exceeded sur export.curves).
	std::string NetworkObj (microarch::MicroarchReport &report) const;
	// Tubes, sommet + origine en double (TubeMeshToObj). "" sans maillage, ou au-dela de
	// max_obj_triangles (tube_limit_exceeded sur export.tubes).
	std::string TubeObj (microarch::MicroarchReport &report) const;

	// Solide imprimable, construit a l'appel (decisions M29 point 5, M33) : BuildSolidMesh avec
	// `params` ; si F <= max_verified_triangles, CheckSolidPrintable avec `printer` a l'echelle
	// mmPerUnit (imprimable ou non), sinon solid_unverified ; SolidToStl (mm, centre en x et y,
	// base du niveau a z = 0) ; puis le solide est libere : ni affiche, ni garde, tubes et cache
	// inchanges. Refus, sans STL : pas de reseau ; mmPerUnit non fini ou <= 0
	// (solid_invalid_parameter sur solid.mm_per_unit) ; refus de BuildSolidMesh, dont le plafond
	// exact de triangles atteint pendant l'extraction (solid_limit_exceeded sur solid.triangles).
	MicroarchSolidExport ExportSolidStl (const microarch::SolidParams &params, double mmPerUnit,
	                                     const PrintConstraints &printer) const;

	const MicroarchMakerLimits &Limits () const { return m_limits; }

	// Parametres convertis pour BuildTubeMesh.
	microarch::TubeParams CurrentTubeParams () const;

	std::size_t NetworkBuilds () const { return m_networkBuilds; }   // compteurs du cache
	std::size_t TubeBuilds () const { return m_tubeBuilds; }

private:
	struct TubeKey
	{
		float radiusPermil = 0.f;
		int   sides        = 0;
		float angleDeg     = 0.f;
		bool operator== (const TubeKey &o) const
		{
			return radiusPermil == o.radiusPermil && sides == o.sides && angleDeg == o.angleDeg;
		}
	};

	microarch::MicroarchSpec          m_spec;
	std::optional<microarch::Network> m_network;
	microarch::MicroarchReport        m_loadReport;
	microarch::MicroarchReport        m_tubeReport;
	MicroarchMakerLimits              m_limits;
	float                             m_radiusPermil = 5.f;
	int                               m_sides        = 6;
	float                             m_angleDeg     = 5.f;
	std::optional<TubeKey>            m_tried;
	Vector3d                          m_origin;
	std::size_t                       m_networkBuilds = 0;
	std::size_t                       m_tubeBuilds    = 0;
};

} // namespace cgarchi

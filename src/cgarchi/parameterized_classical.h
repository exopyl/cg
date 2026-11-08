#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cgarchi/classical_profile.h>
#include "parameterized_shapes.h"   // ParameterizedMesh

namespace cgarchi
{

// Reglages d'une colonne, communs a Classical Column et Classical Colonnade.
// `module` est la longueur du module (demi-diametre inferieur du fut) en unites
// de la scene. Les classes parametrees bornent les reglages a chaque Regenerate
// (Module dans [0,1 ; 10], Radial segments dans [8 ; 256], Curve segments dans
// [2 ; 32], enumerations ramenees dans leur domaine) ; BuildClassicalColumnMesh
// ne les borne pas et rejette un `order` hors domaine.
struct ClassicalColumnSettings
{
	int   order = 0;              // classical::Order
	float module = 1.f;
	bool  pedestal = false;
	bool  entablature = false;
	int   doricCornice = 0;       // classical::DoricCornice
	bool  envelope = false;
	bool  volutes = true;
	bool  flutes = false;
	int   doricFlutes = 0;        // 0 : A (Segment60), 1 : B (Semicircle)
	int   radialSegments = 48;
	int   curveSegments = 6;
};

// Domaine des reglages numeriques : bornes de leurs Parameter, du bornage de
// Regenerate et du document JSON (parameterized_classical_json.h).
constexpr float kClassicalModuleMin = 0.1f, kClassicalModuleMax = 10.f;
constexpr int   kClassicalRadialSegmentsMin = 8, kClassicalRadialSegmentsMax = 256;
constexpr int   kClassicalCurveSegmentsMin = 2, kClassicalCurveSegmentsMax = 32;

// Maillage d'une colonne (tour sur plan, coussinets ioniques), axe Z, pied a
// z = 0 ; une partie de module vaut settings.module / parties par module unites.
// `profile` et `conflicts`, s'ils sont fournis, recoivent le profil (en parties)
// et ses jonctions croisees. Le maillage rendu appartient a l'appelant.
// Leve std::invalid_argument si settings.order est hors de
// [0, classical::kOrderCount), avant d'ecrire `*profile` ou `*conflicts`. Leve
// une std::exception sur tout autre echec ; `*profile` n'est alors pas ecrit,
// `*conflicts` peut l'avoir ete.
std::unique_ptr<Mesh> BuildClassicalColumnMesh (const ClassicalColumnSettings &settings,
                                                classical::ColumnProfile *profile = nullptr,
                                                std::vector<classical::PlanConflict> *conflicts = nullptr);

// Cotes z d'une colonne, en unites monde, dans le repere de
// BuildClassicalColumnMesh (axe Z, pied a z = 0). Chacune est la cote d'un
// sommet du profil tourne, donc d'un anneau du maillage.
struct ClassicalColumnLevels
{
	double foot = 0.0;            // pied du premier membre (piedestal s'il y en a un, sinon base) : 0
	std::optional<double> pedestalTop;   // pied du premier membre du bloc base, si settings.pedestal
	double shaftFoot = 0.0;       // sommet du listel du pied du fut (ShaftZone::foot)
	double shaftTop = 0.0;        // pied du listel de fut_haut (ShaftZone::top)
	double capitalBottom = 0.0;   // pied du premier membre du bloc chapiteau
	double capitalTop = 0.0;      // sommet du dernier membre du bloc chapiteau
	double top = 0.0;             // sommet du profil (ColumnProfile::height) : capitalTop sans entablement
	std::optional<double> entablatureFoot;   // pied de l'architrave, si settings.entablature
};

// Cotes de la colonne que BuildClassicalColumnMesh construirait pour
// `settings`, calculees sur le seul profil, sans tour ni volutes. Memes
// reglages, meme profil, memes cotes : y du profil fois settings.module / parties
// par module, comme le tour.
//
// Domaine : celui de BuildClassicalColumnMesh. Leve std::invalid_argument si
// settings.order est hors de [0, classical::kOrderCount) ; leve une
// std::exception si le profil ne se construit pas. Ne borne rien : `module`
// n'est pas verifie (les cotes lui sont proportionnelles, un module non fini
// les rend non finies) ; `radialSegments` et `curveSegments` sont transmis au
// profil comme pour le maillage, et dans le domaine des Parameter les cotes ne
// dependent d'aucun des deux.
//
// Exclusion : les coussinets ioniques a volutes (« Volutes ») sont concatenes au
// tour et hors profil. Leurs sommets ne sont pas sur des anneaux, et les cotes
// ne disent rien de l'emprise horizontale du maillage de l'ionique a volutes ;
// en z, ces sommets restent dans [foot, top].
ClassicalColumnLevels ComputeClassicalColumnLevels (const ClassicalColumnSettings &settings);

// Colonne d'un des cinq ordres de Vignole : base, fut diminue, chapiteau, et en
// option piedestal et entablement, tournes sur leurs plans
// (classical_lathe.h). Axe Z, pied a z = 0 ; une partie de module vaut
// Module / parties par module unites.
//
// Le maillage appartient a l'objet (ParameterizedMesh) : GetMesh () est valide
// jusqu'au Regenerate suivant ou a la destruction, TakeMesh () en cede la
// propriete. Regenerate capture toute std::exception de la construction : en
// echec, le maillage est vide, LastError () non vide, Profile () et
// PlanConflicts () vides.
//
// Ionique : « Volutes » ajoute les deux coussinets a volutes du chapiteau
// (classical_ionic_capital.h), coques fermees concatenees au tour, sans union.
//
// « Flutes » (decoche par defaut : ornements.cannelures.facultatives) creuse le
// vif du fut ; « Doric flutes » choisit la section du dorique, A (arc de 60
// degres) ou B (demi-cercle). Les conges du fut sont toujours modelises.
//
// Hors perimetre : volutes composites, acanthe, caulicoles, fleurons,
// triglyphes, metopes, gouttes, denticules, mutules, modillons, rudentures ;
// l'entablement est un ressaut carre sur la colonne, pas une poutre filee. Les
// ornements a deux surfaces sont rendus par leur noyau ou par leur enveloppe
// (« Ornament envelope »).
class ParameterizedClassicalColumn : public ParameterizedMesh
{
public:
	ParameterizedClassicalColumn ();
	std::vector<Parameter> GetParameters () override;
	void Regenerate () override;
	std::string GetName () const override { return "Classical Column"; }

	// Profil du dernier Regenerate, en parties ; vide si celui-ci a echoue.
	const classical::ColumnProfile &Profile () const { return m_profile; }

	// Jonctions croisees du dernier profil (FindPlanConflicts) : traitees par le
	// maillage, signalees ici.
	const std::vector<classical::PlanConflict> &PlanConflicts () const { return m_conflicts; }

	// Message de la derniere generation en echec ; vide si elle a reussi. En
	// echec, le maillage est vide.
	const std::string &LastError () const { return m_lastError; }

	// Reglages courants, ceux que GetParameters expose. SetSettings les remplace
	// en entier, sans regenerer ; Regenerate les borne en place avant de
	// construire par BuildClassicalColumnMesh.
	const ClassicalColumnSettings &Settings () const { return m_settings; }
	void SetSettings (const ClassicalColumnSettings &settings) { m_settings = settings; }

private:
	ClassicalColumnSettings m_settings;
	classical::ColumnProfile m_profile;
	std::vector<classical::PlanConflict> m_conflicts;
	std::string m_lastError;
};

// Colonnade de N colonnes d'un ordre, a l'entraxe AxisSpacing, sous un
// entablement file en coupe symetrique (classical_entablature.h), avec ses
// ornements (classical_colonnade.h). Colonnes, entablement et familles
// d'ornements sont des coques fermees concatenees, sans union.
//
// Repere de classical_colonnade.h (X le long de la colonnade, axes des colonnes
// centres sur X = 0, Z vertical, pied a z = 0), mis a l'echelle comme la
// colonne seule. Les colonnes n'ont jamais leur entablement propre. Propriete
// du maillage et regime d'erreur : ceux de ParameterizedClassicalColumn (en
// echec, maillage vide et LastError () non vide ; Warnings () vide).
class ParameterizedClassicalColonnade : public ParameterizedMesh
{
public:
	ParameterizedClassicalColonnade ();
	std::vector<Parameter> GetParameters () override;
	void Regenerate () override;
	std::string GetName () const override { return "Classical Colonnade"; }

	const std::string &LastError () const { return m_lastError; }

	// Avertissements de la derniere generation (maillage au-dela de
	// kColonnadeFaceWarning faces) ; vide sinon.
	const std::string &Warnings () const { return m_warnings; }

	static constexpr unsigned kColonnadeFaceWarning = 300000;

private:
	ClassicalColumnSettings m_settings;
	int  m_columns = 4;
	bool m_ornaments = true;
	std::string m_lastError, m_warnings;
};

} // namespace cgarchi

#pragma once
//
// Scène multi-fichiers pour l'affichage 3D.
//
// Deux niveaux :
//   - Model  : UN fichier chargé = son nom + ses maillages (regroupés dans un
//              VMeshes) + son état d'affichage. Le Model POSSÈDE ses maillages
//              (via VMeshes, dont le destructeur détruit les Mesh*).
//   - VModels : LA scène = plusieurs Model. Possède ses Model via unique_ptr, ce
//              qui garantit des adresses stables (référencées par le survol / la
//              sélection, qui se font au niveau FICHIER, pas au niveau sous-mesh).
//
#include "vmeshes.h"
#include "bvh.h"   // BVH complet requis : membre unique_ptr<BVH> (destruction)

#include <memory>
#include <string>
#include <vector>

// Un fichier de la scène. Non copiable (VMeshes ne l'est pas en toute sécurité :
// il détruit ses Mesh* ; une copie superficielle provoquerait un double-free).
class Model
{
public:
	Model() = default;
	explicit Model(const std::string& name) : m_name(name) {}
	Model(const Model&)            = delete;
	Model& operator=(const Model&) = delete;

	// Recalcule la bbox agrégée des maillages du modèle et la renvoie.
	const BoundingBox& ComputeBBox();

	// Construit un BVH par maillage à faces (pour le picking sur la surface). À
	// appeler UNE fois après chargement : le BVH référence m_pVertices et suppose
	// la géométrie STATIQUE ensuite (une édition du maillage exigerait un rebuild).
	void BuildBVH();

	// true si le modèle a au moins un maillage surfacique (faces) -> BVH interrogeable.
	bool HasSurface() const;

	// Distance du 1er hit surface le long du rayon (orig,dir normalisé), en
	// interrogeant les BVH des maillages ; -1 si aucun hit / pas de surface.
	float RayNearestSurface(const float orig[3], const float dir[3]) const;

public:
	std::string m_name;            // basename du fichier (liste / survol)
	VMeshes     m_meshes;          // maillages de CE fichier (possédés)
	bool        m_visible = true;  // rendu ignoré si false
	BoundingBox m_bbox;            // bbox agrégée (cadrage + surbrillance au survol)

private:
	std::vector<std::unique_ptr<BVH>> m_bvhs;   // un BVH par maillage à faces
};

// La scène : une liste de Model.
class VModels
{
public:
	// Crée un Model vide (nom donné) et renvoie un pointeur STABLE dessus.
	Model* Add(const std::string& name);
	// Retire le i-ème Model (détruit ses maillages). Renvoie false si i hors bornes.
	bool   Remove(std::size_t i);
	void   Clear();

	std::size_t GetNModels() const { return m_models.size(); }
	Model*      GetModel(std::size_t i) { return (i < m_models.size()) ? m_models[i].get() : nullptr; }

	std::vector<std::unique_ptr<Model>>&       GetModels()       { return m_models; }
	const std::vector<std::unique_ptr<Model>>& GetModels() const { return m_models; }

	unsigned int GetNVertices() const;   // somme sur tous les Model
	unsigned int GetNFaces() const;

	// Bbox agrégée (des Model visibles si visibleOnly) — pour cadrer la caméra.
	BoundingBox AggregateBBox(bool visibleOnly = true);

private:
	std::vector<std::unique_ptr<Model>> m_models;
};

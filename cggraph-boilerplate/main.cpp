//
//  Hote natif du canvas -- fenetre, contexte, boucle, et rien d'autre.
//
// Ce fichier est la SEULE partie de l'editeur qui soit propre a un hote. Il ne
// dessine aucun widget et ne connait ni la palette, ni l'inspecteur, ni le
// graphe : il ouvre une fenetre, commence une frame, appelle le canvas, la
// termine. Un second hote -- la cible WebAssembly de l'etape 7 -- remplace ce
// fichier et rien d'autre, ce qui est exactement ce que D4 veut dire par « un
// seul canvas pour les deux hotes ».
//
// DEUXIEME ROLE, d'ou le nom de la cible : c'est le point de depart d'une
// application native de ce depot. L'amorce GLFW, l'amorce ImGui, la boucle et
// l'extinction ne parlent pas de graphe ; seule la ligne canvas.Draw (model)
// designe l'application. Ce qui n'est PAS reutilisable tel quel est signale sur
// place : le fichier de reglages neutralise et l'appel a Pump () portent chacun
// une raison propre a cet editeur.
//
// GLFW et le backend OpenGL 3 plutot que Vulkan : les deux se compilent aussi
// sous Emscripten, ou GLFW est fourni par la chaine (-sUSE_GLFW=3) et le
// backend rend en WebGL 2. Un hote Vulkan aurait ferme cette porte.
//
#include <cstddef>
#include <cstdio>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/canvas/node_canvas.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/ui/editor_model.h"

namespace
{

void OnGlfwError (int code, const char *description)
{
	std::fprintf (stderr, "glfw %d : %s\n", code, description);
}

// Le document donne en argument, relu DANS le graphe encore vide du modele --
// LoadGraph l'exige, les identifiants du document etant repris tels quels.
// Rend vrai si quelque chose a ete relu ; un echec est NOMME sur la sortie
// d'erreur et laisse l'editeur s'ouvrir sur un graphe vide, ce qui vaut mieux
// que de ne pas s'ouvrir du tout.
bool LoadDocument (const char *path, cggraph_ui::EditorModel &model)
{
	const cggraph_nodes::CatalogFactory factory;
	const cggraph::LoadResult result =
	    cggraph::LoadGraphFromFile (path, factory, model.GetGraph ());
	if (!result.IsOk ())
	{
		std::fprintf (stderr, "%s : %s : %s\n", path, cggraph::ToString (result.status),
		              result.detail.c_str ());
		return false;
	}

	// Les noeuds dont la version differe sont DANS le graphe, et l'evaluateur
	// les refusera par un statut nomme. Le taire ferait passer un refus de
	// calcul pour un defaut de l'editeur.
	for (std::size_t i = 0; i < result.incompatible.size (); ++i)
		std::fprintf (stderr, "noeud %u : version de document differente\n",
		              static_cast<unsigned> (result.incompatible[i]));
	return true;
}

} // namespace

// UN CHEMIN DE DOCUMENT EN ARGUMENT, facultatif. C'est le seul « chargement »
// que cet hote connaisse, et c'est bien l'HOTE qui doit le connaitre : le
// canvas ne voit qu'un graphe, jamais l'evenement qui l'a rempli.
int main (int argc, char **argv)
{
	glfwSetErrorCallback (&OnGlfwError);
	if (!glfwInit ())
		return 1;

	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow *window = glfwCreateWindow (1400, 900, "cggraph", nullptr, nullptr);
	if (window == nullptr)
	{
		glfwTerminate ();
		return 1;
	}

	glfwMakeContextCurrent (window);
	glfwSwapInterval (1);

	IMGUI_CHECKVERSION ();
	ImGui::CreateContext ();

	// AUCUN FICHIER DE REGLAGES, comme pour l'editeur de noeuds : les positions
	// appartiennent au document du graphe, et la disposition des deux panneaux
	// flottants est derivee de la taille d'affichage. Un imgui.ini ressusciterait
	// une disposition calculee pour un autre cadre, et il serait ecrit dans le
	// repertoire courant de l'utilisateur.
	ImGui::GetIO ().IniFilename = nullptr;
	ImGui::GetIO ().LogFilename = nullptr;

	ImGui::StyleColorsDark ();
	ImGui_ImplGlfw_InitForOpenGL (window, true);
	ImGui_ImplOpenGL3_Init ("#version 130");

	cggraph_ui::EditorModel model;

	// LE DOCUMENT EST RELU AVANT que le canvas n'existe : sa trace de placement
	// designe des identifiants de noeuds, et un canvas qui aurait deja dessine le
	// graphe vide n'aurait rien de faux -- mais cet ordre-ci rend l'intention
	// lisible, le canvas ne voyant jamais qu'un graphe deja peuple.
	const bool loaded = argc > 1 && LoadDocument (argv[1], model);

	cggraph_canvas::NodeCanvas canvas;

	// LE DECLENCHEUR DE CADRAGE, et il n'est appele qu'ici. Les documents ecrits
	// avant que le graphe ne devienne le fond plein cadre posent leurs noeuds
	// vers l'abscisse zero, donc SOUS la palette qui flotte au-dessus : sans ce
	// cadrage, ouvrir un tel document ne montre rien.
	if (loaded)
		canvas.RequestFitToContent ();

	while (!glfwWindowShouldClose (window))
	{
		glfwPollEvents ();

		// HORS FRAME, et cet appel n'est pas decoratif ici. Sur cet hote le
		// pilote est celui a fil, donc Pump () ne fait rien : c'est justement
		// pourquoi il doit y etre. Un hote qui ne l'ecrit pas fonctionne en
		// natif et ne calcule jamais sur la cible WebAssembly, ou le meme appel
		// est le SEUL lieu de calcul. Le seul endroit ou cette divergence peut
		// se voir est celui-ci.
		model.Pump ();

		ImGui_ImplOpenGL3_NewFrame ();
		ImGui_ImplGlfw_NewFrame ();
		ImGui::NewFrame ();

		canvas.Draw (model);

		ImGui::Render ();

		int width = 0;
		int height = 0;
		glfwGetFramebufferSize (window, &width, &height);
		glViewport (0, 0, width, height);
		glClearColor (0.11f, 0.11f, 0.13f, 1.0f);
		glClear (GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData (ImGui::GetDrawData ());

		glfwSwapBuffers (window);
	}

	ImGui_ImplOpenGL3_Shutdown ();
	ImGui_ImplGlfw_Shutdown ();
	ImGui::DestroyContext ();

	glfwDestroyWindow (window);
	glfwTerminate ();
	return 0;
}

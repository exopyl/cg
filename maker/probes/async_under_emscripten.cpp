// ===========================================================================
//  Que fait cggraph::AsyncEvaluator sous Emscripten ? -- sonde CONSERVEE.
// ===========================================================================
//
// Il COMPILE et il LIE sous emcc. Cela ne dit rien de ce qu'il fait : son
// constructeur demarre un std::thread, et le build WASM du depot est mono-fil.
// Cette sonde le construit et rapporte ce qui arrive reellement.
//
// Resultat mesure le 2026-08-25, emcc 6.0.8, node :
//
//   avant construction du graphe
//   graphe ok
//   avant AsyncEvaluator
//   std::system_error : thread constructor failed: Not supported (code 138)
//   fin
//
// Autrement dit : l'objet ne s'instancie PAS. Toute cible WASM qui en detient
// un -- cggraph_ui::EditorModel en detient un -- est hors d'atteinte du
// navigateur. Le pont de maker/graph_api.cpp utilise donc Evaluator
// directement.
//
// Rejouer :
//
//   cmake --preset maker-wasm -B build/wasm-probe
//   cmake --build build/wasm-probe --target cggraph
//   em++ -std=c++17 -fexceptions -O0 maker/probes/async_under_emscripten.cpp \
//        build/wasm-probe/src/cggraph/libcggraph.a \
//        -o build/wasm-probe/async_probe.js -sENVIRONMENT=node
//   node build/wasm-probe/async_probe.js
//
#include <cstdio>
#include <exception>
#include <system_error>

#include "../../src/cggraph/core/async_evaluator.h"
#include "../../src/cggraph/core/graph.h"

int main ()
{
	std::printf ("avant construction du graphe\n");
	std::fflush (stdout);
	cggraph::Graph graph;
	std::printf ("graphe ok\n");
	std::fflush (stdout);

	try
	{
		std::printf ("avant AsyncEvaluator\n");
		std::fflush (stdout);
		cggraph::AsyncEvaluator async (graph);
		std::printf ("AsyncEvaluator CONSTRUIT -- un fil existe donc\n");
		std::printf ("IsIdle=%d IsBusy=%d started=%u\n", (int)async.IsIdle (),
		             (int)async.IsBusy (), async.GetStartedCount ());
		std::fflush (stdout);
		async.RequestNow (cggraph::kInvalidNodeId);
		async.WaitIdle ();
		std::printf ("WaitIdle rendu -- le fil a tourne\n");
		std::fflush (stdout);
	}
	catch (const std::system_error &e)
	{
		std::printf ("std::system_error : %s (code %d)\n", e.what (), e.code ().value ());
	}
	catch (const std::exception &e)
	{
		std::printf ("std::exception : %s\n", e.what ());
	}

	std::printf ("fin\n");
	return 0;
}

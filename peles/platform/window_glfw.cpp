#include "window_glfw.h"

// ⚠ GLFW_INCLUDE_NONE est pose par la configuration, pas ici : sans lui,
// glfw3.h tire son propre en-tete GL, alors que les entrees GL de ce processus
// viennent de glad et d'une seule source.
#include <GLFW/glfw3.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace peles
{
namespace platform
{

namespace
{

std::string g_lastGlfwError;

void OnGlfwError (int code, const char* description)
{
	g_lastGlfwError = std::string ("glfw erreur ") + std::to_string (code) + " : " +
	                  (description != nullptr ? description : "(sans description)");
}

} // namespace

// La table que les rappels de glfw retrouvent par le pointeur utilisateur de
// la fenetre.
struct CallbackTable
{
	InputCallbacks         input;
	std::function<void ()> refresh;
};

// LA TABLE DES CURSEURS EST INDEXEE PAR CursorShape : une forme ajoutee sans
// relever kCursorShapeCount indexerait hors de la table.
static_assert ((int)CursorShape::Crosshair + 1 == kCursorShapeCount,
               "kCursorShapeCount ne suit plus l'enumeration CursorShape");

struct Window::Impl
{
	GLFWwindow*   handle       = nullptr;
	bool          glfwInitDone = false;
	CallbackTable callbacks;

	// Un curseur standard par forme, cree a la premiere demande. Indexe par
	// CursorShape ; Arrow n'en a pas besoin -- nullptr rend celui du systeme.
	std::array<GLFWcursor*, kCursorShapeCount> cursors {};
	CursorShape currentCursor = CursorShape::Arrow;

	~Impl ()
	{
		// LES CURSEURS AVANT LA FENETRE ET AVANT glfwTerminate, qui les
		// detruirait aussi -- mais un curseur detruit apres coup serait un
		// pointeur libere remis a glfw.
		for (GLFWcursor*& cursor : cursors)
		{
			if (cursor != nullptr)
				glfwDestroyCursor (cursor);
			cursor = nullptr;
		}
		if (handle != nullptr)
			glfwDestroyWindow (handle);
		if (glfwInitDone)
			glfwTerminate ();
	}
};

namespace
{

// Les rappels de glfw sont des fonctions libres : la table de rappels se
// retrouve par le pointeur utilisateur de la fenetre, pose par Create.
//
// ⚠ C'EST LA TABLE QUI EST POINTEE, PAS L'OBJET Window. Publier l'objet ferait
// de son type incomplet une dependance de ces fonctions, et surtout donnerait a
// un rappel de glfw un acces a tout ce que la fenetre sait faire -- dont
// SwapBuffers, appele depuis glfwPollEvents. Le seul rappel qui dessine est
// celui de rafraichissement, et il le fait par la fonction que l'hote a remise.
CallbackTable* TableOf (GLFWwindow* handle)
{
	return static_cast<CallbackTable*> (glfwGetWindowUserPointer (handle));
}

InputCallbacks* CallbacksOf (GLFWwindow* handle)
{
	CallbackTable* const table = TableOf (handle);
	return table != nullptr ? &table->input : nullptr;
}

// LES MODIFICATEURS DE glfw, TRADUITS VERS CEUX DE peles. ⚠ Table explicite et
// non recopie du masque : les valeurs de glfw sont les siennes, et rien
// n'oblige un jour a ce qu'elles coincident avec celles d'un autre systeme.
int ToPelesModifiers (int glfwModifiers)
{
	int modifiers = 0;
	if (glfwModifiers & GLFW_MOD_SHIFT)
		modifiers |= kModShift;
	if (glfwModifiers & GLFW_MOD_CONTROL)
		modifiers |= kModControl;
	if (glfwModifiers & GLFW_MOD_ALT)
		modifiers |= kModAlt;
	return modifiers;
}

// LE BOUTON DE glfw, TRADUIT. Rend -1 pour les boutons que peles ne route pas :
// les livrer sous un code arbitraire les ferait passer pour le bouton gauche.
int ToPelesButton (int glfwButton)
{
	switch (glfwButton)
	{
	case GLFW_MOUSE_BUTTON_LEFT:   return kMouseButtonLeft;
	case GLFW_MOUSE_BUTTON_RIGHT:  return kMouseButtonRight;
	case GLFW_MOUSE_BUTTON_MIDDLE: return kMouseButtonMiddle;
	default:                       return -1;
	}
}

// LA TOUCHE DE glfw, TRADUITE. Rend -1 pour tout ce qui n'est pas dans la
// liste : une touche non traduite n'est mise en file par personne.
//
// ⚠ LA LISTE EST CELLE D'UN CHAMP EN LECTURE SEULE, ET L'ELARGIR A DES
// CONSEQUENCES. `GLFW_KEY_V`, `GLFW_KEY_X`, `GLFW_KEY_BACKSPACE` et
// `GLFW_KEY_DELETE` sont absents parce que RmlUi 6.3 n'a pas d'attribut
// `readonly` : ce qui tient le champ non modifiable, c'est qu'il ne recoit
// aucune touche qui modifie.
int ToPelesKey (int glfwKey)
{
	switch (glfwKey)
	{
	case GLFW_KEY_A:         return kKeyA;
	case GLFW_KEY_C:         return kKeyC;
	case GLFW_KEY_LEFT:      return kKeyLeft;
	case GLFW_KEY_RIGHT:     return kKeyRight;
	case GLFW_KEY_UP:        return kKeyUp;
	case GLFW_KEY_DOWN:      return kKeyDown;
	case GLFW_KEY_HOME:      return kKeyHome;
	case GLFW_KEY_END:       return kKeyEnd;
	case GLFW_KEY_PAGE_UP:   return kKeyPageUp;
	case GLFW_KEY_PAGE_DOWN: return kKeyPageDown;
	default:                 return -1;
	}
}

// LA POSITION DU CURSEUR EST RENDUE EN PIXELS DU TAMPON DE DESSIN.
//
// ⚠ glfw LIVRE DES COORDONNEES DE FENETRE, et la geometrie de RmlUi comme le
// rectangle de la vue sont en pixels de TAMPON. Les deux coincident tant que le
// facteur d'echelle vaut 1 -- ce que le lot assume -- mais recopier la valeur
// sans la convertir ferait dependre le picking du reglage d'echelle du systeme,
// et rien ne le signalerait.
void ToFramebufferPixels (GLFWwindow* handle, double windowX, double windowY, int& x, int& y)
{
	int windowWidth = 0, windowHeight = 0;
	int bufferWidth = 0, bufferHeight = 0;
	glfwGetWindowSize (handle, &windowWidth, &windowHeight);
	glfwGetFramebufferSize (handle, &bufferWidth, &bufferHeight);

	const double scaleX = windowWidth > 0 ? (double)bufferWidth / (double)windowWidth : 1.;
	const double scaleY = windowHeight > 0 ? (double)bufferHeight / (double)windowHeight : 1.;

	x = (int)std::lround (windowX * scaleX);
	y = (int)std::lround (windowY * scaleY);
}

} // namespace

Window::Window () : m_impl (new Impl) {}

Window::~Window () = default;

std::unique_ptr<Window> Window::Create (int width, int height, const std::string& title,
                                        std::string& error)
{
	std::unique_ptr<Window> window (new Window);

	glfwSetErrorCallback (&OnGlfwError);
	g_lastGlfwError.clear ();

	if (!glfwInit ())
	{
		error = "glfwInit a echoue. " + g_lastGlfwError;
		return nullptr;
	}
	window->m_impl->glfwInitDone = true;

	glfwDefaultWindowHints ();

	// LE PROFIL EST DEMANDE EXPLICITEMENT PARCE QUE C'EST LA SEULE FORME
	// VERIFIABLE. Sans hint, le profil obtenu depend du pilote, et un pilote qui
	// delivrerait un contexte core rendrait un ecran noir SANS AUCUN MESSAGE --
	// cgre n'a ni pipeline fixe ni GLSL 1.20 dans un tel contexte.
	// GLFW_OPENGL_CORE_PROFILE est interdit pour tout hote de ce depot qui lie
	// cgre.
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

	glfwWindowHint (GLFW_DEPTH_BITS, 24);
	// LE POCHOIR N'EST PAS OPTIONNEL : RmlUi_Renderer_GL2 s'en sert pour les
	// masques de decoupe (RmlUi_Renderer_GL2.cpp, BeginFrame : glEnable
	// (GL_STENCIL_TEST)). Sans plan de pochoir, tout element decoupe disparait.
	glfwWindowHint (GLFW_STENCIL_BITS, 8);
	// Zero echantillon : l'anticrenelage rend la couleur d'un pixel dependante
	// de la position sous-pixel des aretes, donc non deterministe -- et l'oracle
	// de rendu porte sur UN pixel.
	glfwWindowHint (GLFW_SAMPLES, 0);
	glfwWindowHint (GLFW_DOUBLEBUFFER, GLFW_TRUE);

	g_lastGlfwError.clear ();
	window->m_impl->handle =
	    glfwCreateWindow (width, height, title.c_str (), nullptr, nullptr);
	if (window->m_impl->handle == nullptr)
	{
		error = "glfwCreateWindow a rendu nullptr. " + g_lastGlfwError;
		return nullptr;
	}

	glfwMakeContextCurrent (window->m_impl->handle);
	glfwSetWindowUserPointer (window->m_impl->handle, &window->m_impl->callbacks);
	return window;
}

bool Window::ShouldClose () const
{
	return glfwWindowShouldClose (m_impl->handle) != 0;
}

void Window::PollEvents () const
{
	glfwPollEvents ();
}

void Window::SwapBuffers () const
{
	glfwSwapBuffers (m_impl->handle);
}

void Window::GetFramebufferSize (int& width, int& height) const
{
	glfwGetFramebufferSize (m_impl->handle, &width, &height);
}

void Window::SetSize (int width, int height) const
{
	glfwSetWindowSize (m_impl->handle, width, height);
}

void Window::SetRefreshCallback (std::function<void ()> refresh)
{
	m_impl->callbacks.refresh = std::move (refresh);

	if (!m_impl->callbacks.refresh)
	{
		glfwSetWindowRefreshCallback (m_impl->handle, nullptr);
		return;
	}

	glfwSetWindowRefreshCallback (m_impl->handle,
	                              [] (GLFWwindow* handle)
	                              {
		                              CallbackTable* const table = TableOf (handle);
		                              if (table != nullptr && table->refresh)
			                              table->refresh ();
	                              });
}

bool Window::SetIcon (const std::vector<IconImage>& images) const
{
	std::vector<GLFWimage> native;
	native.reserve (images.size ());
	for (const IconImage& image : images)
	{
		if (image.width <= 0 || image.height <= 0 || image.rgba == nullptr)
			continue;
		GLFWimage entry;
		entry.width  = image.width;
		entry.height = image.height;
		// ⚠ glfw NE COPIE PAS AVANT L'APPEL, mais il ne conserve rien apres :
		// le const_cast ne sert qu'a satisfaire une signature C, et les pixels
		// restent en lecture seule de fait.
		entry.pixels = const_cast<unsigned char*> (image.rgba);
		native.push_back (entry);
	}

	if (native.empty ())
		return false;

	// ⚠ SANS EFFET SUR LES PLATEFORMES QUI N'ONT PAS D'ICONE DE FENETRE --
	// Wayland la tire du fichier .desktop, macOS du paquet -- et glfw y pose
	// GLFW_FEATURE_UNAVAILABLE. Ce n'est pas un echec de l'application : le
	// retour vrai signifie « la demande a ete faite », pas « l'icone est a
	// l'ecran », et aucune interface ne permet de verifier le second.
	glfwSetWindowIcon (m_impl->handle, (int)native.size (), native.data ());
	return true;
}

namespace
{

CursorShape CursorShapeFromCss (const std::string& name)
{
	if (name == "ew-resize" || name == "col-resize")
		return CursorShape::ResizeHorizontal;
	if (name == "ns-resize" || name == "row-resize")
		return CursorShape::ResizeVertical;
	if (name == "text")
		return CursorShape::Text;
	if (name == "pointer")
		return CursorShape::Hand;
	if (name == "crosshair")
		return CursorShape::Crosshair;
	return CursorShape::Arrow;
}

int GlfwStandardCursor (CursorShape shape)
{
	switch (shape)
	{
	case CursorShape::ResizeHorizontal: return GLFW_HRESIZE_CURSOR;
	case CursorShape::ResizeVertical:   return GLFW_VRESIZE_CURSOR;
	case CursorShape::Text:             return GLFW_IBEAM_CURSOR;
	case CursorShape::Hand:             return GLFW_HAND_CURSOR;
	case CursorShape::Crosshair:        return GLFW_CROSSHAIR_CURSOR;
	case CursorShape::Arrow:            break;
	}
	return GLFW_ARROW_CURSOR;
}

} // namespace

void Window::SetCursor (const std::string& cssName)
{
	const CursorShape shape = CursorShapeFromCss (cssName);
	if (shape == m_impl->currentCursor)
		return;

	GLFWcursor* cursor = nullptr;
	if (shape != CursorShape::Arrow)
	{
		GLFWcursor*& slot = m_impl->cursors[(std::size_t)shape];
		if (slot == nullptr)
			slot = glfwCreateStandardCursor (GlfwStandardCursor (shape));
		cursor = slot;
	}

	// Un curseur que le systeme refuse de creer laisse celui par defaut : c'est
	// un defaut d'apparence, pas une panne.
	glfwSetCursor (m_impl->handle, cursor);
	m_impl->currentCursor = cursor != nullptr ? shape : CursorShape::Arrow;
}

CursorShape Window::CurrentCursor () const
{
	return m_impl->currentCursor;
}

void Window::SetInputCallbacks (InputCallbacks callbacks)
{
	m_impl->callbacks.input = std::move (callbacks);

	glfwSetCursorPosCallback (m_impl->handle,
	                          [] (GLFWwindow* handle, double x, double y)
	                          {
		                          InputCallbacks* const sink = CallbacksOf (handle);
		                          if (sink == nullptr || !sink->move)
			                          return;
		                          int pixelX = 0, pixelY = 0;
		                          ToFramebufferPixels (handle, x, y, pixelX, pixelY);
		                          sink->move (pixelX, pixelY);
	                          });

	glfwSetMouseButtonCallback (m_impl->handle,
	                            [] (GLFWwindow* handle, int button, int action, int mods)
	                            {
		                            InputCallbacks* const sink = CallbacksOf (handle);
		                            if (sink == nullptr || !sink->button)
			                            return;
		                            const int neutral = ToPelesButton (button);
		                            // Un bouton lateral n'est route par personne :
		                            // le livrer sous un code de repli le ferait
		                            // passer pour le bouton gauche, donc orbiter.
		                            if (neutral < 0)
			                            return;
		                            sink->button (neutral, action != GLFW_RELEASE,
		                                          ToPelesModifiers (mods));
	                            });

	glfwSetScrollCallback (m_impl->handle,
	                       [] (GLFWwindow* handle, double xoffset, double yoffset)
	                       {
		                       (void)xoffset;
		                       InputCallbacks* const sink = CallbacksOf (handle);
		                       if (sink == nullptr || !sink->wheel)
			                       return;
		                       // glfw ne livre pas l'etat des modificateurs avec le
		                       // defilement : il est relu sur la fenetre, seule
		                       // source disponible a cet instant.
		                       int mods = 0;
		                       if (glfwGetKey (handle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
		                           glfwGetKey (handle, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
			                       mods |= kModShift;
		                       if (glfwGetKey (handle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
		                           glfwGetKey (handle, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
			                       mods |= kModControl;
		                       if (glfwGetKey (handle, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
		                           glfwGetKey (handle, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS)
			                       mods |= kModAlt;
		                       sink->wheel ((float)yoffset, mods);
	                       });

	glfwSetCursorEnterCallback (m_impl->handle,
	                            [] (GLFWwindow* handle, int entered)
	                            {
		                            if (entered)
			                            return;
		                            InputCallbacks* const sink = CallbacksOf (handle);
		                            if (sink != nullptr && sink->leave)
			                            sink->leave ();
	                            });

	glfwSetKeyCallback (m_impl->handle,
	                    [] (GLFWwindow* handle, int key, int scancode, int action, int mods)
	                    {
		                    (void)scancode;
		                    InputCallbacks* const sink = CallbacksOf (handle);
		                    if (sink == nullptr || !sink->key)
			                    return;
		                    const int neutral = ToPelesKey (key);
		                    if (neutral < 0)
			                    return;
		                    // GLFW_REPEAT est traite comme un enfoncement : une
		                    // fleche maintenue doit deplacer le curseur de texte a
		                    // chaque repetition, comme dans tout champ de saisie.
		                    sink->key (neutral, action != GLFW_RELEASE, ToPelesModifiers (mods));
	                    });

	glfwSetWindowFocusCallback (m_impl->handle,
	                            [] (GLFWwindow* handle, int focused)
	                            {
		                            if (focused)
			                            return;
		                            InputCallbacks* const sink = CallbacksOf (handle);
		                            if (sink != nullptr && sink->focusLost)
			                            sink->focusLost ();
	                            });
}

void Window::SetClipboardText (const std::string& text) const
{
	glfwSetClipboardString (m_impl->handle, text.c_str ());
}

std::string Window::ClipboardText () const
{
	const char* const content = glfwGetClipboardString (m_impl->handle);
	return content != nullptr ? std::string (content) : std::string ();
}

void Window::SetSwapInterval (int interval) const
{
	glfwSwapInterval (interval);
}

double Window::ElapsedSeconds ()
{
	return glfwGetTime ();
}

} // namespace platform
} // namespace peles

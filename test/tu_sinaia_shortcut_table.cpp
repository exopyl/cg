//
// Table des raccourcis de sinaia (sinaia/ShortcutTable.h, sans wxWidgets) :
// kCanvasKeyBindings est dispatchée par MyGLCanvas::OnKeyDown et affichée par
// Help > Shortcuts, kGestures n'est qu'affichée. Ce qui se vérifie sans wx :
// entrées renseignées, pas de doublon dans une catégorie, catégories d'un seul
// tenant, dispatch à modificateurs exacts, libellés « A + B ».
//
#include <gtest/gtest.h>

#include "../sinaia/ShortcutTable.h"

#include <cstddef>
#include <iterator>
#include <set>
#include <string>
#include <utility>

namespace {

bool Filled (const char* text)
{
	return text != nullptr && text[0] != '\0';
}

} // namespace

TEST (SinaiaShortcutTable, CanvasKeyBindingsAreFilled)
{
	ASSERT_GT (std::size (sinaia::kCanvasKeyBindings), 0u);
	EXPECT_TRUE (Filled (sinaia::kCanvasKeysCategory));
	for (const sinaia::CanvasKeyBinding& b : sinaia::kCanvasKeyBindings)
	{
		EXPECT_TRUE (Filled (b.action));
		EXPECT_STRNE (sinaia::CanvasKeyName (b.key), "?");
	}
}

TEST (SinaiaShortcutTable, CanvasKeyBindingsHaveNoDuplicateChord)
{
	std::set<std::pair<int, unsigned>> seen;
	for (const sinaia::CanvasKeyBinding& b : sinaia::kCanvasKeyBindings)
		EXPECT_TRUE (seen.insert ({ static_cast<int> (b.key), b.modifiers }).second)
			<< sinaia::FormatChord (b.modifiers, sinaia::CanvasKeyName (b.key));
}

TEST (SinaiaShortcutTable, GesturesAreFilled)
{
	ASSERT_GT (std::size (sinaia::kGestures), 0u);
	for (const sinaia::GestureEntry& g : sinaia::kGestures)
	{
		EXPECT_TRUE (Filled (g.category));
		EXPECT_TRUE (Filled (g.gesture));
		EXPECT_TRUE (Filled (g.action));
	}
}

TEST (SinaiaShortcutTable, GesturesHaveNoDuplicateWithinCategory)
{
	std::set<std::pair<std::string, std::string>> seen;
	for (const sinaia::GestureEntry& g : sinaia::kGestures)
		EXPECT_TRUE (seen.insert ({ g.category, g.gesture }).second) << g.category << " / " << g.gesture;
}

// Le dialogue ouvre une catégorie à chaque changement : une catégorie scindée
// y apparaîtrait deux fois.
TEST (SinaiaShortcutTable, GestureCategoriesAreContiguous)
{
	std::set<std::string> closed;
	std::string current;
	for (const sinaia::GestureEntry& g : sinaia::kGestures)
	{
		if (current == g.category)
			continue;
		EXPECT_EQ (closed.count (g.category), 0u) << g.category;
		if (!current.empty())
			closed.insert (current);
		current = g.category;
	}
	EXPECT_NE (current, sinaia::kCanvasKeysCategory);
	EXPECT_EQ (closed.count (sinaia::kCanvasKeysCategory), 0u);
}

TEST (SinaiaShortcutTable, DispatchRequiresExactModifiers)
{
	using sinaia::CanvasCommand;
	using sinaia::CanvasKey;
	namespace mod = sinaia::modifier;

	EXPECT_EQ (sinaia::FindCanvasCommand (CanvasKey::F, mod::None),    CanvasCommand::FrameSelectedModel);
	EXPECT_EQ (sinaia::FindCanvasCommand (CanvasKey::Up, mod::None),   CanvasCommand::RaiseClippingPlane);
	EXPECT_EQ (sinaia::FindCanvasCommand (CanvasKey::Down, mod::None), CanvasCommand::LowerClippingPlane);

	EXPECT_FALSE (sinaia::FindCanvasCommand (CanvasKey::F, mod::Shift).has_value ());
	EXPECT_FALSE (sinaia::FindCanvasCommand (CanvasKey::F, mod::Ctrl).has_value ());
	EXPECT_FALSE (sinaia::FindCanvasCommand (CanvasKey::Up, mod::Alt).has_value ());

	for (const sinaia::CanvasKeyBinding& b : sinaia::kCanvasKeyBindings)
		EXPECT_EQ (sinaia::FindCanvasCommand (b.key, b.modifiers), b.command);
}

TEST (SinaiaShortcutTable, FormatChordOrdersModifiers)
{
	namespace mod = sinaia::modifier;
	EXPECT_EQ (sinaia::FormatChord (mod::None, "F"), "F");
	EXPECT_EQ (sinaia::FormatChord (mod::Shift | mod::Ctrl, "O"), "Ctrl + Shift + O");
	EXPECT_EQ (sinaia::FormatChord (mod::Shift | mod::Alt | mod::Ctrl, "F12"), "Ctrl + Alt + Shift + F12");
}

// Un geste combiné s'écrit « Alt + Left click », comme FormatChord.
TEST (SinaiaShortcutTable, GestureChordsUseSpacedPlus)
{
	for (const sinaia::GestureEntry& g : sinaia::kGestures)
	{
		const std::string gesture = g.gesture;
		for (std::size_t i = gesture.find ('+'); i != std::string::npos; i = gesture.find ('+', i + 1))
		{
			EXPECT_TRUE (i > 0 && gesture[i - 1] == ' ') << gesture;
			EXPECT_TRUE (i + 1 < gesture.size () && gesture[i + 1] == ' ') << gesture;
		}
	}
}

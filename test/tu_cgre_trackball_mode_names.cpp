//
// Noms du mode de rotation (Ctrackball::RotationModeName, ParseRotationMode) :
// la forme de la console `camera mode` et de la preference /View/RotationMode
// de sinaia. Une cle absente ou illisible doit laisser le defaut de l'appelant.
//

#ifdef CG_HAS_CGRE

#include <gtest/gtest.h>

#include "../src/cgre/examinator_trackball.h"

#include <string>

namespace
{
using Mode = Ctrackball::RotationMode;
}

TEST (CgreTrackballModeNames, NamesRoundTrip)
{
	EXPECT_EQ (std::string (Ctrackball::RotationModeName (Mode::Trackball)), "trackball");
	EXPECT_EQ (std::string (Ctrackball::RotationModeName (Mode::Turntable)), "turntable");

	for (const Mode mode : { Mode::Trackball, Mode::Turntable })
	{
		Mode parsed = (mode == Mode::Trackball) ? Mode::Turntable : Mode::Trackball;
		EXPECT_TRUE (Ctrackball::ParseRotationMode (Ctrackball::RotationModeName (mode), parsed));
		EXPECT_EQ (parsed, mode);
	}
}

TEST (CgreTrackballModeNames, UnknownNameKeepsCallerDefault)
{
	for (const Mode fallback : { Mode::Trackball, Mode::Turntable })
		for (const char* name : { "", "Turntable", "TRACKBALL", "turntable ", "platine", "1" })
		{
			Mode mode = fallback;
			EXPECT_FALSE (Ctrackball::ParseRotationMode (name, mode)) << "'" << name << "'";
			EXPECT_EQ (mode, fallback) << "'" << name << "'";
		}

	Mode mode = Mode::Trackball;
	EXPECT_FALSE (Ctrackball::ParseRotationMode (nullptr, mode));
	EXPECT_EQ (mode, Mode::Trackball);
}

#endif // CG_HAS_CGRE

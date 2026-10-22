#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <cgarchi/gothic_microarchitecture.h>

using namespace cgarchi::microarch;   // fichier de test du module seul

// ===========================================================================
//  Microarchitecture gothique : oracle par recherche brute du developpement
// ===========================================================================
//
// DevelopPlan cherche les courbes de base redondantes et les conflits de hauteur
// aux extremites par un index trie sur x. Ce test le compare, sur des plans
// tires avec une graine fixe, a une recherche brute en O(n^2 |G|) qui applique
// les memes criteres. Les points sont deplaces a 1 tol, 1 - 1e-9 tol et
// 1 + 1e-9 tol de leur position : les coincidences tombent au bord de la
// fenetre de l'index.

namespace
{
    double dist(const Vector2d &a, const Vector2d &b) { return std::hypot(a.x - b.x, a.y - b.y); }

    // Courbe reduite a ce qui la compare (meme critere que le developpement).
    struct Key
    {
        bool     arc = false;
        Vector2d start, end, center;
        double   radius = 0.0, sweep = 0.0;
    };

    Key keyOf(const PlanCurve &c)
    {
        Key k;
        k.start = StartPoint(c);
        k.end = EndPoint(c);
        if (const auto *a = std::get_if<PlanArc>(&c))
        {
            k.arc = true;
            k.center = a->center;
            k.radius = a->radius;
            k.sweep = a->sweep;
        }
        return k;
    }

    Key image(const SymmetrySpec &sym, SymmetryElement g, const Key &k)
    {
        Key o = k;
        o.start = Apply(sym, g, k.start);
        o.end = Apply(sym, g, k.end);
        if (k.arc)
        {
            o.center = Apply(sym, g, k.center);
            o.sweep = g.reflected ? -k.sweep : k.sweep;
        }
        return o;
    }

    bool sameCircle(const Key &a, const Key &b, double t)
    {
        return dist(a.center, b.center) <= t && std::fabs(a.radius - b.radius) <= t
            && std::fabs(std::fabs(a.sweep) - std::fabs(b.sweep)) * a.radius <= t;
    }

    bool same(const Key &a, const Key &b, double t)
    {
        if (a.arc != b.arc || dist(a.start, b.start) > t || dist(a.end, b.end) > t)
            return false;
        return !a.arc || (sameCircle(a, b, t) && (a.sweep > 0) == (b.sweep > 0));
    }

    bool reversed(const Key &a, const Key &b, double t)
    {
        if (a.arc != b.arc || dist(a.start, b.end) > t || dist(a.end, b.start) > t)
            return false;
        return !a.arc || (sameCircle(a, b, t) && (a.sweep > 0) != (b.sweep > 0));
    }

    // Plan tire : courbes libres, ou images d'une courbe deja tiree (redondance),
    // dont certaines extremites sont images d'un point existant (conflit).
    MicroarchSpec randomSpec(std::mt19937_64 &rng)
    {
        std::uniform_real_distribution<double> U(-1.0, 1.0);
        MicroarchSpec s;
        s.symmetry.center = Vector2d(3 * U(rng), 3 * U(rng));
        s.symmetry.rotation_order = 1 + static_cast<int>(rng() % 8);
        if (rng() % 2)
            s.symmetry.reflection_axis_deg = std::fmod(std::fabs(U(rng)) * 180.0, 180.0);
        if (rng() % 3 == 0)
            s.tolerances.point = 1e-3;   // tolerance large : plus de quasi-coincidences
        const std::vector<SymmetryElement> elems = Elements(s.symmetry);

        auto addPoint = [&](const Vector2d &p) {
            PointSpec q;
            q.id = "p" + std::to_string(s.points.size());
            q.xy = p;
            if (rng() % 2)
                q.ground = true;
            else
                q.height = 1.0 + static_cast<double>(rng() % 3);
            s.points.push_back(q);
            return static_cast<int>(s.points.size()) - 1;
        };
        auto pickPoint = [&]() {
            if (!s.points.empty() && rng() % 3 == 0)
            {
                const SymmetryElement e = elems[rng() % elems.size()];
                return addPoint(Apply(s.symmetry, e, s.points[rng() % s.points.size()].xy));
            }
            return addPoint(Vector2d(5 * U(rng), 5 * U(rng)));
        };

        const int nb = 2 + static_cast<int>(rng() % 10);
        for (int i = 0; i < nb; ++i)
        {
            BasicCurveSpec c;
            c.id = "c" + std::to_string(i);
            if (i > 0 && rng() % 2)
            {
                // Image d'une courbe existante, dans un sens ou dans l'autre.
                const BasicCurveSpec &src = s.curves[rng() % s.curves.size()];
                const SymmetryElement e = elems[rng() % elems.size()];
                const PlanCurve img = Apply(s.symmetry, e, src.plan);
                Vector2d pa = StartPoint(img), pb = EndPoint(img);
                const bool swap = rng() % 2;
                if (swap)
                    std::swap(pa, pb);
                c.start_point = addPoint(pa);
                c.end_point = addPoint(pb);
                if (const auto *arc = std::get_if<PlanArc>(&img))
                    c.plan = MakeArc(pa, pb, arc->center, (arc->sweep > 0) != swap);
                else
                    c.plan = PlanSegment{pa, pb};
            }
            else
            {
                c.start_point = pickPoint();
                c.end_point = pickPoint();
                const Vector2d pa = s.points[static_cast<std::size_t>(c.start_point)].xy;
                const Vector2d pb = s.points[static_cast<std::size_t>(c.end_point)].xy;
                if (rng() % 3 == 0)
                {
                    const Vector2d m((pa.x + pb.x) / 2, (pa.y + pb.y) / 2), d(pb.x - pa.x, pb.y - pa.y);
                    const double t = 2 * U(rng);
                    c.plan = MakeArc(pa, pb, Vector2d(m.x - d.y * t, m.y + d.x * t), rng() % 2);
                }
                else
                    c.plan = PlanSegment{pa, pb};
            }
            s.curves.push_back(c);
        }

        // Deplacements au bord de la tolerance, puis courbes reconstruites sur les points deplaces.
        const double tol = Resolve(s.tolerances, Scale(s)).point;
        const double factors[] = {0.0, 1.0, 1.0 - 1e-9, 1.0 + 1e-9};
        for (PointSpec &p : s.points)
        {
            const double f = factors[rng() % 4] * ((rng() % 2) ? 1.0 : 0.5);
            const double angle = 3.14159 * U(rng);
            p.xy = Vector2d(p.xy.x + f * tol * std::cos(angle), p.xy.y + f * tol * std::sin(angle));
        }
        for (BasicCurveSpec &c : s.curves)
        {
            const Vector2d pa = s.points[static_cast<std::size_t>(c.start_point)].xy;
            const Vector2d pb = s.points[static_cast<std::size_t>(c.end_point)].xy;
            if (const auto *a = std::get_if<PlanArc>(&c.plan))
                c.plan = MakeArc(pa, pb, a->center, a->sweep > 0);
            else
                c.plan = PlanSegment{pa, pb};
        }
        return s;
    }

    struct Found
    {
        std::multiset<std::string> redundant;
        std::multiset<std::string> conflicts;
    };

    // Recherche brute, memes criteres que DevelopPlan.
    Found bruteForce(const MicroarchSpec &s)
    {
        const ResolvedTolerances tol = Resolve(s.tolerances, Scale(s));
        const std::vector<SymmetryElement> elems = Elements(s.symmetry);
        Found f;
        std::vector<Key> keys;
        for (const BasicCurveSpec &c : s.curves)
            keys.push_back(keyOf(c.plan));
        for (std::size_t j = 0; j < keys.size(); ++j)
        {
            bool hit = false;
            for (std::size_t i = 0; i < j && !hit; ++i)
                for (SymmetryElement g : elems)
                {
                    const Key im = image(s.symmetry, g, keys[i]);
                    if (same(im, keys[j], tol.point) || reversed(im, keys[j], tol.point))
                    {
                        f.redundant.insert("curves[" + std::to_string(j) + "]");
                        hit = true;
                        break;
                    }
                }
        }
        std::vector<std::size_t> ends;
        for (const BasicCurveSpec &c : s.curves)
            for (int p : {c.start_point, c.end_point})
                ends.push_back(static_cast<std::size_t>(p));
        std::sort(ends.begin(), ends.end());
        ends.erase(std::unique(ends.begin(), ends.end()), ends.end());
        for (std::size_t b = 0; b < ends.size(); ++b)
            for (std::size_t a = 0; a < b; ++a)
            {
                const PointSpec &pa = s.points[ends[a]];
                const PointSpec &pb = s.points[ends[b]];
                if (std::fabs(pa.Z() - pb.Z()) <= tol.height)
                    continue;
                for (std::size_t e = 1; e < elems.size(); ++e)
                    if (dist(Apply(s.symmetry, elems[e], pa.xy), pb.xy) <= tol.point)
                    {
                        f.conflicts.insert("points[" + std::to_string(ends[b]) + "]");
                        break;
                    }
            }
        return f;
    }
}

TEST(TEST_cgarchi_gothic_micro_oracle, indexed_search_matches_brute_force)
{
    std::mt19937_64 rng(12345);
    int compared = 0;
    std::size_t redundantFound = 0, conflictsFound = 0;
    for (int it = 0; it < 400; ++it)
    {
        const MicroarchSpec s = randomSpec(rng);
        MicroarchReport structure;
        if (!ValidateStructure(s, structure))
            continue;   // courbe degeneree ou arc dont le deplacement a separe les extremites
        const Found expected = bruteForce(s);

        MicroarchReport report;
        DevelopPlan(s, report);
        // Les compteurs comptent tout ; les chemins ne se comparent que si les
        // entrees ne sont pas plafonnees.
        EXPECT_EQ(report.Count(MicroarchError::RedundantBasicCurve), expected.redundant.size()) << "cas " << it;
        EXPECT_EQ(report.Count(MicroarchWarning::EndpointHeightConflict), expected.conflicts.size()) << "cas " << it;
        const bool capped = report.entries.size() >= kMicroarchReportMaxEntries;
        Found got;
        for (const MicroarchDiagnostic &d : report.entries)
        {
            if (d.error && d.code == static_cast<int>(MicroarchError::RedundantBasicCurve))
                got.redundant.insert(d.path);
            if (!d.error && d.code == static_cast<int>(MicroarchWarning::EndpointHeightConflict))
                got.conflicts.insert(d.path);
        }
        if (!capped)
        {
            EXPECT_EQ(got.redundant, expected.redundant) << "cas " << it;
            EXPECT_EQ(got.conflicts, expected.conflicts) << "cas " << it;
        }
        ++compared;
        redundantFound += expected.redundant.size();
        conflictsFound += expected.conflicts.size();
    }
    // L'oracle n'est probant que s'il a trouve des cas positifs des deux sortes.
    EXPECT_GT(compared, 200);
    EXPECT_GT(redundantFound, 50u);
    EXPECT_GT(conflictsFound, 50u);
    RecordProperty("compared", compared);
    RecordProperty("redundant", static_cast<int>(redundantFound));
    RecordProperty("conflicts", static_cast<int>(conflictsFound));
}

#include <cgarchi/parameterized_gothic_microarchitecture.h>

#include <cmath>
#include <string>
#include <utility>

#include <cgarchi/gothic_microarchitecture_export.h>
#include <cgarchi/gothic_microarchitecture_json.h>

namespace cgarchi
{

namespace
{
	constexpr double kPi = 3.14159265358979323846;
}

ParameterizedGothicMicroarchitecture::ParameterizedGothicMicroarchitecture (const MicroarchMakerLimits &limits)
	: m_limits(limits)
{
}

std::vector<Parameter> ParameterizedGothicMicroarchitecture::GetParameters ()
{
	return {
		Parameter::MakeFloat("Tube radius (permil of D)", &m_radiusPermil, 0.1f, 100.f),
		Parameter::MakeInt("Tube sides", &m_sides, microarch::TubeLimits::min_sides, microarch::TubeLimits::max_sides),
		Parameter::MakeFloat("Sampling angle (deg)", &m_angleDeg, 0.01f, 22.5f)};
}

microarch::TubeParams ParameterizedGothicMicroarchitecture::CurrentTubeParams () const
{
	microarch::TubeParams p;
	p.radius_ratio       = static_cast<double>(m_radiusPermil) / 1000.0;
	p.sides              = m_sides;
	p.sampling.max_angle = static_cast<double>(m_angleDeg) * (kPi / 180.0);
	return p;
}

void ParameterizedGothicMicroarchitecture::Regenerate ()
{
	if (!m_network)
		return;
	const TubeKey key{m_radiusPermil, m_sides, m_angleDeg};
	if (m_tried && *m_tried == key)
		return;
	// Libere avant de construire : deux maillages ne coexistent jamais.
	delete m_pMesh;
	m_pMesh = nullptr;
	m_tubeReport = microarch::MicroarchReport{};
	m_origin = Vector3d();
	microarch::TubeMesh tubes = microarch::BuildTubeMesh(*m_network, CurrentTubeParams(), m_tubeReport, m_limits.tubes);
	if (tubes.mesh)
	{
		m_pMesh  = tubes.mesh.release();
		m_origin = tubes.origin;
	}
	m_tried = key;
	++m_tubeBuilds;
}

Mesh *ParameterizedGothicMicroarchitecture::TakeMesh ()
{
	m_tried.reset();
	return ParameterizedMesh::TakeMesh();
}

bool ParameterizedGothicMicroarchitecture::Load (const std::string &specText, const NetworkJsonSink &sink)
{
	delete m_pMesh;
	m_pMesh = nullptr;
	m_network.reset();
	m_spec = microarch::MicroarchSpec{};
	m_loadReport = microarch::MicroarchReport{};
	m_tubeReport = microarch::MicroarchReport{};
	m_tried.reset();
	m_origin = Vector3d();

	microarch::MicroarchSpec spec;
	if (!microarch::LoadSpecFromJson(specText, spec, m_loadReport))
		return false;
	std::optional<microarch::Network> network = microarch::BuildNetwork(spec, m_loadReport);
	++m_networkBuilds;
	if (!network)
		return false;
	m_spec    = std::move(spec);
	m_network = std::move(network);
	{
		// Le texte du reseau est cede avant les tubes, et libere avant leur construction.
		const std::string json = microarch::NetworkToJson(*m_network, m_spec, m_loadReport);
		if (sink)
			sink(json);
	}
	Regenerate();
	return true;
}

const char *SolidExportVerdictName (MicroarchSolidExport::Verdict verdict)
{
	switch (verdict)
	{
	case MicroarchSolidExport::Verdict::Printable: return "printable";
	case MicroarchSolidExport::Verdict::NotPrintable: return "not_printable";
	case MicroarchSolidExport::Verdict::Unverified: return "unverified";
	case MicroarchSolidExport::Verdict::Refused: break;
	}
	return "refused";
}

MicroarchSolidExport ParameterizedGothicMicroarchitecture::ExportSolidStl (const microarch::SolidParams &params,
                                                                          double mmPerUnit, const PrintConstraints &printer) const
{
	MicroarchSolidExport out;
	if (!m_network)
		return out;
	if (!(std::isfinite(mmPerUnit) && mmPerUnit > 0.0))
	{
		out.report.Record(microarch::MicroarchError::SolidInvalidParameter, "solid.mm_per_unit",
		                  "the scale must be a finite number of millimetres per drawing unit, > 0");
		return out;
	}
	microarch::SolidMesh solid = microarch::BuildSolidMesh(*m_network, params, out.report, m_limits.solid);
	if (!solid.mesh)
		return out;
	out.triangles = solid.mesh->GetNFaces();
	{
		PrintConstraints scaled = printer;
		scaled.mmPerUnit = mmPerUnit;
		microarch::SolidPrintOptions options;
		options.max_verified_triangles = m_limits.max_verified_triangles;
		const microarch::SolidPrintVerdict v = microarch::CheckSolidPrintable(solid, *m_network, &scaled, out.report, options);
		out.verdict = !v.verified ? MicroarchSolidExport::Verdict::Unverified
		              : v.printable ? MicroarchSolidExport::Verdict::Printable
		                            : MicroarchSolidExport::Verdict::NotPrintable;
	}
	out.stl = microarch::SolidToStl(solid, mmPerUnit, microarch::NetworkBase(*m_network), out.extent_mm);
	return out;
}

std::string ParameterizedGothicMicroarchitecture::NetworkObj (microarch::MicroarchReport &report) const
{
	if (!m_network)
		return std::string();
	const microarch::SamplingParams sampling = CurrentTubeParams().sampling;
	const std::size_t points = microarch::CountNetworkSamples(*m_network, sampling, m_limits.max_obj_points);
	if (points > m_limits.max_obj_points)
	{
		report.Record(microarch::MicroarchError::SampleLimitExceeded, "export.curves",
		              "the curves OBJ would have more than " + std::to_string(m_limits.max_obj_points)
		              + " points ; increase the sampling angle");
		return std::string();
	}
	return microarch::NetworkToObj(*m_network, m_spec, sampling, report);
}

std::string ParameterizedGothicMicroarchitecture::TubeObj (microarch::MicroarchReport &report) const
{
	if (!m_pMesh)
		return std::string();
	if (m_pMesh->GetNFaces() > m_limits.max_obj_triangles)
	{
		report.Record(microarch::MicroarchError::TubeLimitExceeded, "export.tubes",
		              std::to_string(m_pMesh->GetNFaces()) + " triangles > " + std::to_string(m_limits.max_obj_triangles)
		              + " for the tubes OBJ ; increase the sampling angle or reduce the tube sides");
		return std::string();
	}
	return microarch::TubeMeshToObj(*m_pMesh, m_origin);
}

} // namespace cgarchi

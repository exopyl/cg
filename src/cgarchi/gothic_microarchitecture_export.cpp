#include <cgarchi/gothic_microarchitecture_export.h>

#include <exception>
#include <fstream>
#include <limits>
#include <locale>
#include <sstream>

namespace cgarchi::microarch
{

namespace
{
	// Nom d'objet OBJ : un seul mot, sans caractere de controle ni '#' (debut de
	// commentaire OBJ), unique par l'indice de la courbe dans le reseau.
	std::string objectName (const std::string &id, SymmetryElement g, std::size_t index)
	{
		std::string name = id.empty() ? std::string("curve") : id;
		for (char &ch : name)
			if (static_cast<unsigned char>(ch) <= 0x20 || ch == 0x7F || ch == '#')
				ch = '_';
		return name + "_k" + std::to_string(g.k) + (g.reflected ? "_r" : "") + "_" + std::to_string(index);
	}
}

std::string NetworkToObj (const Network &network, const MicroarchSpec &spec, const SamplingParams &params,
                          MicroarchReport &report)
{
	const std::size_t before = report.ErrorCount();
	const std::vector<std::vector<Vector3d>> lines = SampleNetwork(network, params, report);
	if (report.ErrorCount() != before)
		return std::string();

	std::ostringstream os;
	os.imbue(std::locale::classic());
	os.precision(std::numeric_limits<double>::max_digits10);
	os << "# gothic-microarchitecture network : " << lines.size() << " curves\n";
	std::size_t next = 1;
	for (std::size_t i = 0; i < lines.size() && i < network.curves.size(); ++i)
	{
		const SpaceCurve &c = network.curves[i];
		const bool known = c.basic >= 0 && static_cast<std::size_t>(c.basic) < spec.curves.size();
		os << "o " << objectName(known ? spec.curves[static_cast<std::size_t>(c.basic)].id : std::string(), c.element, i) << "\n";
		for (const Vector3d &p : lines[i])
			os << "v " << p.x << " " << p.y << " " << p.z << "\n";
		if (lines[i].size() >= 2)
		{
			os << "l";
			for (std::size_t k = 0; k < lines[i].size(); ++k)
				os << " " << next + k;
			os << "\n";
		}
		next += lines[i].size();
	}
	return os.str();
}

bool WriteNetworkObj (const Network &network, const MicroarchSpec &spec, const SamplingParams &params,
                      const std::string &path, MicroarchReport &report)
{
	try
	{
		const std::size_t before = report.ErrorCount();
		const std::string text = NetworkToObj(network, spec, params, report);
		if (report.ErrorCount() != before)
			return false;
		std::ofstream out(path, std::ios::binary);
		if (out)
		{
			out << text;
			out.close();
		}
		if (!out)
		{
			report.Record(MicroarchError::ExportWriteFailed, "export.path", "cannot write \"" + path + "\"");
			return false;
		}
		return true;
	}
	catch (const std::exception &)
	{
		// Epuisement memoire : compte sans entree, pour ne rien allouer ici.
		++report.errorCounts[static_cast<std::size_t>(MicroarchError::ExportWriteFailed)];
		return false;
	}
}

} // namespace cgarchi::microarch

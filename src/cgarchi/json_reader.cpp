#include <cgarchi/json_reader.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace cgarchi::json_reader
{

using nlohmann::json;

std::string Join (const std::string &parent, const std::string &key)
{
	return parent.empty() ? key : parent + "." + key;
}

std::string Indexed (const std::string &array, std::size_t i)
{
	return array + "[" + std::to_string(i) + "]";
}

std::string NumberText (double v, int precision)
{
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os.precision(precision);
	os << v;
	return os.str();
}

const json *Member (const json &obj, const char *key)
{
	const auto it = obj.find(key);
	return it == obj.end() ? nullptr : &*it;
}

std::string Bounds::Text (int precision) const
{
	return (loOpen ? "(" : "[") + NumberText(lo, precision) + ", " + NumberText(hi, precision) + (hiOpen ? ")" : "]");
}

Bounds AnyNumber () { return Bounds(); }
Bounds AtLeast (double lo) { Bounds b; b.lo = lo; return b; }
Bounds Above (double lo) { Bounds b; b.lo = lo; b.loOpen = true; return b; }
Bounds Interval (double lo, bool loOpen, double hi, bool hiOpen)
{
	Bounds b;
	b.lo = lo; b.loOpen = loOpen;
	b.hi = hi; b.hiOpen = hiOpen;
	return b;
}

void StrictReader::Fault (JsonFault kind, const std::string &path, const std::string &message)
{
	OnFault(kind, path, message);
}

json StrictReader::ParseRejectingDuplicateKeys (const std::string &text)
{
	// Premiere passe SAX, sans DOM. Le chemin courant est tenu dans une seule
	// chaine : chaque conteneur en note la longueur a son ouverture, et chaque
	// cle ou element y remplace le segment precedent. Le cout est lineaire en la
	// taille du texte, plus la copie d'au plus kDuplicateKeyPathLimit chemins.
	// Un parse a rappel (parser_callback_t) ne l'est pas : a chaque fin de
	// conteneur, nlohmann parcourt les membres du parent.
	class DuplicateKeys final : public nlohmann::json_sax<json>
	{
	public:
		explicit DuplicateKeys (StrictReader &reader) : m_reader(reader) {}

		bool null () override { return Value(); }
		bool boolean (bool) override { return Value(); }
		bool number_integer (number_integer_t) override { return Value(); }
		bool number_unsigned (number_unsigned_t) override { return Value(); }
		bool number_float (number_float_t, const string_t &) override { return Value(); }
		bool string (string_t &) override { return Value(); }
		bool binary (binary_t &) override { return Value(); }

		bool start_object (std::size_t) override { return Open(true); }
		bool start_array (std::size_t) override { return Open(false); }
		bool end_object () override { return Close(); }
		bool end_array () override { return Close(); }

		bool key (string_t &val) override
		{
			Scope &top = m_scopes.back();
			m_path.resize(top.base);
			if (top.base != 0)
				m_path += '.';
			m_path += val;
			if (!top.keys.insert(val).second)
			{
				++m_duplicates;
				m_reader.Fault(JsonFault::Schema, m_duplicates <= kDuplicateKeyPathLimit ? m_path : std::string(),
				               "duplicate key");
			}
			return true;
		}

		// L'erreur est levee par la seconde passe (json::parse), a l'identique.
		bool parse_error (std::size_t, const std::string &, const nlohmann::detail::exception &) override
		{
			return false;
		}

	private:
		// `base` : longueur du chemin du conteneur. Un tableau garde l'indice de
		// l'element en cours.
		struct Scope
		{
			bool                  object = false;
			std::size_t           base = 0;
			std::size_t           index = 0;
			std::set<std::string> keys;
		};

		bool Value ()
		{
			if (!m_scopes.empty() && !m_scopes.back().object)
				++m_scopes.back().index;
			return true;
		}
		bool Open (bool object)
		{
			if (!m_scopes.empty() && !m_scopes.back().object)
			{
				const Scope &array = m_scopes.back();
				m_path.resize(array.base);
				m_path += '[';
				m_path += std::to_string(array.index);
				m_path += ']';
			}
			m_scopes.emplace_back();
			m_scopes.back().object = object;
			m_scopes.back().base = m_path.size();
			return true;
		}
		bool Close ()
		{
			m_scopes.pop_back();
			return Value();
		}

		StrictReader      &m_reader;
		std::vector<Scope> m_scopes;
		std::string        m_path;
		std::size_t        m_duplicates = 0;
	};

	DuplicateKeys duplicates(*this);
	json::sax_parse(text, &duplicates);
	return json::parse(text);
}

void StrictReader::TypeError (const std::string &path, const char *expected, const json &value)
{
	Fault(JsonFault::Type, path, std::string("expected ") + expected + ", not " + value.type_name());
}

void StrictReader::Known (const json &obj, const std::string &parent, std::initializer_list<const char *> names)
{
	for (auto it = obj.begin(); it != obj.end(); ++it)
		if (std::none_of(names.begin(), names.end(), [&](const char *n) { return it.key() == n; }))
			Fault(JsonFault::Schema, Join(parent, it.key()), "unknown field");
}

bool StrictReader::Require (const json &obj, const std::string &parent, std::initializer_list<const char *> names)
{
	bool ok = true;
	for (const char *n : names)
		if (!obj.contains(n))
		{
			Fault(JsonFault::Missing, Join(parent, n), "required field is missing");
			ok = false;
		}
	return ok;
}

const json *StrictReader::Object (const json &obj, const std::string &parent, const char *key)
{
	const json *v = Member(obj, key);
	if (v && !v->is_object())
	{
		TypeError(Join(parent, key), "an object", *v);
		return nullptr;
	}
	return v;
}

bool StrictReader::Finite (const json &obj, const std::string &parent, const char *key, double &out)
{
	const json *v = Member(obj, key);
	if (!v)
		return false;
	const std::string path = Join(parent, key);
	if (!v->is_number())
	{
		TypeError(path, "a number", *v);
		return false;
	}
	const double x = v->get<double>();
	if (!std::isfinite(x))
	{
		Fault(JsonFault::Value, path, "non-finite value");
		return false;
	}
	out = x;
	return true;
}

bool StrictReader::Number (const json &obj, const std::string &parent, const char *key, const Bounds &bounds, double &out)
{
	double x = 0.0;
	if (!Finite(obj, parent, key, x))
		return false;
	if (!bounds.Contains(x))
	{
		Fault(JsonFault::Value, Join(parent, key), NumberText(x, m_precision) + " outside " + bounds.Text(m_precision));
		return false;
	}
	out = x;
	return true;
}

bool StrictReader::ToFloat (const std::string &path, double x, float &out)
{
	if (std::fabs(x) > std::numeric_limits<float>::max())
	{
		Fault(JsonFault::Value, path, NumberText(x, m_precision) + " is not finite as a float");
		return false;
	}
	out = static_cast<float>(x);
	return true;
}

bool StrictReader::Float (const json &obj, const std::string &parent, const char *key, const Bounds &bounds, float &out)
{
	double x = 0.0;
	float f = 0.f;
	const std::string path = Join(parent, key);
	if (!Finite(obj, parent, key, x) || !ToFloat(path, x, f))
		return false;
	const float lo = static_cast<float>(bounds.lo);
	const float hi = static_cast<float>(bounds.hi);
	if (!((bounds.loOpen ? f > lo : f >= lo) && (bounds.hiOpen ? f < hi : f <= hi)))
	{
		std::ostringstream read;
		read.imbue(std::locale::classic());
		read << std::setprecision(12) << x;
		const std::string rounded = (static_cast<double>(f) != x) ? " (" + NumberText(f, m_precision) + " as a float)" : "";
		Fault(JsonFault::Value, path, read.str() + rounded + " outside " + bounds.Text(m_precision));
		return false;
	}
	out = f;
	return true;
}

bool StrictReader::Integer (const json &obj, const std::string &parent, const char *key, int lo, int hi, int &out)
{
	const json *v = Member(obj, key);
	if (!v)
		return false;
	const std::string path = Join(parent, key);
	const double x = v->is_number() ? v->get<double>() : 0.0;
	if (!v->is_number() || !std::isfinite(x) || std::floor(x) != x)
	{
		TypeError(path, "an integer", *v);
		return false;
	}
	if (x < lo || x > hi)
	{
		Fault(JsonFault::Value, path,
		      NumberText(x, m_precision) + " outside [" + std::to_string(lo) + ", " + std::to_string(hi) + "]");
		return false;
	}
	out = static_cast<int>(x);
	return true;
}

bool StrictReader::Boolean (const json &obj, const std::string &parent, const char *key, bool &out)
{
	const json *v = Member(obj, key);
	if (!v)
		return false;
	if (!v->is_boolean())
	{
		TypeError(Join(parent, key), "a boolean", *v);
		return false;
	}
	out = v->get<bool>();
	return true;
}

bool StrictReader::String (const json &obj, const std::string &parent, const char *key, std::string &out)
{
	const json *v = Member(obj, key);
	if (!v)
		return false;
	if (!v->is_string())
	{
		TypeError(Join(parent, key), "a string", *v);
		return false;
	}
	out = v->get<std::string>();
	return true;
}

bool StrictReader::Enum (const json &obj, const std::string &parent, const char *key,
                   std::initializer_list<const char *> names, int &index)
{
	std::string s;
	if (!String(obj, parent, key, s))
		return false;
	int i = 0;
	for (const char *n : names)
	{
		if (s == n)
		{
			index = i;
			return true;
		}
		++i;
	}
	std::string expected;
	for (const char *n : names)
		expected += (expected.empty() ? "" : ", ") + std::string(n);
	Fault(JsonFault::Value, Join(parent, key), "unknown value \"" + s + "\" (expected one of: " + expected + ")");
	return false;
}

std::optional<SchemaId> ParseSchemaId (std::string_view text)
{
	const std::size_t slash = text.find('/');
	if (slash == 0 || slash == std::string_view::npos)
		return std::nullopt;
	const std::string_view digits = text.substr(slash + 1);
	if (digits.size() < 2 || digits[0] != 'v' || digits[1] == '0')
		return std::nullopt;
	int version = 0;
	for (const char c : digits.substr(1))
	{
		if (c < '0' || c > '9' || version > (std::numeric_limits<int>::max() - (c - '0')) / 10)
			return std::nullopt;
		version = version * 10 + (c - '0');
	}
	return SchemaId{std::string(text.substr(0, slash)), version};
}

SchemaRead ReadSchema (StrictReader &r, const json &doc, const char *expected)
{
	SchemaRead read;
	const json *schema = Member(doc, "schema");
	if (!schema)
		return read;
	if (!schema->is_string())
	{
		r.TypeError("schema", "a string", *schema);
		read.status = SchemaStatus::NotAString;
		return read;
	}
	const std::string text = schema->get<std::string>();
	read.id = ParseSchemaId(text);
	if (text == expected)
	{
		read.status = SchemaStatus::Expected;
		return read;
	}
	r.Fault(JsonFault::Schema, "schema", "unsupported schema \"" + text + "\" (expected " + expected + ")");
	read.status = SchemaStatus::Unsupported;
	return read;
}

} // namespace cgarchi::json_reader

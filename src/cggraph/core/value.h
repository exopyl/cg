#pragma once
//
//  Valeur transitant sur un lien -- type-erasee.
//
// Une valeur est un couple (descripteur de type, charge utile partagee en
// lecture seule). La charge est detenue par shared_ptr<const void> : le moteur
// ne connait pas le type reel, et ne peut donc pas ecrire dedans. Un noeud qui
// veut ecrire copie, franchement, une fois.
//
// La lecture exige de nommer le type attendu : Get<T> rend nullptr si le
// descripteur ne correspond pas, ce qui rend impossible une conversion muette.
//
#include <cstddef>
#include <memory>

#include "type_registry.h"

namespace cggraph
{

class Value
{
public:
	Value () = default;

	// Rend une valeur vide si le type ou la charge est nul : une valeur portant
	// un type sans charge n'aurait aucun sens pour un consommateur.
	// Accepte shared_ptr<T> comme shared_ptr<const T> : la charge devient
	// immediatement lisible seule, sans que l'appelant ait a la qualifier.
	template <class T>
	static Value Make (const TypeDesc *type, std::shared_ptr<T> payload)
	{
		if (type == nullptr || payload == nullptr)
			return Value ();
		std::shared_ptr<const T> readOnly = std::move (payload);
		return Value (type, std::static_pointer_cast<const void> (readOnly));
	}

	bool IsEmpty () const { return m_payload == nullptr; }

	const TypeDesc *GetType () const { return m_type; }

	template <class T>
	const T *Get (const TypeDesc *expected) const
	{
		if (m_type == nullptr || m_type != expected)
			return nullptr;
		return static_cast<const T *> (m_payload.get ());
	}

	template <class T>
	std::shared_ptr<const T> Share (const TypeDesc *expected) const
	{
		if (m_type == nullptr || m_type != expected)
			return nullptr;
		return std::static_pointer_cast<const T> (m_payload);
	}

	// 0 quand le type ne sait pas se mesurer -- un cache borne doit traiter ce
	// cas, non l'interdire.
	std::size_t GetSizeHint () const
	{
		if (m_type == nullptr || m_payload == nullptr || m_type->sizeHint == nullptr)
			return 0;
		return m_type->sizeHint (m_payload.get ());
	}

private:
	Value (const TypeDesc *type, std::shared_ptr<const void> payload)
		: m_type (type), m_payload (std::move (payload))
	{
	}

	const TypeDesc *m_type = nullptr;
	std::shared_ptr<const void> m_payload;
};

} // namespace cggraph

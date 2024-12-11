#ifndef __PAIR_H__
#define __PAIR_H__
/*
#define GEN_T template <class T>
#define PAIR pair<T>

GEN_T class pair
{
public:
	pair (const T&, const T&);
	pair (const PAIR&);
	~pair ();

	T first (void) const;
	T second (void) const;

private:
	T *m_first, *m_second;
};

GEN_T PAIR::pair (const T& first, const T& second)
{
	m_first = new T(first);
	m_second = new T(second);
}

GEN_T PAIR::pair (const PAIR& src)
{
	m_first = new T (src.first());
	m_second = new T (src.second());
}

GEN_T PAIR::~pair ()
{
	delete m_first;
	delete m_second;
}

GEN_T PAIR::first (void) const
{
	return *m_first;
}

GEN_T PAIR::secondt (void) const
{
	return *m_second;
}

#undef GEN_T
#undef PAIR
*/
#endif // __PAIR_H__

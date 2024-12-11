#ifndef __BINTREE_H__
#define __BINTREE_H__

#define GEN_T template <class T>
#define BINTREE bintree<T>

GEN_T class bintree
{
public:
	bintree (const T&);
	bintree (const T&, const BINTREE&, const BINTREE&);
	bintree (const BINTREE&);
	~bintree ();
	
	bool is_leaf (void) const;
	T get_data (void) const;
	BINTREE get_left (void) const;
	BINTREE get_right (void) const;

	BINTREE& operator= (const BINTREE&);

private:
	T *m_data;
	BINTREE *m_left, *m_right;
};

GEN_T BINTREE::bintree (const T& data)
{
	*m_data = NULL;
	m_left = NULL;
	m_right = NULL;
}

GEN_T BINTREE::bintree (const T& data, const BINTREE& left, const BINTREE& right)
{
	*m_data = data;
	m_left = new BINTREE (left);
	m_right = new BINTREE (right);
}

GEN_T BINTREE::bintree (const BINTREE& src)
{
	*m_data = src.data;
	if (src.is_leaf())
	{
		m_left = NULL;
		m_right = NULL;
	}
	else
	{
		m_left = new BINTREE (*src.m_left);
		m_right = new BINTREE (*src.m_right);
	}
}

GEN_T BINTREE::~bintree ()
{
	delete m_data;
	if (!is_leaf())
	{
		delete m_left;
		delete m_right;
	}
}

GEN_T bool BINTREE::is_leaf (void) const
{
	return (bool)((m_left == NULL) && (m_right == NULL));
}

GEN_T T BINTREE::get_data (void) const
{
	return *m_data;
}

GEN_T BINTREE BINTREE::get_left (void) const
{
	return m_left;
}

GEN_T BINTREE BINTREE::get_right (void) const
{
	return m_right;
}

GEN_T BINTREE& BINTREE::operator= (const bintree& src)
{
	if (&src != this)
	{
		BINTREE::~bintree ();
		BINTREE::bintree (src);
	}
	return &this;
}

#undef GEN_T
#undef BINTREE

#endif // __BINTREE_H__

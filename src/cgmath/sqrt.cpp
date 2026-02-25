#include <cmath>

// ref : https://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi

// Sqrt1
// Reference : http://ilab.usc.edu/wiki/index.php/Fast_Square_Root
// Algorithm: Babylonian Method + some manipulations on IEEE 32 bit floating point representation
float sqrt1(const float x)
{
	union
	{
		int i;
		float x;
	} u;
	u.x = x;
	u.i = (1 << 29) + (u.i >> 1) - (1 << 22);

	// Two Babylonian Steps (simplified from:)
	// u.x = 0.5f * (u.x + x/u.x);
	// u.x = 0.5f * (u.x + x/u.x);
	u.x = u.x + x / u.x;
	u.x = 0.25f*u.x + x / u.x;

	return u.x;
}

// Sqrt2
// Reference : http://ilab.usc.edu/wiki/index.php/Fast_Square_Root
// Algorithm: The Magic Number(Quake 3)
#define SQRT_MAGIC_F 0x5f3759df 
float sqrt2(const float x)
{
	const float xhalf = 0.5f*x;

	union // get bits for floating value
	{
		float x;
		int i;
	} u;
	u.x = x;
	u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
	return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy 
}

// Sqrt3
// Reference : http://ilab.usc.edu/wiki/index.php/Fast_Square_Root
// Algorithm: Log base 2 approximation and Newton's Method
float sqrt3(const float x)
{
	union
	{
		int i;
		float x;
	} u;

	u.x = x;
	u.i = (1 << 29) + (u.i >> 1) - (1 << 22);
	return u.x;
}

// Sqrt4
// Reference : I got it a long time a go from a forum and I forgot, please contact me if you know its reference.
// Algorithm : Bakhsali Approximation
float sqrt4(const float m)
{
	int i = 0;
	while ((i*i) <= m)
		i++;
	i--;
	float d = m - i*i;
	float p = d / (2 * i);
	float a = i + p;
	return a - (p*p) / (2 * a);
}

// Sqrt5
// Reference : http://www.dreamincode.net/code/snippet244.htm
// Algorithm: Babylonian Method
float sqrt5(const float m)
{
	float i = 0;
	float x1, x2;
	while ((i*i) <= m)
		i += 0.1f;
	x1 = i;
	for (int j = 0; j < 10; j++)
	{
		x2 = m;
		x2 /= x1;
		x2 += x1;
		x2 /= 2;
		x1 = x2;
	}
	return x2;
}

// Sqrt6
// Reference : http://www.azillionmonkeys.com/qed/sqroot.html#calcmeth
// Algorithm: Dependant on IEEE representation and only works for 32 bits
double sqrt6(double y)
{
	double x, z, tempf;
	unsigned long *tfptr = ((unsigned long *)&tempf) + 1;
	tempf = y;
	*tfptr = (0xbfcdd90a - *tfptr) >> 1;
	x = tempf;
	z = y*0.5;
	x = (1.5*x) - (x*x)*(x*z);    //The more you make replicates of this statement 
								  //the higher the accuracy, here only 2 replicates are used  
	x = (1.5*x) - (x*x)*(x*z);
	return x*y;
}

// Sqrt7
// Reference : http://bits.stephan-brumme.com/squareRoot.html
// Algorithm: Dependant on IEEE representation and only works for 32 bits
float sqrt7(float x)
{
	unsigned int i = *(unsigned int*)&x;
	// adjust bias
	i += 127 << 23;
	// approximation of square root
	i >>= 1;
	return *(float*)&i;
}

// Sqrt8
// Reference : http://forums.techarena.in/software-development/1290144.htm
// Algorithm: Babylonian Method
double sqrt9(const double fg)
{
	double n = fg / 2.0;
	double lstX = 0.0;
	while (n != lstX)
	{
		lstX = n;
		n = (n + fg / n) / 2.0;
	}
	return n;
}

// Sqrt9
// Reference : http://www.functionx.com/cpp/examples/squareroot.htm
// Algorithm: Babylonian Method
double Abs(double Nbr)
{
	if (Nbr >= 0)
		return Nbr;
	else
		return -Nbr;
}

double sqrt10(double Nbr)
{
	double Number = Nbr / 2;
	const double Tolerance = 1.0e-7;
	do
	{
		Number = (Number + Nbr / Number) / 2;
	} while (Abs(Number * Number - Nbr) > Tolerance);

	return Number;
}

// Sqrt10
// Reference : http://www.cs.uni.edu/~jacobson/C++/newton.html
// Algorithm: Newton's Approximation Method
double sqrt11(const double number)
{
	const double ACCURACY = 0.001;
	double lower, upper, guess;

	if (number < 1)
	{
		lower = number;
		upper = 1;
	}
	else
	{
		lower = 1;
		upper = number;
	}

	while ((upper - lower) > ACCURACY)
	{
		guess = (lower + upper) / 2;
		if (guess*guess > number)
			upper = guess;
		else
			lower = guess;
	}
	return (lower + upper) / 2;

}

// Sqrt11
// Reference : https://fr.wikipedia.org/wiki/M%C3%A9thode_de_Newton
// Algorithm: Newton's Approximation Method
double sqrt12(double a)
{
	if (a < 0) {
		return -1; // erreur : racine carrée non définie
	}

	double x = a;              // approximation initiale
	double epsilon = 1e-6;     // précision souhaitée

	while (std::fabs(x * x - a) > epsilon)
	{
		x = 0.5 * (x + a / x);
	}

	return x;
}

// Sqrt12
// Reference : http://cjjscript.q8ieng.com/?p=32
// Algorithm: Babylonian Method
double sqrt13(int n)
{
	// double a = (eventually the main method will plug values into a)
	double a = (double)n;
	double x = 1;

	// For loop to get the square root value of the entered number.
	for (int i = 0; i < n; i++)
	{
		x = 0.5 * (x + a / x);
	}

	return x;
}
#if 0
// Sqrt13
// Reference : N / A
// Algorithm : Assembly fsqrt
double sqrt13(double n)
{
	__asm {
		fld n
		fsqrt
	}
}

// Sqrt14
// Reference : N / A
// Algorithm : Assembly fsqrt 2
double inline __declspec (naked) __fastcall sqrt14(double n)
{
	_asm fld qword ptr[esp + 4]
	_asm fsqrt
	_asm ret 8
}
#endif

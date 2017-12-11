
#include "Matrix.h"

namespace Matrix
{

vector2d v2d_zero = {0,0};
matrix2d m2d_identity = {1,0,0,1};

matrix2d m2d_multiply_m2d(matrix2d m1, matrix2d m2)
{
	matrix2d result = {
		m1.a*m2.a+m1.b*m2.c, m1.a*m2.b+m1.b*m2.d,
		m1.c*m2.a+m1.d*m2.c, m1.c*m2.b+m1.d*m2.d
	};
	return result;
}
vector2d m2d_multiply_v2d(matrix2d m, vector2d v)
{
	vector2d result = {
		m.a*v.x+m.b*v.y,
		m.c*v.x+m.d*v.y
	};
	return result;
}
matrix2d m2d_multiply_float(matrix2d m, float s)
{
	matrix2d result = {
		m.a*s, m.b*s,
		m.c*s, m.d*s,
	};
	return result;
}

vector2d v2d_multiply_float(vector2d v, float s)
{
	vector2d result = {
		v.x*s,
		v.y*s
	};
	return result;
}

vector2d v2d_add(vector2d v1, vector2d v2)
{
	vector2d result = {
		v1.x+v2.x,
		v1.y+v2.y
	};
	return result;
}

vector2d v2d_sub(vector2d v1, vector2d v2)
{
	vector2d result = {
		v1.x-v2.x,
		v1.y-v2.y
	};
	return result;
}

matrix2d m2d_new(float me0, float me1, float me2, float me3)
{
	matrix2d result = {me0,me1,me2,me3};
	return result;
}

vector2d v2d_new(float x, float y)
{
	vector2d result = {x, y};
	return result;
}

}

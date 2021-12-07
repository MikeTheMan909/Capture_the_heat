#include <math.h>
#include <stdio.h>


void main(){
	float RH_inside = 51;
	RH_inside = RH_inside/100;
	printf("RH_inside = %f\n\r", RH_inside);
	float T_dry	= 21.4;
	float u = 56.8191;
	static const double E = 2.718281828459045;
	double h, t, s, x;
	
	t = RH_inside * 610.78 * exp(((17.08085 * (T_dry - 0)) / (234.175 + T_dry - 0)));;
	
	printf("t = %f\n\r", t);
	
	s = 101325 * pow(E,-(u/((T_dry + 273.15) * 8.314)));
	printf("s = %f\n\r", s);

	x = 622 * t/(s - t);
	printf("x = %f", x);
}



#include <math.h>

int main() {
  double t = 9;
  double z = 10;
  double *x = &t;
  double *y = &z;
  double xpy = pow(&t, z);
}

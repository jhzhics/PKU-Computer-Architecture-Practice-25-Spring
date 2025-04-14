#include <trap.h>

#define N 128
int get_a(int x, int y)
{
	return x * x / (y + 1);
}
int get_b(int x, int y)
{
	return y * y / (x + 1);
}


int a[N][N];
int b[N][1];

int c[N][1];

int main() {
	for (int i = 0; i < N; i ++) {
		for (int j = 0; j < N; j ++) {
			a[i][j] = get_a(i, j);
		}
	}
	for (int i = 0; i < N; i ++) {
		b[i][0] = get_b(i, 0);
	}
	
	for (int i = 0; i < N; i ++) {
		for (int j = 0; j < 1; j ++) {
			c[i][j] = 0;
			for (int k = 0; k < N; k ++) {
				c[i][j] += a[i][k] * b[k][j];
			}
		}
	}

	return 0;
}
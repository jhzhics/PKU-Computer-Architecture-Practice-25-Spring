#include <time.h>
#include <stdlib.h>
#include <stdio.h>

void quicksort(float *begin, float *end);
void task1();
void matmul(float *A, float *B, float *C, int n, int m, int k);
void task2();
size_t Ackermann(size_t m, size_t n);
void task3();

int main()
{
    task1();
    task2();
    task3();
}

void task1()
{
    int n;
    scanf("%d", &n);
    float *ar = malloc(n * sizeof(float));
    for(int i = 0;i < n; i++)
    {
        scanf("%f", ar + i);
    }
    clock_t clk = clock();
    quicksort(ar, ar + n);
    clk = clock() - clk;
    printf("Task1 qsort Time: %lf\n", (double)clk / CLOCKS_PER_SEC);
    free(ar);
}


void quicksort(float *begin, float *end)
{
    if (begin >= end)
        return;
    float *left = begin;
    float *right = end;
    float pivot = *(left + (right - left) / 2);
    while (left <= right)
    {
        while (*left < pivot)
            left++;
        while (*right > pivot)
            right--;
        if (left <= right)
        {
            float tmp = *left;
            *left = *right;
            *right = tmp;
            left++;
            right--;
        }
    }
    quicksort(begin, right);
    quicksort(left, end);  
}

void task2()
{
    int n, m, k;
    scanf("%d %d %d", &n, &m, &k);
    float *A = malloc(n * m * sizeof(float));
    float *B = malloc(m * k * sizeof(float));
    float *C = malloc(n * k * sizeof(float));
    for(int i = 0;i < n; i++)
    {
        for(int j = 0;j < m; j++)
        {
            scanf("%f", A + i * m + j);
        }
    }
    for(int i = 0;i < m; i++)
    {
        for(int j = 0;j < k; j++)
        {
            scanf("%f", B + i * k + j);
        }
    }
    clock_t clk = clock();
    matmul(A, B, C, n, m, k);
    clk = clock() - clk;
    printf("Task2 matmul Time: %lf\n", (double)clk / CLOCKS_PER_SEC);
    free(A);
    free(B);
    free(C);
}

void matmul(float *A, float *B, float *C, int n, int m, int k)
{
    for(int i = 0;i < n; i++)
    {
        for(int j = 0;j < k; j++)
        {
            C[i * k + j] = 0;
            for(int l = 0;l < m; l++)
            {
                C[i * k + j] += A[i * m + l] * B[l * k + j];
            }
        }
    }
}

size_t Ackermann(size_t m, size_t n)
{
    if (m == 0)
        return n + 1;
    if (n == 0)
        return Ackermann(m - 1, 1);
    return Ackermann(m - 1, Ackermann(m, n - 1));
}

void task3()
{
    size_t m, n;
    scanf("%lu %lu", &m, &n);
    clock_t clk = clock();
    size_t res = Ackermann(m, n);
    clk = clock() - clk;
    printf("Task3 ackermann Time: %lf\n", (double)clk / CLOCKS_PER_SEC);
}
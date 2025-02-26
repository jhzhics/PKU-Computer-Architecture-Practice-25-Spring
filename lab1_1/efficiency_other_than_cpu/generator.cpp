#include <random>
#include <iostream>


// TASK 1
void task1()
{
  using std::cout;
  static constexpr int N = 10000000;
  std::minstd_rand gen(0);
  std::uniform_real_distribution<float> dis(0, 1);
  std::cout << N << std::endl;
  for(int i = 0;i < N; i++)
  {
    std::cout << dis(gen) << (i != N - 1 ? ' ' : '\n');
  }
}

void task2()
{
  using std::cout;
  static constexpr int N = 1000;
  static constexpr int M = 1000;
  static constexpr int K = 1000;
  std::minstd_rand gen(0);
  std::uniform_real_distribution<float> dis(0, 1);
  std::cout << N << ' ' << M << ' ' << K << std::endl;
  for(int i = 0; i < N * M; i++)
  {
    std::cout << dis(gen) << (i != N * M - 1 ? ' ' : '\n');
  }
  for(int i = 0; i < M * K; i++)
  {
    std::cout << dis(gen) << (i != M * K - 1 ? ' ' : '\n');
  }
}

void task3()
{
  using std::cout;
  static constexpr int M = 3;
  static constexpr int N = 12;
  cout << M << ' ' << N << std::endl;
}
int main()
{
  task1();
  task2();
  task3();
}
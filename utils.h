#include <random>

static uint64_t uniform_distribution_portable(std::mt19937_64& mersenne_twister, uint64_t n)
{
  uint64_t secureMax = mersenne_twister.max() - mersenne_twister.max() % n;
  uint64_t x;
  do
    x = mersenne_twister();
  while (x >= secureMax);
  return x / (secureMax / n);
}

template<typename T>
void loki_shuffle(std::vector<T>& a, uint64_t seed)
{
if (a.size() <= 1) return;
std::mt19937_64 mersenne_twister(seed);
for (size_t i = 1; i < a.size(); i++)
{
    size_t j = (size_t)uniform_distribution_portable(mersenne_twister, i+1);
    if (i != j)
    std::swap(a[i], a[j]);
}
}
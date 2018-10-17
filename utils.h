#ifndef UTILS_H
#define UTILS_H

#include <random>
#include <string.h>

typedef struct ec_scalar {
    unsigned char data[32];
    bool operator<(const ec_scalar &other) const { return memcmp(this->data, other.data, sizeof(data)) < 0; }
} secret_key, public_key, hash32, byte32;

struct string64 { char data[65]; };

struct pcg32_random_t
{
  uint64_t state;
  uint64_t inc; // seed
};

extern pcg32_random_t global_rng;

hash32   generate_block_hash();
void     generate_keys      (public_key &pub, secret_key &sec);
uint32_t pcg32_random_r     (pcg32_random_t* rng);
bool     percent_chance     (int percent_chance_to_execute);
int      choose_n           (int max_val_not_inclusive);
string64 byte32_to_string   (byte32 const &byte);

inline std::ostream& operator<<(std::ostream& o, const ec_scalar& s) {
    return o << byte32_to_string(s).data;
}

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

#endif // UTILS_H

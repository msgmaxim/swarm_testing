#include "utils.h"
#include "crypto-ops.h"

string64 byte32_to_string(byte32 byte)
{
  char table[16];
  table[0] = '0'; table[1] = '1'; table[2] = '2'; table[3] = '3'; table[4] = '4';
  table[5] = '5'; table[6] = '6'; table[7] = '7'; table[8] = '8'; table[9] = '9';
  table[10] = 'A'; table[11] = 'B'; table[12] = 'C'; table[13] = 'D'; table[14] = 'E';
  table[15] = 'F';

  string64 result;
  result.data[64] = 0;

  for (int i = 0, index64 = 0; i < 32; i++)
  {
    char ch = byte.data[i];
    char ch1 = (ch & 0xF);
    char ch2 = (ch >> 4) & 0xF;
    result.data[index64++] = table[ch1];
    result.data[index64++] = table[ch2];
  }

  return result;
}

static void random_scalar(ec_scalar &res)
{
    unsigned char tmp[64];
    constexpr size_t iterations = sizeof(tmp)/sizeof(uint32_t);
    for (int i = 0; i < iterations; ++i)
    {
      uint32_t random_number = pcg32_random_r(&global_rng);
      memcpy(tmp + (i * sizeof(random_number)), &random_number, sizeof(random_number));
    }

    sc_reduce(tmp);
    memcpy(&res, tmp, 32);
}

void generate_keys(public_key &pub, secret_key &sec)
{
    ge_p3 point;
    secret_key rng;
    random_scalar(rng);
    sec = rng;
    sc_reduce32(&sec.data[0]);  // reduce in case second round of keys (sendkeys)

    ge_scalarmult_base(&point, &sec.data[0]);
    ge_p3_tobytes(&pub.data[0], &point);
}

hash32 generate_block_hash()
{
  hash32 result = {};
  constexpr size_t iterations = sizeof(result.data)/sizeof(uint32_t);
  for (int i = 0; i < iterations; ++i)
  {
    uint32_t random_number = pcg32_random_r(&global_rng);
    memcpy(result.data + (i * sizeof(random_number)), &random_number, sizeof(random_number));
  }

  return result;
}

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

bool choose_100(int percent_chance_to_execute)
{
  uint32_t random_number = pcg32_random_r(&global_rng);
  uint32_t choose100     = random_number % 100;
  bool result            = choose100 <= percent_chance_to_execute;
  return result;
}

#include "swarms.h"
#include <iostream>
#include <vector>
#include <random>

constexpr size_t MAX_SWARM_SIZE = 10; /// deisred
// We never create a new swarm unless there are SWARM_BUFFER extra nodes
// available in the queue.
constexpr size_t SWARM_BUFFER = 4;
// if a swarm has strictly less nodes than this, it is considered unhealthy
// and nearby swarms will mirror it's data. It will disappear, and is already considered gone.
constexpr size_t MIN_SWARM_SIZE = 4;

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

std::vector<uint64_t> get_swarm_ids(const std::map<pub_key, service_node_info>& m_service_nodes_infos)
{
  std::map<uint64_t, size_t> counts;
  for (const auto& entry : m_service_nodes_infos)
    counts[entry.second.swarm_id]++;
  std::vector<uint64_t> swarm_ids;
  swarm_ids.reserve(counts.size());
  for (const auto& entry : counts)
    if (entry.second >= MIN_SWARM_SIZE) swarm_ids.push_back(entry.first);
  return swarm_ids;
}

static uint64_t get_new_node_swarm_id(uint64_t seed, const std::map<pub_key, service_node_info>& sn_infos)
{
  // XXX XXX XXX improvement proposal: have the swarm id be "remembered" from
  // the last time, if the new node was already registered before. This would
  // have huge performance gains preventing nodes from having to resync every
  // time they rejoin the network.

  std::vector<uint64_t> swarm_ids = get_swarm_ids(sn_infos);

  std::mt19937_64 mersenne_twister(seed);

  const size_t swarm_index = (size_t)uniform_distribution_portable(mersenne_twister, swarm_ids.size());

//   return swarm_ids[swarm_index];
  return swarm_ids.size();
}

swarms::swarms(std::map<pub_key, service_node_info>& infos)
  : m_service_nodes_infos(infos)
{}

void swarms::process_reg(const std::string& hash, const std::string& pk) {
    std::cout << "process register\n";
}

void swarms::process_dereg() {
    std::cout << "process DEregister\n";
}

void swarms::process_block(const std::string& hash) {
    std::cout << "--- process block ---\n";

    std::map<SwarmID, size_t> swarm_sizes;
    std::vector<pub_key> swarm_queue;

    for (const auto& entry : m_service_nodes_infos) {
      const auto id = entry.second.swarm_id;
      if (id == 0) {
        swarm_queue.push_back(entry.first);
      } else {
        swarm_sizes[id]++;
      }
    }

    std::cout << "queue size: " << swarm_queue.size() << std::endl;

    std::cout << "swarm count: " << swarm_sizes.size();
    std::cout << ", [";

    for (const auto& entry : swarm_sizes) {
      std::cout << " " << entry.second;
    }

    std::cout << "]\n";


    uint64_t seed = 0;
    std::memcpy(&seed, hash.c_str(), sizeof(seed));
    std::mt19937_64 mersenne_twister(seed);

    std::vector<SwarmID> swarms_to_decommision;

    /// 1. If there are any swarms that are about to dissapear -> try to fill nodes in

    /// TODO: this can be improved by prioritizing the swarms that need fewer extra
    /// nodes, so that the number of decommissioned nodes is as small as possible
    for (const auto& swarm : swarm_sizes) {
      if (swarm.second < MAX_SWARM_SIZE) {
        const auto needed = MAX_SWARM_SIZE - swarm.second;

        if (needed > swarm_queue.size()) {
          /// don't assign nodes to a swarm that is about to get decommisioned
          swarms_to_decommision.push_back(swarm.first);
          continue;
        }

        for (auto i = 0u; i < needed; ++i) {
          /// TODO: need to make sure that swarm_queue is sorted
          const auto idx = uniform_distribution_portable(mersenne_twister, swarm_queue.size());
          const auto sn_pk = swarm_queue.at(idx);
          swarm_queue.erase(swarm_queue.begin() + idx);
          m_service_nodes_infos.at(sn_pk).swarm_id = swarm.first;
        }

        /// TODO: if not enough nodes in the queue, see if we can steal from a large swarm?
      }
    }

    // 2. If we still have nodes in the queue, use them to fill in swarms above the minimal requirement

    /// 3. If there are still enough nodes for IDEAL_SWARM_SIZE + some safety buffer, create a new swarm
    if (swarm_queue.size() >= MAX_SWARM_SIZE + SWARM_BUFFER) {
      /// create a new swarm with MAX_SWARM_SIZE (?) nodes in it (randomly selected)

      /// shuffle the queue and select MAX_SWARM_SIZE first elements

      const auto new_swarm_id = get_new_node_swarm_id(seed + swarm_queue.size(), m_service_nodes_infos); /// TODO: do I really need to pass index here?

      loki_shuffle(swarm_queue, seed + new_swarm_id);

      /// TODO: have a while loop in case there we can create multiple swarms at once
      for (auto i = 0u; i < MAX_SWARM_SIZE; ++i) {
        const auto sn_pk = swarm_queue.at(i);
        m_service_nodes_infos.at(sn_pk).swarm_id = new_swarm_id;
      }

      std::cout << "creating a new swarm with id: " << new_swarm_id << "\n";

    }

    /// 3. If there is a swarm with less than MIN_SWARM_SIZE, decommission that swarm (should almost never happen due to safety buffer).
    for (auto swarm : swarms_to_decommision) {
      
    }



}
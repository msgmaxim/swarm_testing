#include <iostream>
#include <vector>
#include <random>
#include <cstring>
#include <cassert>
#include <algorithm>

#include "swarms.h"
#include "utils.h"

// We never create a new swarm unless there are SWARM_BUFFER extra nodes
// available in the queue.
constexpr size_t SWARM_BUFFER = 4;

static int count_bits(unsigned char byte)
{
    int dist = 0;

    while (byte != 0) {
        dist++;
        byte &= byte - 1;
    }

    return dist;
}


static size_t get_distance(const public_key& pubkey, SwarmID swarm_id)
{
    char data[32];
    memcpy(data + 0 * sizeof(swarm_id), &swarm_id, sizeof(swarm_id));
    memcpy(data + 1 * sizeof(swarm_id), &swarm_id, sizeof(swarm_id));
    memcpy(data + 2 * sizeof(swarm_id), &swarm_id, sizeof(swarm_id));
    memcpy(data + 3 * sizeof(swarm_id), &swarm_id, sizeof(swarm_id));

    size_t distance = 0;

    for (size_t i = 0; i < sizeof(data); i++) {
        distance += count_bits(((unsigned char)data[i]) ^ (unsigned char)pubkey.data[i]);
    }

    return distance;
}

static std::vector<SwarmID> get_swarm_ids(std::map<public_key, service_node_info>& infos)
{
    std::map<SwarmID, size_t> counts;
    for (const auto& entry : infos)
      counts[entry.second.swarm_id]++;

    std::vector<SwarmID> swarm_ids;
    swarm_ids.reserve(counts.size());
    for (const auto& entry : counts)
      if (entry.second >= MIN_SWARM_SIZE)
        swarm_ids.push_back(entry.first);
    return swarm_ids;
}

SwarmID get_swarm_id_for_pubkey(const std::vector<swarm_info>& swarms, const public_key& pk)
{
    auto best = std::make_pair<size_t, SwarmID>(1024, 0);
    for (const auto& swarm_info : swarms) {
        const auto dist = get_distance(pk, swarm_info.id);
        /// Note: this results in swarms with smaller ids
        /// having more users assigned to them
        best = std::min(best, std::make_pair(dist, swarm_info.id));
    }

    return best.second;
}

std::vector<uint64_t> get_swarm_ids(const std::map<public_key, service_node_info>& m_service_nodes_infos)
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

static uint64_t get_new_swarm_id(uint64_t seed)
{
  std::mt19937_64 mersenne_twister(seed);
  return uniform_distribution_portable(mersenne_twister, UINT64_MAX);
}

swarms::swarms(std::map<public_key, service_node_info>& infos)
  : m_service_nodes_infos(infos)
{}

void swarms::process_reg(const public_key& pk) {
    m_service_nodes_infos[pk].swarm_id = 0; // assign to queue
    std::cout << "process register: " << pk << std::endl;
}

void swarms::process_dereg(const public_key& pk) {
    std::cout << "process DEREGISTER: " << pk << std::endl;
    assert(m_service_nodes_infos.find(pk) != m_service_nodes_infos.end());
}

void swarms::process_block(const hash32& hash, Stats& stats) {
    std::cout << "--- process block ---\n";

    std::map<SwarmID, size_t> swarm_sizes;
    std::vector<public_key> swarm_queue;

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
    std::memcpy(&seed, hash.data, sizeof(seed));
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

      const auto new_swarm_id = get_new_swarm_id(seed + swarm_queue.size());

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
    stats.inactive_count += swarm_queue.size();
}

std::vector<swarm_info> swarm_jcktm::get_swarms(add_low_count_swarms add) const
{
  std::map<uint64_t, size_t> swarm_id_and_size;
  for (const auto &entry : m_service_nodes_infos)
    swarm_id_and_size[entry.second.swarm_id]++;

  std::vector<swarm_info> valid_swarms;
  valid_swarms.reserve(swarm_id_and_size.size());

  for (const auto &entry : swarm_id_and_size)
  {
    bool add_swarm                                 = true;
    if (add == add_low_count_swarms::no) add_swarm = entry.second >= MIN_SWARM_SIZE;

    if (add_swarm)
    {
      swarm_info swarm = {};
      swarm.id         = entry.first;
      swarm.size       = static_cast<uint16_t>(entry.second);
      valid_swarms.push_back(swarm);
    }
  }

  return valid_swarms;
}

SwarmID swarm_jcktm::add_new_snode_to_swarm(public_key const &snode_public_key,
                                            hash32 const &block_hash,
                                            uint64_t tx_index)
{
  std::vector<swarm_info> valid_swarms = this->get_swarms(add_low_count_swarms::no);
  swarm_info *desired_swarm = nullptr;
  {
    if (!valid_swarms.empty())
    {
      uint64_t swarm_index_rng_seed = 0;
      std::memcpy(&swarm_index_rng_seed, block_hash.data, sizeof(swarm_index_rng_seed));
      swarm_index_rng_seed += tx_index;

      std::mt19937_64 swarm_index_rng(swarm_index_rng_seed);
      const size_t swarm_index = (size_t)uniform_distribution_portable(swarm_index_rng, valid_swarms.size());

      desired_swarm = &valid_swarms[swarm_index];
    }
  }

  service_node_info result                = {};
  result.swarm_id                         = (desired_swarm) ? desired_swarm->id : 0;
  m_service_nodes_infos[snode_public_key] = result;

  // Check overflow of swarm
  if (desired_swarm && ++desired_swarm->size > MAX_SWARM_SIZE)
  {
    uint64_t overflow_rng_seed = 0;
    std::memcpy(&overflow_rng_seed, block_hash.data, sizeof(overflow_rng_seed));
    overflow_rng_seed += desired_swarm->id;
    std::mt19937_64 overflow_rng(overflow_rng_seed);

    bool move_to_new_swarm = (bool)uniform_distribution_portable(overflow_rng, 2);
    uint64_t new_swarm_id  = uniform_distribution_portable(overflow_rng, UINT64_MAX);

    for (auto &entry : m_service_nodes_infos)
    {
      if (entry.second.swarm_id == desired_swarm->id)
      {
        if (move_to_new_swarm)
        {
            entry.second.swarm_id = new_swarm_id;
            ++this->stats.movements;
        }
        move_to_new_swarm = !move_to_new_swarm;
      }
    }
  }

  return m_service_nodes_infos[snode_public_key].swarm_id;
}

SwarmID swarm_jcktm::remove_snode_from_swarm(public_key const &snode_key)
{
  assert(m_service_nodes_infos.find(snode_key) != m_service_nodes_infos.end());
  uint64_t const swarm_id = m_service_nodes_infos[snode_key].swarm_id;
  m_service_nodes_infos.erase(snode_key);

  std::vector<swarm_info> all_swarms = this->get_swarms(add_low_count_swarms::yes);
  swarm_info *starving_swarm         = nullptr;
  {
    auto it = std::find_if(all_swarms.begin(),
                           all_swarms.end(),
                           [swarm_id](const swarm_info &swarm) { return (swarm.id == swarm_id); });

    if (it == all_swarms.end()) // last node in swarm was deleted
    {
        ++this->lifetime_stat.num_times_swarm_died;
        return swarm_id;
    }

    starving_swarm = &(*it);
  }

  // Steal nodes from the largest swarm and add to starving swarm
  {
    auto get_largest_swarm = [](std::vector<swarm_info> &swarms) -> swarm_info * {
      if (swarms.size() == 0) return nullptr;

      swarm_info *largest_swarm = &swarms[0];
      for (swarm_info &check : swarms)
      {
        if (check.size > largest_swarm->size) largest_swarm = &check;
      }

      return largest_swarm;
    };

    for (swarm_info *largest_swarm = get_largest_swarm(all_swarms);
         largest_swarm && largest_swarm->size > MIN_SWARM_SIZE;
         largest_swarm = get_largest_swarm(all_swarms))
    {
      for (auto &it : m_service_nodes_infos) // Reassign node from largest swarm to starving swarm
      {
        service_node_info &snode = it.second;
        if (snode.swarm_id == largest_swarm->id)
        {
          ++this->lifetime_stat.num_times_nodes_stolen;
          ++this->stats.movements;

          snode.swarm_id = starving_swarm->id;
          --largest_swarm->size;
          ++starving_swarm->size;
          break;
        }
      }

      if (starving_swarm->size >= MIN_SWARM_SIZE)
      {
        break;
      }
    }
  }

  if (starving_swarm->size < MIN_SWARM_SIZE)
  {
    // XXX XXX XXX XXX
    // All nodes should fetch data from this swarm at this point only.
    // The registered nodes will eventually disappear and after this point,
    // it is already considered gone. It only exists to retrieve data from.
    //
    // XXX XXX XXX XXX
    //         internship optimization hardfork idea:
    //         when nodes have been decomissioned for more than 10 blocks,
    //         move the nodes into new swarms immediately.
  }

  return swarm_id;
}

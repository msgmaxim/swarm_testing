#include <vector>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string.h>
#include <algorithm>

#include "swarms.h"
#include "utils.h"

struct SnodeEvent
{
  enum struct Type { REG, DEREG, };
  Type type;
  public_key pubkey;
};

struct Event
{
    uint64_t                height;
    hash32                  block_hash;
    std::vector<SnodeEvent> snode_events;
};

#if 0
std::vector<Event> read_events(const char* path) {

    std::vector<Event> result;

    std::ifstream file(path);
    std::string line;

    while(std::getline(file, line)) {

        auto pos = line.find_first_of(" ");
        auto pos2 = line.find_first_of(" ", pos + 1);
        auto pos3 = line.find_first_of(" ", pos2 + 1);

        const uint64_t h = std::stoi(line.substr(0, pos));
        const std::string hash = line.substr(pos + 2, pos2 - pos - 3);
        const std::string type_str = line.substr(pos2 + 1, pos3 - pos2 - 1);
        const std::string pk = line.substr(pos3 + 1);
        EventType type;

        if (type_str == "reg") {
            type = EventType::REG;
        } else if (type_str == "dereg") {
            type = EventType::DEREG;
        } else {
            abort();
        }

        result.push_back({type, h, hash, pk});

    }

    file.close();

    return result;
}
#endif

int global_num_deregistrations;
int global_num_registrations;
std::vector<Event> generate_random_events(int num_events) {

    std::vector<Event> result;
    result.reserve(num_events);

    std::mt19937_64 mt(0);
    std::vector<public_key> registered_snodes;

    for (int idx = 0; idx < num_events; ++idx) {
        Event new_event      = {};
        new_event.height     = idx;
        new_event.block_hash = generate_block_hash();

        int num_snode_events = (uniform_distribution_portable(mt, 5) + 1);
        new_event.snode_events.reserve(num_snode_events);
        for (int i = 0; i < num_snode_events; ++i)
        {
          if (!registered_snodes.empty() && percent_chance(10))
          {
            SnodeEvent snode_event = {};
            snode_event.type       = SnodeEvent::Type::DEREG;
            auto rnd_idx           = uniform_distribution_portable(mt, registered_snodes.size());
            auto it                = registered_snodes.begin() + rnd_idx;
            snode_event.pubkey     = *it;

            registered_snodes.erase(it);
            new_event.snode_events.push_back(snode_event);

            ++global_num_deregistrations;
          }
          else if (percent_chance(50))
          {
            SnodeEvent snode_event = {};
            snode_event.type       = SnodeEvent::Type::REG;
            secret_key dummy_key   = {};
            generate_keys(snode_event.pubkey, dummy_key);

            registered_snodes.push_back(snode_event.pubkey);
            new_event.snode_events.push_back(snode_event);

            ++global_num_registrations;
          }
        }

        result.push_back(new_event);
    }

    return result;

}

static std::vector<public_key> generate_random_users(size_t num_users)
{
  std::vector<public_key> users;
  users.reserve(num_users);

  for (auto i = 0u; i < num_users; ++i) {
    secret_key dummy_key   = {};
    public_key pub_key;
    generate_keys(pub_key, dummy_key);
    users.push_back(pub_key);
  }

  return users;
}

size_t count_movements(const std::map<public_key, service_node_info>& prev,
                       const std::map<public_key, service_node_info>& cur)
{

  size_t movements = 0;
  /// count all nodes in prev that now have different swarm_id
  for (const auto& entry : prev) {

      /// Note: in my implementation of the queue, swarm_id of 0 represents the queue, not a real swarm
      if (entry.second.swarm_id == 0) continue;

      const auto pk = entry.first;
      if (cur.find(pk) != cur.end() && cur.at(pk).swarm_id != entry.second.swarm_id) {
          movements++;
      }
  }

  return movements;

}

std::map<SwarmID, std::vector<public_key>> map_users_to_swarms(const std::vector<swarm_info>& swarms, const std::vector<public_key>& users) {
  std::map<SwarmID, std::vector<public_key>> swarm2pks;

  for (const auto& user : users) {
    swarm2pks[get_swarm_id_for_pubkey(swarms, user)].push_back(user);
  }
  return swarm2pks;
}

void print_identity_mapping(const std::map<SwarmID, std::vector<public_key>>& swarm2pks) {
  std::vector<size_t> counts;

  for (const auto& entry : swarm2pks) {
    std::cout << entry.second.size() << " ";
    counts.push_back(entry.second.size());
  }
  std::cout << "\n";
  std::cout << "standard deviation: " << standard_deviation(counts) << std::endl;
}

void after_testing_evaluate_swarm(char const *algorithm_name,
                                  std::map<public_key, service_node_info> const &all_snode_info,
                                  lifetime_stats const &lifetime_stat,
                                  Stats const &stat,
                                  std::vector<swarm_info> const &all_swarms)
{
  struct swarm_counts
  {
    int swarm_size;
    int count;
  };

  float avg_swarm_size = 0;
  std::vector<swarm_counts> swarm_sizes;

  int highest_move_count = -1;
  int lowest_move_count  = INT32_MAX;
  for (auto const &it : all_snode_info)
  {
    public_key const &key = it.first;
    service_node_info const &info = it.second;

    if (info.num_times_moved_swarms > highest_move_count)
      highest_move_count = info.num_times_moved_swarms;

    if (info.num_times_moved_swarms < lowest_move_count)
      lowest_move_count = info.num_times_moved_swarms;
  }

  for (size_t swarm_index = 0; swarm_index < all_swarms.size(); ++swarm_index)
  {
    swarm_info const *swarm = &all_swarms[swarm_index];
    avg_swarm_size += swarm->size;

    auto size_it =
        std::find_if(swarm_sizes.begin(), swarm_sizes.end(), [&swarm](swarm_counts &swarm_count) {
          return swarm_count.swarm_size == swarm->size;
        });

    if (size_it == swarm_sizes.end())
    {
      swarm_counts entry = {};
      entry.swarm_size   = swarm->size;
      entry.count++;
      swarm_sizes.push_back(entry);
    }
    else
    {
      ++size_it->count;
    }
  }

  const auto num_active = std::count_if(all_swarms.begin(), all_swarms.end(), [](const swarm_info& si){ return si.size >= MIN_SWARM_SIZE; });

  avg_swarm_size /= (float)all_swarms.size();
  printf("[%s]\n", algorithm_name);
  printf("  Num Swarms                      %zu/%zu\n", num_active, all_swarms.size());
  printf("  Avg Swarm Size                  %05.2f\n", avg_swarm_size);
  printf("  Num Nodes Stolen                %d\n", lifetime_stat.num_times_nodes_stolen);
  printf("  Num Swarm Count Changes         %d\n", lifetime_stat.num_swarm_count_changes);
  printf("  Num Times Nodes Moved Swarms    %d\n", (int)stat.movements); // TODO: Im treating this as a count over the lifetime of the entire test
  printf("  Num Times Swarm Died            %d\n", lifetime_stat.num_times_swarm_died);
  printf("  Highest Times Node Moved Swarms %d\n", highest_move_count);
  printf("  Lowest Times Node Moved Swarms  %d\n", lowest_move_count);

  std::sort(swarm_sizes.begin(), swarm_sizes.end(), [](swarm_counts const &a, swarm_counts const &b) {
    return !(a.swarm_size < b.swarm_size);
  });

  printf("\n  [Swarm Composition]\n");
  for (size_t swarm_index = 0; swarm_index < swarm_sizes.size(); ++swarm_index)
  {
    const auto it            = swarm_sizes.begin() + swarm_index;
    float percent_of_network = ((float)it->count / all_swarms.size()) * 100.0f;
    printf("    [%03zu] Swarm Size %02d  Count %04d (%05.2f%% of Network)\n",
           swarm_index,
           it->swarm_size,
           it->count,
           percent_of_network);
  }

  constexpr bool print_individual_swarms = false;
  if (print_individual_swarms)
  {
    printf("\n  [Swarm Details]\n");
    for (size_t swarm_index = 0; swarm_index < all_swarms.size(); ++swarm_index)
    {
      swarm_info const *swarm = &all_swarms[swarm_index];
      printf("    [%03zu] Swarm %021zu  Size %d\n", swarm_index, swarm->id, (int)swarm->size);
    }
  }

  printf("\n");
}

/// Thing to optimise for in swarms:

/// 1. randomness at swarm creation
/// 2. shuffling of nodes between swarms
/// 3. number of "inactive" nodes - done
/// 4. amount of data migration (don't know how to test yet)
int main(int argc, char **argv)
{
  int num_events = 10000;
  if (argc == 1)
  {
      // do nothing, run default num_events
  }
  else if (argc == 2)
  {
    num_events = atoi(argv[1]);
  }
  else
  {
    printf("Usage: swarm-testing [number of test events]");
    return 0;
  }

  // const auto events = read_events("sn_registration_data.txt");
  std::vector<Event> const events = generate_random_events(num_events);

  const std::vector<public_key> users = generate_random_users(10000);

  swarm_jcktm jcktm = {};

  constexpr bool RUN_JCKTM        = true;
  constexpr bool RUN_QUEUE_BUFFER = true;

  if (RUN_JCKTM)
  {
    for (Event const &event : events)
    {
      for (size_t tx_index = 0; tx_index < event.snode_events.size(); ++tx_index)
      {
        SnodeEvent const *snode_event = &event.snode_events[tx_index];
        if (snode_event->type == SnodeEvent::Type::REG)
        {
          jcktm.add_new_snode_to_swarm(snode_event->pubkey, event.block_hash, tx_index);
        }
        else if (snode_event->type == SnodeEvent::Type::DEREG)
        {
          jcktm.remove_snode_from_swarm(snode_event->pubkey);
        }
      }

      jcktm.after_all_add_and_remove_swarms(event.block_hash);
      if (jcktm.lifetime_stat.last_swarm_count != jcktm.m_swarms.size())
      {
        jcktm.lifetime_stat.last_swarm_count = jcktm.m_swarms.size();
        jcktm.lifetime_stat.num_swarm_count_changes++;
      }
    }
  }

  std::map<public_key, service_node_info> m_service_nodes_infos;
  swarms swarms_(m_service_nodes_infos);
  std::vector<Stats> stats;
  if (RUN_QUEUE_BUFFER)
  {
    constexpr bool MUTE_COUT = true;
    std::streambuf *old = std::cout.rdbuf();
    if (MUTE_COUT)
    {
        std::cout.rdbuf(nullptr);
    }

    uint64_t prev_h = 0;
    for (const auto e : events)
    {
      if (e.height > prev_h)
      {

        if (prev_h != 0)
        {
          stats.push_back({});
          auto prev_state = m_service_nodes_infos;
          swarms_.process_block(e.block_hash, stats.back());
          stats.back().movements = count_movements(prev_state, m_service_nodes_infos);
        }

        prev_h = e.height;
      }

      for (SnodeEvent const &snode_event : e.snode_events)
      {
        if (snode_event.type == SnodeEvent::Type::REG)
        {
          m_service_nodes_infos.insert({snode_event.pubkey, {}});
          swarms_.process_reg(snode_event.pubkey);
        }
        else if (snode_event.type == SnodeEvent::Type::DEREG)
        {
          swarms_.process_dereg(snode_event.pubkey);
          m_service_nodes_infos.erase(snode_event.pubkey);
        }
      }
    }

    if (MUTE_COUT)
    {
      std::cout.rdbuf(old);
    }
  }

  // Global information
  {
      printf("[Summary]\n");
      printf("  Num Events Generated    %d\n", num_events);
      printf("  Num Service Nodes       %zu\n", jcktm.m_service_nodes_infos.size());
      printf("  Num Registrations       %d\n", global_num_registrations);
      printf("  Num Deregistrations     %d\n", global_num_deregistrations);
      printf("  Min/Max Swarm Size      %d/%d\n", (int)MIN_SWARM_SIZE, (int)MAX_SWARM_SIZE);
      printf("\n");
  }

  if (RUN_JCKTM) // jcktm stats
  {
    std::vector<swarm_info> all_swarms = jcktm.get_swarms(swarm_jcktm::add_low_count_swarms::yes);
    after_testing_evaluate_swarm("Jcktm Algorithm", jcktm.m_service_nodes_infos, jcktm.lifetime_stat, jcktm.stats, all_swarms);
  }

  if (RUN_QUEUE_BUFFER) // queue algo stats
  {
    std::vector<swarm_info> all_swarms;
    {
      std::map<uint64_t, size_t> swarm_id_and_size;
      for (const auto &entry : m_service_nodes_infos)
        swarm_id_and_size[entry.second.swarm_id]++;

      all_swarms.reserve(swarm_id_and_size.size());
      for (const auto &entry : swarm_id_and_size)
      {
        swarm_info swarm = {};
        swarm.id         = entry.first;
        swarm.size       = static_cast<uint16_t>(entry.second);
        all_swarms.push_back(swarm);
      }
    }

    lifetime_stats lifetime_stat = {};
    Stats stats_tmp = {};
    after_testing_evaluate_swarm("QueueBuffer Algorithm", m_service_nodes_infos, lifetime_stat, stats_tmp, all_swarms);

    // accumulate stats
    size_t total_inactive  = 0;
    size_t total_movements = 0;
    for (const auto &s : stats)
    {
      total_inactive += s.inactive_count;
      total_movements += s.movements;
    }

    std::cout << "inactive nodes mean: " << total_inactive / stats.size() << std::endl;
    std::cout << "total movements: " << total_movements << std::endl;
  }

}

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <set>

#include "swarms.h"
#include "utils.h"

enum class EventType {
    REG, DEREG
};

struct Event
{
    EventType type;
    uint64_t height; /// block height
    std::string hash; /// block hash
    std::string pk; /// sn pub key
};


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

static int get_random_sn_id(std::mt19937_64& mt, const std::set<int>& registered) {

    int rnd_idx;
    do {

      rnd_idx = uniform_distribution_portable(mt, 1000000);

    } while (registered.find(rnd_idx) != registered.end());

    return rnd_idx;

}

std::vector<Event> generate_random_events() {

    std::vector<Event> result;

    std::mt19937_64 mt(0);

    std::set<int> registered;

    for (uint64_t idx = 0; idx < 10000; ++idx) {

        auto dice = uniform_distribution_portable(mt, 100);

        /// make register every 10th block
        if (dice % 10 == 0) {

            auto rnd_idx = get_random_sn_id(mt, registered);
            registered.insert(rnd_idx);
            result.push_back({EventType::REG, idx, std::to_string(idx), std::to_string(rnd_idx)});
        }

        /// make a deregister every 20th block
        if (!registered.empty() && dice % 20 == 0) {
            auto rnd_idx = uniform_distribution_portable(mt, registered.size());
            auto it = registered.begin();
            std::advance(it, rnd_idx);
            result.push_back({EventType::DEREG, idx, std::to_string(idx), std::to_string(*it)});
            registered.erase(it);
        }

    }

    return result;

}


size_t count_movements(const std::map<pub_key, service_node_info>& prev,
                       const std::map<pub_key, service_node_info>& cur)
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

/// Thing to optimise for in swarms:

/// 1. randomness at swarm creation
/// 2. shuffling of nodes between swarms
/// 3. number of "inactive" nodes - done
/// 4. amount of data migration (don't know how to test yet)

int main() {

    std::map<pub_key, service_node_info> m_service_nodes_infos;
    swarms swarms_(m_service_nodes_infos);

    // const auto events = read_events("sn_registration_data.txt");
    const auto events = generate_random_events();


    std::vector<Stats> stats;

    uint64_t prev_h = 0;
    for (const auto e : events) {
        
        if (e.height > prev_h) {

            if (prev_h != 0) {
                stats.push_back({});


                auto prev_state = m_service_nodes_infos;
                swarms_.process_block(e.hash, stats.back());

                stats.back().movements = count_movements(prev_state, m_service_nodes_infos);
            }

            prev_h = e.height;
        }

        if (e.type == EventType::REG) {
            m_service_nodes_infos.insert({e.pk, {}});
            swarms_.process_reg(e.pk);
        } else {
            swarms_.process_dereg(e.pk);
            m_service_nodes_infos.erase(e.pk);
        }
    }


    /// accumulate stats

    size_t total_inactive = 0;
    size_t total_movements = 0;
    for (const auto& s : stats) {
        total_inactive += s.inactive_count;
        total_movements += s.movements;
    }

    std::cout << "inactive nodes mean: " << total_inactive / stats.size() << std::endl;
    std::cout << "total movements: " << total_movements << std::endl;



}
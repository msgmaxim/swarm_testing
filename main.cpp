#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <set>
#include <mutex>

#include <string.h>
#include "swarms.h"
#include "utils.h"

pcg32_random_t global_rng = {0, 0x70eae936f6bca02d};

enum class EventType {
    NONE, REG, DEREG
};

struct Event
{
    EventType  type;
    uint64_t   height;
    hash32     block_hash;
    public_key snode_pubkey;
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

    std::vector<public_key> registered_snodes;
    for (uint64_t idx = 0; idx < 10000; ++idx) {

        auto dice = uniform_distribution_portable(mt, 100);

        Event new_event      = {};
        new_event.type       = EventType::NONE;
        new_event.height     = idx;
        new_event.block_hash = generate_block_hash();

        /// make register every 10th block
        if (dice % 10 == 0) {
            new_event.type = EventType::REG;
            secret_key dummy_key = {};
            generate_keys(new_event.snode_pubkey, dummy_key);
            registered_snodes.push_back(new_event.snode_pubkey);
            result.push_back(new_event);
        }

        /// make a deregister every 20th block
        if (!registered_snodes.empty() && dice % 20 == 0) {
            new_event.type = EventType::DEREG;
            auto rnd_idx = uniform_distribution_portable(mt, registered_snodes.size());
            auto it = registered_snodes.begin() + rnd_idx;
            new_event.snode_pubkey = *it;
            registered_snodes.erase(it);
        }

    }

    return result;

}

/// Thing to optimise for in swarms:

/// 1. randomness at swarm creation
/// 2. shuffling of nodes between swarms
/// 3. number of "inactive" nodes - done
/// 4. amount of data migration (don't know how to test yet)

int main() {

   {
     std::vector<Event> events = generate_random_events();
     swarm_jcktm jcktm = {};
     for (Event &event : events)
     {
       if (event.type == EventType::REG)
       {
         jcktm.add_new_snode_to_swarm(event.snode_pubkey, event.block_hash, 0);
       }
       else
       {
         jcktm.remove_snode_from_swarm(event.snode_pubkey);
       }
     }
   }

    std::map<public_key, service_node_info> m_service_nodes_infos;
    swarms swarms_(m_service_nodes_infos);

    // const auto events = read_events("sn_registration_data.txt");
    const auto events = generate_random_events();

    std::vector<Stats> stats;
    uint64_t prev_h = 0;
    for (const auto e : events) {
        
        if (e.height > prev_h) {

            if (prev_h != 0) {
                stats.push_back({});
                swarms_.process_block(e.block_hash, stats.back());
            }

            prev_h = e.height;
        }

        if (e.type == EventType::REG) {
            m_service_nodes_infos.insert({e.snode_pubkey, {}});
            swarms_.process_reg(e.snode_pubkey);
        } else {
            swarms_.process_dereg(e.snode_pubkey);
            m_service_nodes_infos.erase(e.snode_pubkey);
        }
    }

    const auto total_inactive = std::accumulate(stats.begin(), stats.end(), 0, [](int acc, Stats& s) {
        return acc + s.inactive_count;
    });

    std::cout << "inactive nodes mean: " << total_inactive / stats.size() << std::endl;
}

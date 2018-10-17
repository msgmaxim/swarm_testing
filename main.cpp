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

/// Thing to optimise for in swarms:

/// 1. randomness at swarm creation
/// 2. shuffling of nodes between swarms
/// 3. number of "inactive" nodes
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
                swarms_.process_block(e.hash, stats.back());
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


    const auto total_inactive = std::accumulate(stats.begin(), stats.end(), 0, [](int acc, Stats& s) {
        return acc + s.inactive_count;
    });

    std::cout << "inactive nodes mean: " << total_inactive / stats.size() << std::endl;



}
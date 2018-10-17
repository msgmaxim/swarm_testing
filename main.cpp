#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include "swarms.h"

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

int main() {

    std::map<pub_key, service_node_info> m_service_nodes_infos;
    swarms swarms_(m_service_nodes_infos);

    const auto events = read_events("sn_registration_data.txt");


    uint64_t prev_h = 0;
    for (const auto e : events) {
        
        if (e.height > prev_h) {

            if (prev_h != 0) {
                swarms_.process_block(e.hash);
            }

            prev_h = e.height;
        }

        if (e.type == EventType::REG) {
            m_service_nodes_infos[e.pk].swarm_id = 0; // assign to queue
            swarms_.process_reg(e.hash, e.pk);
        } else {
            m_service_nodes_infos.erase(e.pk);
            swarms_.process_dereg();
        }
    }




}
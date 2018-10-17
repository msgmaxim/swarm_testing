
#include <map>
#include <string>

using pub_key = std::string;
using SwarmID = uint64_t;

struct Stats {
    size_t inactive_count = 0; /// nodes not participating in an "active" swarm
    size_t movements = 0; /// difference in swarm composition compared to previous (if existed)
};

struct service_node_info {
    SwarmID swarm_id;
};

class swarms {

private:

    std::map<pub_key, service_node_info>& m_service_nodes_infos;
public:
    void process_reg(const std::string& pk);

    void process_dereg(const std::string& pk);
    swarms(std::map<pub_key, service_node_info>& infos);
    void process_block(const std::string& hash, Stats& stats);

};
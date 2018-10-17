
#include <map>
#include <string>

using pub_key = std::string;
using SwarmID = uint64_t;

struct service_node_info {
    SwarmID swarm_id;
};

class swarms {

private:

    std::map<pub_key, service_node_info>& m_service_nodes_infos;
public:
    void process_reg(const std::string& hash, const std::string& pk);

    void process_dereg();
    swarms(std::map<pub_key, service_node_info>& infos);
    void process_block(const std::string& hash);

};
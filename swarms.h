#ifndef SWARMS_H
#define SWARMS_H

#include <map>
#include <string>
#include <vector>
#include <string.h>
#include "utils.h"

using pub_key = std::string;
using SwarmID = uint64_t;

struct Stats {
    size_t inactive_count = 0; /// nodes not participating in an "active" swarm
    size_t movements = 0; /// difference in swarm composition 
};

struct service_node_info {
    SwarmID swarm_id;
};

struct swarm_info
{
  uint64_t id;
  uint16_t size;
};

class swarms {

private:

    std::map<public_key, service_node_info>& m_service_nodes_infos;
public:
    void process_reg(const public_key& pk);

    void process_dereg(const public_key& pk);
    swarms(std::map<public_key, service_node_info>& infos);
    void process_block(const hash32& hash, Stats& stats);

};

struct swarm_jcktm
{
    std::map<public_key, service_node_info> m_service_nodes_infos;

    enum struct add_low_count_swarms { no, yes, };
    std::vector<swarm_info> get_swarms               (add_low_count_swarms add) const;
    void                    add_new_snode_to_swarm   (public_key const &snode_public_key, hash32 const &block_hash, uint64_t tx_index);
    void                    remove_snode_from_swarm  (const public_key& snode_key);
};

#endif // SWARMS_H

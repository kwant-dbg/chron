#pragma once
#include <vector>
#include <string>
#include "DataTypes.h"
#include "robin_hood.h"

void runMultiCriteriaRaptor(int start_node, int end_node, const Time& start_time,
    const robin_hood::unordered_map<int, Stop>& stops,
    const robin_hood::unordered_map<int, std::vector<Transfer>>& transfers,
    const robin_hood::unordered_map<std::string, Trip>& trips,
    const robin_hood::unordered_map<int, std::vector<std::string>>& routes_at_stop,
    double max_transit_speed_mps,
    robin_hood::unordered_map<int, std::vector<Journey>>& final_profiles,
    robin_hood::unordered_map<int, robin_hood::unordered_map<int, Journey>>& predecessors);


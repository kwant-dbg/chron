#pragma once
#include <vector>
#include <string>
#include "DataTypes.h"
#include "robin_hood.h"

void run_raptor(int src, int dest, const Time& start_t,
    const robin_hood::unordered_map<int, Stop>& stops,
    const robin_hood::unordered_map<int, std::vector<Transfer>>& transfers,
    const robin_hood::unordered_map<std::string, std::vector<StopTime>>& trips,
    const robin_hood::unordered_map<int, std::vector<std::string>>& routes_at_stop,
    robin_hood::unordered_map<int, std::vector<Journey>>& profiles,
    robin_hood::unordered_map<int, robin_hood::unordered_map<int, Journey>>& preds);


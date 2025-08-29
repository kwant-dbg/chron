#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "Raptor.h"
#include "DataTypes.h"

using namespace std;

constexpr double WALKING_SPEED_MPS = 1.4;
constexpr double MAX_WALK_DISTANCE_METERS = 1500;
constexpr int MAX_TRIPS = 5;

void merge(vector<Journey>& profile, const Journey& new_journey) {
    for (const auto& existing : profile) {
        if (existing.arrival_time <= new_journey.arrival_time && existing.trips <= new_journey.trips) {
            return;
        }
    }
    profile.erase(remove_if(profile.begin(), profile.end(),
        [&](const Journey& existing) {
            return new_journey.arrival_time <= existing.arrival_time && new_journey.trips <= existing.trips;
        }),
        profile.end());
    auto it = lower_bound(profile.begin(), profile.end(), new_journey);
    profile.insert(it, new_journey);
}

void runMultiCriteriaRaptor(int start_node, int end_node, const Time& start_time,
    const robin_hood::unordered_map<int, Stop>& stops,
    const robin_hood::unordered_map<int, vector<Transfer>>& transfers,
    const robin_hood::unordered_map<string, vector<StopTime>>& trips,
    const robin_hood::unordered_map<int, vector<string>>& routes_at_stop,
    robin_hood::unordered_map<int, vector<Journey>>& final_profiles,
    robin_hood::unordered_map<int, robin_hood::unordered_map<int, Journey>>& predecessors) {

    vector<robin_hood::unordered_map<int, vector<Journey>>> profiles(MAX_TRIPS + 1);

    merge(profiles[0][start_node], { start_time, 0, start_time, -1, "Start" });
    const auto& start_stop_details = stops.at(start_node);

    for (const auto& stop_pair : stops) {
        double dist = haversine(start_stop_details.lat, start_stop_details.lon, stop_pair.second.lat, stop_pair.second.lon);
        if (dist <= MAX_WALK_DISTANCE_METERS && stop_pair.first != start_node) {
            int walk_secs = static_cast<int>(dist / WALKING_SPEED_MPS);
            Journey j = { Time::fromSeconds(start_time.toSeconds() + walk_secs), 0, start_time, start_node, "Walk" };
            merge(profiles[0][stop_pair.first], j);
        }
    }

    if (transfers.count(start_node)) {
        for (const auto& transfer : transfers.at(start_node)) {
            Journey j = { Time::fromSeconds(start_time.toSeconds() + transfer.duration_seconds), 0, start_time, start_node, "Walk" };
            merge(profiles[0][transfer.to_stop_id], j);
        }
    }

    for (int k = 1; k <= MAX_TRIPS; ++k) {
        robin_hood::unordered_map<int, vector<Journey>> q;
        for (const auto& p : profiles[k - 1]) {
            int stop_id = p.first;
            if (routes_at_stop.count(stop_id)) {
                for (const auto& trip_id : routes_at_stop.at(stop_id)) {
                    const auto& schedule = trips.at(trip_id);
                    int board_idx = -1;
                    for (size_t i = 0; i < schedule.size(); ++i) {
                        if (schedule[i].stop_id == stop_id) {
                            board_idx = i;
                            break;
                        }
                    }

                    if (board_idx != -1) {
                        Journey earliest_board;
                        earliest_board.from_stop_id = -1;
                        for (size_t i = board_idx; i < schedule.size(); ++i) {
                            const auto& stop_time = schedule[i];
                            if (profiles[k - 1].count(stop_time.stop_id)) {
                                for (const auto& prev_j : profiles[k - 1].at(stop_time.stop_id)) {
                                    if (prev_j.arrival_time <= stop_time.departure_time) {
                                        if (earliest_board.from_stop_id == -1 || prev_j.arrival_time < earliest_board.arrival_time) {
                                            earliest_board = prev_j;
                                        }
                                    }
                                }
                            }
                            if (earliest_board.from_stop_id != -1) {
                                int pred_id = (i > static_cast<size_t>(board_idx)) ? schedule[i - 1].stop_id : earliest_board.from_stop_id;
                                Journey new_j = { stop_time.arrival_time, k, earliest_board.departure_time, pred_id, "Trip " + trip_id };
                                merge(q[stop_time.stop_id], new_j);
                            }
                        }
                    }
                }
            }
        }

        for (const auto& p : q) {
            for (const auto& journey : p.second) {
                merge(profiles[k][p.first], journey);
                if (transfers.count(p.first)) {
                    for (const auto& transfer : transfers.at(p.first)) {
                        Journey transfer_j = { Time::fromSeconds(journey.arrival_time.toSeconds() + transfer.duration_seconds), journey.trips, journey.departure_time, p.first, "Walk" };
                        merge(profiles[k][transfer.to_stop_id], transfer_j);
                    }
                }
            }
        }
    }

    robin_hood::unordered_map<int, vector<Journey>> temp_final_profiles;
    for (int k = 0; k <= MAX_TRIPS; ++k) {
        for (const auto& p : profiles[k]) {
            for (const auto& journey : p.second) {
                merge(temp_final_profiles[p.first], journey);
            }
        }
    }

    const auto& end_stop_details = stops.at(end_node);
    for (const auto& p : temp_final_profiles) {
        if (p.first == end_node) continue;
        const auto& reached_stop_details = stops.at(p.first);
        double dist = haversine(reached_stop_details.lat, reached_stop_details.lon, end_stop_details.lat, end_stop_details.lon);
        if (dist <= MAX_WALK_DISTANCE_METERS) {
            int walk_secs = static_cast<int>(dist / WALKING_SPEED_MPS);
            for (const auto& journey : p.second) {
                Journey final_walk = { Time::fromSeconds(journey.arrival_time.toSeconds() + walk_secs), journey.trips, journey.departure_time, p.first, "Walk" };
                merge(final_profiles[end_node], final_walk);
            }
        }
    }

    for (const auto& p : temp_final_profiles) {
        for (const auto& journey : p.second) {
            merge(final_profiles[p.first], journey);
        }
    }

    for (const auto& p : final_profiles) {
        for (const auto& journey : p.second) {
            predecessors[p.first][journey.trips] = journey;
        }
    }
}


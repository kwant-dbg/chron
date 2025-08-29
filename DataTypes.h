#pragma once
#define _USE_MATH_DEFINES
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "robin_hood.h"

struct Stop {
    int id;
    std::string name;
    double lat, lon;
};

struct StopTime {
    std::string trip_id;
    struct Time arrival_time, departure_time;
    int stop_id, stop_sequence;
};

struct Trip {
    std::vector<StopTime> schedule;
    robin_hood::unordered_map<int, int> stop_to_idx; // Maps stop_id to its index
};

struct Transfer {
    int from_stop_id, to_stop_id, duration_seconds;
};

struct Time {
    int h, m, s;
    int toSeconds() const { return h * 3600 + m * 60 + s; }
    static Time fromSeconds(int total_s) {
        return { total_s / 3600, (total_s % 3600) / 60, total_s % 60 };
    }
    bool operator<=(const Time& other) const { return toSeconds() <= other.toSeconds(); }
    bool operator<(const Time& other) const { return toSeconds() < other.toSeconds(); }
};

struct Journey {
    Time arrival_time;
    int trips;
    Time departure_time;
    int from_stop_id;
    std::string method;
    bool operator<(const Journey& other) const {
        if (arrival_time.toSeconds() != other.arrival_time.toSeconds())
            return arrival_time < other.arrival_time;
        return trips < other.trips;
    }
};

inline double haversine(double lat1, double lon1, double lat2, double lon2) {
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;
    double a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
    return 6371 * 2 * asin(sqrt(a)) * 1000;
}


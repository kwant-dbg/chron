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

struct Time {
    int h, m, s;
    int to_secs() const { return h * 3600 + m * 60 + s; }
    static Time from_secs(int total_s) {
        return { total_s / 3600, (total_s % 3600) / 60, total_s % 60 };
    }
    bool operator<=(const Time& other) const { return to_secs() <= other.to_secs(); }
    bool operator<(const Time& other) const { return to_secs() < other.to_secs(); }
};

struct StopTime {
    std::string tid;
    Time arr, dep;
    int sid, seq;
};

struct Transfer {
    int u, v, dur;
};

struct Journey {
    Time arr, dep;
    int k; // trips
    int from;
    std::string meth;
    bool operator<(const Journey& other) const {
        if (arr.to_secs() != other.arr.to_secs())
            return arr < other.arr;
        return k < other.k;
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


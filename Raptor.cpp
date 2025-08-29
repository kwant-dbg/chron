#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <omp.h>
#include "Raptor.h"
#include "DataTypes.h"

using namespace std;

constexpr double WALK_V = 1.4;
constexpr double MAX_WALK = 1500;
constexpr int MAX_K = 5;

void merge(vector<Journey>& p, const Journey& nj) {
    for (const auto& ej : p) {
        if (ej.arr <= nj.arr && ej.k <= nj.k) {
            return;
        }
    }
    p.erase(remove_if(p.begin(), p.end(),
        [&](const Journey& ej) {
            return nj.arr <= ej.arr && nj.k <= ej.k;
        }),
        p.end());
    p.insert(lower_bound(p.begin(), p.end(), nj), nj);
}

void run_raptor(int src, int dest, const Time& start_t,
    const robin_hood::unordered_map<int, Stop>& stops,
    const robin_hood::unordered_map<int, vector<Transfer>>& transfers,
    const robin_hood::unordered_map<string, vector<StopTime>>& trips,
    const robin_hood::unordered_map<int, vector<string>>& routes_at_stop,
    robin_hood::unordered_map<int, vector<Journey>>& profiles,
    robin_hood::unordered_map<int, robin_hood::unordered_map<int, Journey>>& preds) {

    vector<robin_hood::unordered_map<int, vector<Journey>>> dp(MAX_K + 1);

    merge(dp[0][src], { start_t, start_t, 0, -1, "Start" });
    const auto& src_stop = stops.at(src);

    for (const auto& p : stops) {
        double dist = haversine(src_stop.lat, src_stop.lon, p.second.lat, p.second.lon);
        if (dist <= MAX_WALK && p.first != src) {
            int walk_t = static_cast<int>(dist / WALK_V);
            Journey j = { Time::from_secs(start_t.to_secs() + walk_t), start_t, 0, src, "Walk" };
            merge(dp[0][p.first], j);
        }
    }

    if (transfers.count(src)) {
        for (const auto& t : transfers.at(src)) {
            Journey j = { Time::from_secs(start_t.to_secs() + t.dur), start_t, 0, src, "Walk" };
            merge(dp[0][t.v], j);
        }
    }

    for (int k = 1; k <= MAX_K; ++k) {
        vector<int> marked;
        marked.reserve(dp[k - 1].size());
        for (const auto& p : dp[k - 1]) {
            marked.push_back(p.first);
        }

        vector<robin_hood::unordered_map<int, vector<Journey>>> local_q(omp_get_max_threads());

#pragma omp parallel for schedule(dynamic)
        for (size_t i = 0; i < marked.size(); ++i) {
            int sid = marked[i];
            int tid = omp_get_thread_num();

            if (routes_at_stop.count(sid)) {
                for (const auto& trip_id : routes_at_stop.at(sid)) {
                    const auto& sched = trips.at(trip_id);

                    int b_idx = -1;
                    for (size_t j = 0; j < sched.size(); ++j) {
                        if (sched[j].sid == sid) {
                            b_idx = j;
                            break;
                        }
                    }

                    if (b_idx != -1) {
                        Journey best_j;
                        best_j.from = -1;

                        for (size_t j = b_idx; j < sched.size(); ++j) {
                            const auto& st = sched[j];

                            if (dp[k - 1].count(st.sid)) {
                                for (const auto& pj : dp[k - 1].at(st.sid)) {
                                    if (pj.arr <= st.dep) {
                                        if (best_j.from == -1 || pj.arr < best_j.arr) {
                                            best_j = pj;
                                        }
                                    }
                                }
                            }

                            if (best_j.from != -1) {
                                int p_id = (j > static_cast<size_t>(b_idx)) ? sched[j - 1].sid : best_j.from;
                                Journey nj = { st.arr, best_j.dep, k, p_id, "Trip " + trip_id };
                                merge(local_q[tid][st.sid], nj);
                            }
                        }
                    }
                }
            }
        }

        robin_hood::unordered_map<int, vector<Journey>> q;
        for (const auto& lq : local_q) {
            for (const auto& p : lq) {
                for (const auto& j : p.second) {
                    merge(q[p.first], j);
                }
            }
        }

        for (const auto& p : q) {
            for (const auto& j : p.second) {
                merge(dp[k][p.first], j);
                if (transfers.count(p.first)) {
                    for (const auto& t : transfers.at(p.first)) {
                        Journey tj = { Time::from_secs(j.arr.to_secs() + t.dur), j.dep, j.k, p.first, "Walk" };
                        merge(dp[k][t.v], tj);
                    }
                }
            }
        }
    }

    robin_hood::unordered_map<int, vector<Journey>> res;
    for (int k = 0; k <= MAX_K; ++k) {
        for (const auto& p : dp[k]) {
            for (const auto& j : p.second) {
                merge(res[p.first], j);
            }
        }
    }

    const auto& dest_stop = stops.at(dest);
    for (const auto& p : res) {
        if (p.first == dest) continue;
        const auto& curr_stop = stops.at(p.first);
        double dist = haversine(curr_stop.lat, curr_stop.lon, dest_stop.lat, dest_stop.lon);
        if (dist <= MAX_WALK) {
            int walk_t = static_cast<int>(dist / WALK_V);
            for (const auto& j : p.second) {
                Journey fw = { Time::from_secs(j.arr.to_secs() + walk_t), j.dep, j.k, p.first, "Walk" };
                merge(profiles[dest], fw);
            }
        }
    }

    for (const auto& p : res) {
        for (const auto& j : p.second) {
            merge(profiles[p.first], j);
        }
    }

    for (const auto& p : profiles) {
        for (const auto& j : p.second) {
            preds[p.first][j.k] = j;
        }
    }
}


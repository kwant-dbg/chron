#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include "httplib.h"
#include "DataTypes.h"
#include "Raptor.h"
#include "robin_hood.h"

using namespace std;

void load_data(const string& dir,
    robin_hood::unordered_map<int, Stop>& stops,
    robin_hood::unordered_map<string, vector<StopTime>>& trips,
    robin_hood::unordered_map<int, vector<Transfer>>& transfers,
    robin_hood::unordered_map<int, vector<string>>& routes_at_stop,
    robin_hood::unordered_map<string, int>& name_to_id) {

    ifstream stops_file(dir + "/stops.txt");
    string line;
    getline(stops_file, line);
    while (getline(stops_file, line)) {
        stringstream ss(line);
        string field;
        Stop s;
        getline(ss, field, ','); s.id = stoi(field);
        getline(ss, field, ',');
        getline(ss, field, ','); s.name = field;
        getline(ss, field, ','); s.lat = stod(field);
        getline(ss, field, ','); s.lon = stod(field);
        stops[s.id] = s;
        name_to_id[s.name] = s.id;
    }

    ifstream stop_times_file(dir + "/stop_times.txt");
    getline(stop_times_file, line);
    while (getline(stop_times_file, line)) {
        stringstream ss(line);
        string field;
        StopTime st;
        getline(ss, field, ','); st.tid = field;
        getline(ss, field, ':'); st.arr.h = stoi(field);
        getline(ss, field, ':'); st.arr.m = stoi(field);
        getline(ss, field, ','); st.arr.s = stoi(field);
        getline(ss, field, ':'); st.dep.h = stoi(field);
        getline(ss, field, ':'); st.dep.m = stoi(field);
        getline(ss, field, ','); st.dep.s = stoi(field);
        getline(ss, field, ','); st.sid = stoi(field);
        getline(ss, field, ','); st.seq = stoi(field);
        trips[st.tid].push_back(st);
    }

    for (auto const& [tid, sched] : trips) {
        for (const auto& st : sched) {
            routes_at_stop[st.sid].push_back(tid);
        }
    }

    ifstream transfers_file(dir + "/transfers.txt");
    getline(transfers_file, line);
    while (getline(transfers_file, line)) {
        stringstream ss(line);
        string field;
        Transfer t;
        getline(ss, field, ','); t.u = stoi(field);
        getline(ss, field, ','); t.v = stoi(field);
        getline(ss, field, ',');
        getline(ss, field, ','); t.dur = stoi(field);
        transfers[t.u].push_back(t);
    }
    cout << "GTFS data loaded." << endl;
}

int main() {
    robin_hood::unordered_map<int, Stop> stops;
    robin_hood::unordered_map<string, vector<StopTime>> trips;
    robin_hood::unordered_map<int, vector<Transfer>> transfers;
    robin_hood::unordered_map<int, vector<string>> routes_at_stop;
    robin_hood::unordered_map<string, int> name_to_id;

    load_data("text", stops, trips, transfers, routes_at_stop, name_to_id);

    httplib::Server svr;
    svr.set_mount_point("/", "./web");

    svr.Get("/hi", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello World!", "text/plain");
        });

    svr.Post("/calculate", [&](const httplib::Request& req, httplib::Response& res) {
        string start_name = req.get_param_value("start");
        string end_name = req.get_param_value("end");
        Time start_t;
        sscanf(req.get_param_value("time").c_str(), "%d:%d", &start_t.h, &start_t.m);
        start_t.s = 0;

        if (name_to_id.find(start_name) == name_to_id.end() ||
            name_to_id.find(end_name) == name_to_id.end()) {
            res.set_content("{\"error\":\"Invalid stop name\"}", "application/json");
            return;
        }

        int src = name_to_id[start_name];
        int dest = name_to_id[end_name];

        robin_hood::unordered_map<int, vector<Journey>> profiles;
        robin_hood::unordered_map<int, robin_hood::unordered_map<int, Journey>> preds;
        run_raptor(src, dest, start_t, stops, transfers, trips, routes_at_stop, profiles, preds);

        string json = "{\"journeys\":[";
        if (profiles.count(dest)) {
            bool first_j = true;
            for (const auto& j : profiles.at(dest)) {
                if (!first_j) json += ",";
                json += "{";
                json += "\"arrival\":\"" + to_string(j.arr.h) + ":" + to_string(j.arr.m) + "\",";
                json += "\"trips\":" + to_string(j.k) + ",";
                json += "\"path\":[";

                vector<pair<int, string>> path;
                Journey curr = j;
                int curr_sid = dest;

                while (curr.from != -1) {
                    path.push_back({ curr_sid, curr.meth });
                    int prev_sid = curr.from;
                    int prev_k = curr.meth.find("Walk") != string::npos ? curr.k : curr.k - 1;

                    if (preds.count(prev_sid) && preds.at(prev_sid).count(prev_k)) {
                        curr = preds.at(prev_sid).at(prev_k);
                        curr_sid = prev_sid;
                    }
                    else {
                        break;
                    }
                }
                path.push_back({ src, "Start" });
                reverse(path.begin(), path.end());

                bool first_s = true;
                for (const auto& step : path) {
                    if (!first_s) json += ",";
                    const auto& s = stops.at(step.first);
                    json += "{";
                    json += "\"stop_name\":\"" + s.name + "\",";
                    json += "\"lat\":" + to_string(s.lat) + ",";
                    json += "\"lon\":" + to_string(s.lon) + ",";
                    json += "\"method\":\"" + step.second + "\"";
                    json += "}";
                    first_s = false;
                }
                json += "]}";
                first_j = false;
            }
        }
        json += "]}";
        res.set_content(json, "application/json");
        });

    cout << "Server starting on http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}


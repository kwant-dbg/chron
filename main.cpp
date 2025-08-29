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
    robin_hood::unordered_map<string, int>& stop_name_to_id) {

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
        stop_name_to_id[s.name] = s.id;
    }

    ifstream stop_times_file(dir + "/stop_times.txt");
    getline(stop_times_file, line);
    while (getline(stop_times_file, line)) {
        stringstream ss(line);
        string field;
        StopTime st;
        getline(ss, field, ','); st.trip_id = field;
        getline(ss, field, ':'); st.arrival_time.h = stoi(field);
        getline(ss, field, ':'); st.arrival_time.m = stoi(field);
        getline(ss, field, ','); st.arrival_time.s = stoi(field);
        getline(ss, field, ':'); st.departure_time.h = stoi(field);
        getline(ss, field, ':'); st.departure_time.m = stoi(field);
        getline(ss, field, ','); st.departure_time.s = stoi(field);
        getline(ss, field, ','); st.stop_id = stoi(field);
        getline(ss, field, ','); st.stop_sequence = stoi(field);
        trips[st.trip_id].push_back(st);
    }

    for (auto const& [trip_id, schedule] : trips) {
        for (const auto& stop_time : schedule) {
            routes_at_stop[stop_time.stop_id].push_back(trip_id);
        }
    }

    ifstream transfers_file(dir + "/transfers.txt");
    getline(transfers_file, line);
    while (getline(transfers_file, line)) {
        stringstream ss(line);
        string field;
        Transfer t;
        getline(ss, field, ','); t.from_stop_id = stoi(field);
        getline(ss, field, ','); t.to_stop_id = stoi(field);
        getline(ss, field, ',');
        getline(ss, field, ','); t.duration_seconds = stoi(field);
        transfers[t.from_stop_id].push_back(t);
    }
    cout << "GTFS data loaded." << endl;
}

int main() {
    robin_hood::unordered_map<int, Stop> stops;
    robin_hood::unordered_map<string, vector<StopTime>> trips;
    robin_hood::unordered_map<int, vector<Transfer>> transfers;
    robin_hood::unordered_map<int, vector<string>> routes_at_stop;
    robin_hood::unordered_map<string, int> stop_name_to_id;

    load_data("data", stops, trips, transfers, routes_at_stop, stop_name_to_id);

    httplib::Server svr;
    svr.set_mount_point("/", "./web");

    svr.Get("/hi", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello World!", "text/plain");
        });

    svr.Post("/calculate", [&](const httplib::Request& req, httplib::Response& res) {
        string start_stop_name = req.get_param_value("start");
        string end_stop_name = req.get_param_value("end");
        Time start_time;
        sscanf(req.get_param_value("time").c_str(), "%d:%d", &start_time.h, &start_time.m);
        start_time.s = 0;

        if (stop_name_to_id.find(start_stop_name) == stop_name_to_id.end() ||
            stop_name_to_id.find(end_stop_name) == stop_name_to_id.end()) {
            res.set_content("{\"error\":\"Invalid stop name\"}", "application/json");
            return;
        }

        int start_node = stop_name_to_id[start_stop_name];
        int end_node = stop_name_to_id[end_stop_name];

        robin_hood::unordered_map<int, vector<Journey>> final_profiles;
        robin_hood::unordered_map<int, robin_hood::unordered_map<int, Journey>> predecessors;
        runMultiCriteriaRaptor(start_node, end_node, start_time, stops, transfers, trips, routes_at_stop, final_profiles, predecessors);

        string json = "{\"journeys\":[";
        if (final_profiles.count(end_node)) {
            bool first_journey = true;
            for (const auto& j : final_profiles.at(end_node)) {
                if (!first_journey) json += ",";
                json += "{";
                json += "\"arrival\":\"" + to_string(j.arrival_time.h) + ":" + to_string(j.arrival_time.m) + "\",";
                json += "\"trips\":" + to_string(j.trips) + ",";
                json += "\"path\":[";

                vector<pair<int, string>> path;
                Journey current = j;
                int current_stop = end_node;

                while (current.from_stop_id != -1) {
                    path.push_back({ current_stop, current.method });
                    int prev_stop = current.from_stop_id;
                    int prev_trips = current.method.find("Walk") != string::npos ? current.trips : current.trips - 1;

                    if (predecessors.count(prev_stop) && predecessors.at(prev_stop).count(prev_trips)) {
                        current = predecessors.at(prev_stop).at(prev_trips);
                        current_stop = prev_stop;
                    }
                    else {
                        break;
                    }
                }
                path.push_back({ start_node, "Start" });
                reverse(path.begin(), path.end());

                bool first_step = true;
                for (const auto& step : path) {
                    if (!first_step) json += ",";
                    const auto& s = stops.at(step.first);
                    json += "{";
                    json += "\"stop_name\":\"" + s.name + "\",";
                    json += "\"lat\":" + to_string(s.lat) + ",";
                    json += "\"lon\":" + to_string(s.lon) + ",";
                    json += "\"method\":\"" + step.second + "\"";
                    json += "}";
                    first_step = false;
                }
                json += "]}";
                first_journey = false;
            }
        }
        json += "]}";
        res.set_content(json, "application/json");
        });

    cout << "Server starting on http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}


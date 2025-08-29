# Temporal Pathfinder 🗺️

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)  
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

An efficient public transit journey planner for **Delhi, India**, built with **C++** and powered by the **RAPTOR algorithm**.  
This project provides a **web-based interface** to find the fastest routes at specific times.

---

## 🌟 Key Features

- ⚡ **Fast & Efficient:** Utilizes the modern **RAPTOR** algorithm for rapid route calculations.  
- 🕒 **Time-Dependent:** Finds the best route based on your specified departure time.  
- 🌐 **Web Interface:** A clean, simple web UI for entering your start, destination, and time.  
- 📊 **Real-World Data:** Powered by the official GTFS transit data for Delhi.  
- 🏆 **Optimal Journeys:** Provides multiple journey options, prioritizing arrival time and minimizing transfers.  

---

## 💻 Live Demo & Screenshots

This is how the application looks in action. The interface allows users to input their journey details, and the map visualizes the resulting route options.

| Web Interface | Server Log |
|---------------|------------|
| ![UI Screenshot](img/Screenshot%202025-08-20%20005027.png) | ![Log Screenshot](img/Screenshot%202025-08-20%20005141.png) |


---

## 🧠 The Algorithm: RAPTOR

The core of this project is the **RAPTOR (Round-bAsed Public Transit Optimized Router)** algorithm.  

Unlike classic graph-based algorithms (like Dijkstra's), RAPTOR is **tailored for public transit systems**.  

- Works in **rounds**: each round `k` finds the earliest arrival times at stops with at most `k-1` transfers.  
- Designed for **large-scale transit networks**.  
- Prioritizes **realistic and optimal journeys**.  

---

## 🚀 Getting Started

Follow these instructions to get a local copy up and running.

### Prerequisites
- A C++ compiler that supports **C++11 or newer** (e.g., GCC/g++).  
- The **Delhi GTFS dataset**, available [here](https://mobilitydatabase.org/feeds/gtfs/mdb-1262).  

### Installation & Execution

1. **Clone the repository:**
   ```sh
   git clone https://github.com/L0calised/TemporalPathfinder01.git
   cd TemporalPathfinder01
sh

2. **Set up the data:**

   * Create a directory named `data` in the project root.
   * Download the GTFS files (`stops.txt`, `stop_times.txt`, `trips.txt`, etc.) and place them inside the `data` folder.

3. **Compile the source code:**

   ```sh
   g++ Sources/main.cpp Sources/Raptor.cpp -o pathfinder -IHeaders -std=c++11 -pthread
   ```

4. **Run the application:**

   ```sh
   ./pathfinder
   ```

   You should see:

   ```
   Server starting on http://localhost:8080
   ```

5. **Access the web interface:**
   Open your browser and go to 👉 **[http://localhost:8080](http://localhost:8080)**

---

## 📁 Project Structure

```
TemporalPathfinder/
├── Headers/
│   ├── DataTypes.h     # Defines data structures (Stop, Route, etc.)
│   ├── httplib.h       # Single-file C++ HTTP/HTTPS library
│   └── Raptor.h        # Header for the RAPTOR algorithm
└── Sources/
    ├── main.cpp        # Main application entry point and web server logic
    └── Raptor.cpp      # Implementation of the RAPTOR algorithm
```

## 📜 License

Distributed under the **MIT License**.
See the [`LICENSE`](LICENSE) file for more details.



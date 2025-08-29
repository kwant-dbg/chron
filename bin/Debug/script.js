document.addEventListener('DOMContentLoaded', () => {
    // --- Element References ---
    const startInput = document.getElementById('start-stop-input');
    const endInput = document.getElementById('end-stop-input');
    const startSuggestions = document.getElementById('start-suggestions');
    const endSuggestions = document.getElementById('end-suggestions');
    const timeInput = document.getElementById('start-time');
    const findRouteBtn = document.getElementById('find-route-btn');
    const resultsContainer = document.getElementById('results-container');

    // --- Map Initialization (Centered on Delhi) ---
    const map = L.map('map').setView([28.6139, 77.2090], 11);
    L.tileLayer('https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png', {
        maxZoom: 19,
        attribution: '© OpenStreetMap, © CARTO'
    }).addTo(map);

    let allStops = [];
    let selectedStartId = null;
    let selectedEndId = null;
    let stopMarkers = {};
    let currentRouteLayer = null;

    // --- Helper Functions ---
    function formatDuration(totalSeconds) {
        if (totalSeconds < 0) return 'N/A';
        const hours = Math.floor(totalSeconds / 3600);
        const minutes = Math.floor((totalSeconds % 3600) / 60);
        return `${hours > 0 ? hours + 'h ' : ''}${minutes}m`;
    }

    function timeToSeconds(timeStr) {
        if (!timeStr) return 0;
        const parts = timeStr.split(':');
        if (parts.length !== 3) return 0;
        const [h, m, s] = parts.map(Number);
        return h * 3600 + m * 60 + s;
    }

    // --- Data Fetching and Initial Map Plotting ---
    async function initialize() {
        try {
            const response = await fetch('/api/stops');
            allStops = await response.json();

            // Note: For very large datasets, you might not want to plot all markers at once.
            // But for now, this will work.
            allStops.forEach(stop => {
                if (stop.lat && stop.lon) {
                    const marker = L.marker([stop.lat, stop.lon], { opacity: 0.6, title: stop.name }).addTo(map);
                    stopMarkers[stop.id] = marker;
                }
            });
        } catch (error) {
            resultsContainer.innerHTML = '<p style="color: red;">Error connecting to backend.</p>';
        }
    }

    // --- Autocomplete Logic ---
    function showSuggestions(inputValue, suggestionsContainer, inputElement) {
        suggestionsContainer.innerHTML = '';
        if (!inputValue) return;
        const filteredStops = allStops.filter(stop =>
        stop.name.toLowerCase().includes(inputValue.toLowerCase())
        ).slice(0, 10);
        filteredStops.forEach(stop => {
            const div = document.createElement('div');
            div.textContent = stop.name;
            div.className = 'suggestion-item';
            div.addEventListener('click', () => {
                inputElement.value = stop.name;
                if (inputElement === startInput) selectedStartId = stop.id;
                if (inputElement === endInput) selectedEndId = stop.id;
                suggestionsContainer.innerHTML = '';
            });
            suggestionsContainer.appendChild(div);
        });
    }

    startInput.addEventListener('input', () => showSuggestions(startInput.value, startSuggestions, startInput));
    endInput.addEventListener('input', () => showSuggestions(endInput.value, endSuggestions, endInput));
    document.addEventListener('click', (e) => {
        if (!e.target.closest('.autocomplete-container')) {
            startSuggestions.innerHTML = '';
            endSuggestions.innerHTML = '';
        }
    });

    // --- Route Drawing and Results Display ---
    function drawRouteOnMap(path) {
        if (currentRouteLayer) map.removeLayer(currentRouteLayer);
        const routeLayers = [];
        let fullPathCoords = [];

        const startMarker = stopMarkers[selectedStartId];
        if (!startMarker) return;

        let lastCoords = startMarker.getLatLng();
        fullPathCoords.push(lastCoords);

        path.forEach(step => {
            const currentMarker = stopMarkers[step.stop_id];
            if (currentMarker) {
                const currentCoords = currentMarker.getLatLng();
                const segmentCoords = [lastCoords, currentCoords];
                const isWalk = step.method === "Walk";

                const polyline = L.polyline(segmentCoords, {
                    color: isWalk ? '#f542a4' : '#1a73e8',
                    weight: isWalk ? 4 : 6,
                    dashArray: isWalk ? '5, 10' : null
                });
                routeLayers.push(polyline);
                fullPathCoords.push(currentCoords);
                lastCoords = currentCoords;
            }
        });

        currentRouteLayer = L.layerGroup(routeLayers).addTo(map);
        if (fullPathCoords.length > 1) map.fitBounds(fullPathCoords, { padding: [50, 50] });
    }

    function displayResults(data) {
        resultsContainer.innerHTML = '';
        Object.values(stopMarkers).forEach(marker => marker.setOpacity(0.6));

        const journeys = data.results || [];
        if (journeys.length === 0) {
            resultsContainer.innerHTML = '<p>No path found for the selected criteria.</p>';
            return;
        }

        const table = document.createElement('table');
        table.innerHTML = `<thead><tr><th>Arrival Time</th><th>Travel Time</th><th>Trips</th></tr></thead><tbody></tbody>`;
        const tbody = table.querySelector('tbody');

        journeys.forEach(result => {
            if (!result.departure_time) return;
            const row = tbody.insertRow();
            const travelSeconds = timeToSeconds(result.arrival_time) - timeToSeconds(result.departure_time);

            row.innerHTML = `<td><b>${result.arrival_time}</b></td><td>${formatDuration(travelSeconds)}</td><td>${result.trips}</td>`;
            row.style.cursor = 'pointer';
            row.addEventListener('click', () => {
                if(result.path) drawRouteOnMap(result.path);
                tbody.querySelectorAll('tr').forEach(r => r.style.backgroundColor = '');
                row.style.backgroundColor = '#dde7f5';
            });
        });
        resultsContainer.appendChild(table);
    }

    // --- Find Route Button Event Listener ---
    findRouteBtn.addEventListener('click', async () => {
        if (!selectedStartId || !selectedEndId) {
            alert('Please select a valid start and end stop from the suggestions.');
            return;
        }

        const time = timeInput.value;
        resultsContainer.innerHTML = '<p>Searching...</p>';
        if (currentRouteLayer) map.removeLayer(currentRouteLayer);

        try {
            const response = await fetch(`/api/route?from=${selectedStartId}&to=${selectedEndId}&time=${time}`);
            const routeData = await response.json();
            displayResults(routeData);
        } catch (error) {
            resultsContainer.innerHTML = '<p style="color: red;">Error getting route from C++ backend.</p>';
        }
    });

    // --- Initial setup ---
    initialize();
});

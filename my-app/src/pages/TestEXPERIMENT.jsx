import React, { useState, useEffect } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { Play, Pause, Square, Download, Save, Settings, AlertCircle } from 'lucide-react';

function ExperimentInterface() {
  const [isRunning, setIsRunning] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [activeGraph, setActiveGraph] = useState('velocity');
  const [timeLimit, setTimeLimit] = useState(60);
  const [samplingRate, setSamplingRate] = useState(10);
  const [elapsedTime, setElapsedTime] = useState(0);
  const [showSettings, setShowSettings] = useState(false);

  // Simulated sensor data
  const [sensorData, setSensorData] = useState([]);
  const [graphData, setGraphData] = useState([]);

  // Generate random sensor readings (simulated)
  const generateReading = (time) => ({
    timestamp: time,
    velocity: (Math.sin(time / 10) * 5 + Math.random() * 2).toFixed(2),
    displacement: (time * 0.5 + Math.random() * 0.5).toFixed(2),
    acceleration: (Math.cos(time / 8) * 2 + Math.random() * 0.5).toFixed(2),
    temperature: (25 + Math.random() * 2).toFixed(1),
    pressure: (101.3 + Math.random() * 0.5).toFixed(2)
  });

  // Experiment timer
  useEffect(() => {
    let interval;
    if (isRunning && !isPaused && elapsedTime < timeLimit) {
      interval = setInterval(() => {
        setElapsedTime(prev => {
          const newTime = prev + (1000 / samplingRate);
          const reading = generateReading(newTime / 1000);

          setSensorData(prevData => [reading, ...prevData].slice(0, 20));
          setGraphData(prevData => [...prevData, {
            time: (newTime / 1000).toFixed(1),
            velocity: parseFloat(reading.velocity),
            displacement: parseFloat(reading.displacement),
            acceleration: parseFloat(reading.acceleration)
          }].slice(-50));

          return newTime;
        });
      }, 1000 / samplingRate);
    } else if (elapsedTime >= timeLimit && isRunning) {
      setIsRunning(false);
      setIsPaused(false);
    }
    return () => clearInterval(interval);
  }, [isRunning, isPaused, elapsedTime, timeLimit, samplingRate]);

  const handleStart = () => {
    if (elapsedTime === 0) {
      setSensorData([]);
      setGraphData([]);
    }
    setIsRunning(true);
    setIsPaused(false);
  };

  const handlePause = () => {
    setIsPaused(!isPaused);
  };

  const handleStop = () => {
    setIsRunning(false);
    setIsPaused(false);
    setElapsedTime(0);
    setSensorData([]);
    setGraphData([]);
  };

  const handleExportCSV = () => {
    const headers = ['Time', 'Velocity', 'Displacement', 'Acceleration', 'Temperature', 'Pressure'];
    const csvContent = [
      headers.join(','),
      ...sensorData.reverse().map(row =>
        `${row.timestamp.toFixed(2)},${row.velocity},${row.displacement},${row.acceleration},${row.temperature},${row.pressure}`
      )
    ].join('\n');

    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `experiment_data_${new Date().getTime()}.csv`;
    a.click();
  };

  const handleSaveToProfile = () => {
    // Implement save to profile functionality
    alert('Data saved to your profile!');
  };

  const getGraphColor = () => {
    switch(activeGraph) {
      case 'velocity': return '#667eea';
      case 'displacement': return '#22c55e';
      case 'acceleration': return '#f59e0b';
      default: return '#667eea';
    }
  };

  const progress = (elapsedTime / (timeLimit * 1000)) * 100;

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-200 p-6">
      {/* Header */}
      <div className="max-w-7xl mx-auto mb-6">
        <div className="bg-gradient-to-r from-purple-600 to-indigo-600 rounded-2xl p-6 text-white shadow-xl">
          <div className="flex items-center justify-between">
            <div>
              <h1 className="text-3xl font-bold mb-2">Motion Analysis Experiment</h1>
              <p className="opacity-90">Real-time kinematic data collection and visualization</p>
            </div>
            <button
              onClick={() => setShowSettings(!showSettings)}
              className="p-3 bg-white/20 hover:bg-white/30 rounded-xl transition-all"
            >
              <Settings size={24} />
            </button>
          </div>
        </div>
      </div>

      <div className="max-w-7xl mx-auto">
        {/* Settings Panel */}
        {showSettings && (
          <div className="bg-white rounded-2xl p-6 mb-6 shadow-lg border border-slate-200">
            <h3 className="text-xl font-bold text-slate-800 mb-4">Experiment Settings</h3>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div>
                <label className="block text-sm font-semibold text-slate-700 mb-2">
                  Time Limit (seconds)
                </label>
                <input
                  type="number"
                  value={timeLimit}
                  onChange={(e) => setTimeLimit(Number(e.target.value))}
                  disabled={isRunning}
                  className="w-full px-4 py-3 border-2 border-slate-200 rounded-xl focus:border-purple-500 focus:outline-none disabled:bg-slate-100"
                  min="10"
                  max="300"
                />
              </div>
              <div>
                <label className="block text-sm font-semibold text-slate-700 mb-2">
                  Sampling Rate (Hz)
                </label>
                <input
                  type="number"
                  value={samplingRate}
                  onChange={(e) => setSamplingRate(Number(e.target.value))}
                  disabled={isRunning}
                  className="w-full px-4 py-3 border-2 border-slate-200 rounded-xl focus:border-purple-500 focus:outline-none disabled:bg-slate-100"
                  min="1"
                  max="100"
                />
              </div>
            </div>
          </div>
        )}

        {/* Control Panel */}
        <div className="bg-white rounded-2xl p-6 mb-6 shadow-lg border border-slate-200">
          <div className="flex flex-col md:flex-row items-center justify-between gap-4">
            {/* Timer Display */}
            <div className="flex items-center gap-4">
              <div className="text-center">
                <div className="text-4xl font-bold text-slate-800">
                  {Math.floor(elapsedTime / 1000)}s
                </div>
                <div className="text-sm text-slate-500">Elapsed Time</div>
              </div>
              <div className="h-16 w-px bg-slate-200" />
              <div className="text-center">
                <div className="text-2xl font-bold text-purple-600">
                  {timeLimit}s
                </div>
                <div className="text-sm text-slate-500">Total Time</div>
              </div>
            </div>

            {/* Control Buttons */}
            <div className="flex gap-3">
              {!isRunning ? (
                <button
                  onClick={handleStart}
                  className="flex items-center gap-2 px-6 py-3 bg-gradient-to-r from-green-500 to-emerald-600 text-white rounded-xl font-semibold hover:shadow-lg transition-all hover:-translate-y-0.5"
                >
                  <Play size={20} />
                  Start
                </button>
              ) : (
                <button
                  onClick={handlePause}
                  className="flex items-center gap-2 px-6 py-3 bg-gradient-to-r from-yellow-500 to-orange-600 text-white rounded-xl font-semibold hover:shadow-lg transition-all hover:-translate-y-0.5"
                >
                  <Pause size={20} />
                  {isPaused ? 'Resume' : 'Pause'}
                </button>
              )}
              <button
                onClick={handleStop}
                disabled={!isRunning && elapsedTime === 0}
                className="flex items-center gap-2 px-6 py-3 bg-gradient-to-r from-red-500 to-rose-600 text-white rounded-xl font-semibold hover:shadow-lg transition-all hover:-translate-y-0.5 disabled:opacity-50 disabled:cursor-not-allowed"
              >
                <Square size={20} />
                Stop
              </button>
            </div>
          </div>

          {/* Progress Bar */}
          <div className="mt-4">
            <div className="h-2 bg-slate-200 rounded-full overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-purple-600 to-indigo-600 transition-all duration-300"
                style={{ width: `${Math.min(progress, 100)}%` }}
              />
            </div>
          </div>
        </div>

        {/* Main Content Grid */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {/* Graph Section */}
          <div className="lg:col-span-2 bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
            <div className="flex items-center justify-between mb-6">
              <h3 className="text-xl font-bold text-slate-800">Live Data Visualization</h3>
              <div className="flex gap-2">
                <button
                  onClick={() => setActiveGraph('velocity')}
                  className={`px-4 py-2 rounded-lg font-semibold transition-all ${
                    activeGraph === 'velocity'
                      ? 'bg-purple-600 text-white shadow-md'
                      : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                  }`}
                >
                  V-t
                </button>
                <button
                  onClick={() => setActiveGraph('displacement')}
                  className={`px-4 py-2 rounded-lg font-semibold transition-all ${
                    activeGraph === 'displacement'
                      ? 'bg-green-600 text-white shadow-md'
                      : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                  }`}
                >
                  S-t
                </button>
                <button
                  onClick={() => setActiveGraph('acceleration')}
                  className={`px-4 py-2 rounded-lg font-semibold transition-all ${
                    activeGraph === 'acceleration'
                      ? 'bg-orange-600 text-white shadow-md'
                      : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                  }`}
                >
                  a-t
                </button>
              </div>
            </div>

            <div className="h-96 bg-slate-50 rounded-xl p-4">
              {graphData.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={graphData}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
                    <XAxis
                      dataKey="time"
                      label={{ value: 'Time (s)', position: 'insideBottom', offset: -5 }}
                      stroke="#64748b"
                    />
                    <YAxis
                      label={{ value: activeGraph === 'velocity' ? 'Velocity (m/s)' : activeGraph === 'displacement' ? 'Displacement (m)' : 'Acceleration (m/s²)', angle: -90, position: 'insideLeft' }}
                      stroke="#64748b"
                    />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#fff', border: '1px solid #e2e8f0', borderRadius: '8px' }}
                    />
                    <Legend />
                    <Line
                      type="monotone"
                      dataKey={activeGraph}
                      stroke={getGraphColor()}
                      strokeWidth={2}
                      dot={false}
                      name={activeGraph.charAt(0).toUpperCase() + activeGraph.slice(1)}
                    />
                  </LineChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex items-center justify-center text-slate-400">
                  <div className="text-center">
                    <AlertCircle size={48} className="mx-auto mb-2" />
                    <p>Start the experiment to see live data</p>
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* Sensor Data Table */}
          <div className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
            <h3 className="text-xl font-bold text-slate-800 mb-4">Sensor Readings</h3>
            <div className="overflow-y-auto max-h-96">
              {sensorData.length > 0 ? (
                <table className="w-full text-sm">
                  <thead className="sticky top-0 bg-slate-100">
                    <tr>
                      <th className="text-left p-2 font-semibold text-slate-700">Time</th>
                      <th className="text-right p-2 font-semibold text-slate-700">Value</th>
                    </tr>
                  </thead>
                  <tbody>
                    {sensorData.map((reading, idx) => (
                      <React.Fragment key={idx}>
                        <tr className="border-t border-slate-100">
                          <td className="p-2 text-slate-600 font-medium" colSpan={2}>
                            {reading.timestamp.toFixed(2)}s
                          </td>
                        </tr>
                        <tr className="bg-purple-50">
                          <td className="pl-4 py-1 text-slate-600">Velocity</td>
                          <td className="text-right pr-2 py-1 font-mono text-purple-700">{reading.velocity} m/s</td>
                        </tr>
                        <tr className="bg-green-50">
                          <td className="pl-4 py-1 text-slate-600">Displacement</td>
                          <td className="text-right pr-2 py-1 font-mono text-green-700">{reading.displacement} m</td>
                        </tr>
                        <tr className="bg-orange-50">
                          <td className="pl-4 py-1 text-slate-600">Acceleration</td>
                          <td className="text-right pr-2 py-1 font-mono text-orange-700">{reading.acceleration} m/s²</td>
                        </tr>
                        <tr className="bg-blue-50">
                          <td className="pl-4 py-1 text-slate-600">Temperature</td>
                          <td className="text-right pr-2 py-1 font-mono text-blue-700">{reading.temperature} °C</td>
                        </tr>
                        <tr className="bg-slate-50">
                          <td className="pl-4 py-1 text-slate-600">Pressure</td>
                          <td className="text-right pr-2 py-1 font-mono text-slate-700">{reading.pressure} kPa</td>
                        </tr>
                      </React.Fragment>
                    ))}
                  </tbody>
                </table>
              ) : (
                <div className="text-center text-slate-400 py-8">
                  <p>No data recorded yet</p>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Export Actions */}
        <div className="mt-6 bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
          <h3 className="text-xl font-bold text-slate-800 mb-4">Export & Save</h3>
          <div className="flex flex-col sm:flex-row gap-4">
            <button
              onClick={handleExportCSV}
              disabled={sensorData.length === 0}
              className="flex-1 flex items-center justify-center gap-2 px-6 py-3 bg-gradient-to-r from-blue-600 to-cyan-600 text-white rounded-xl font-semibold hover:shadow-lg transition-all hover:-translate-y-0.5 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Download size={20} />
              Export as CSV
            </button>
            <button
              onClick={handleSaveToProfile}
              disabled={sensorData.length === 0}
              className="flex-1 flex items-center justify-center gap-2 px-6 py-3 bg-gradient-to-r from-purple-600 to-indigo-600 text-white rounded-xl font-semibold hover:shadow-lg transition-all hover:-translate-y-0.5 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <Save size={20} />
              Save to Profile
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

export default ExperimentInterface;
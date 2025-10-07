import React, { useState, useEffect, useMemo, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, BarChart, Bar } from 'recharts';
import { Play, Pause, Square, Download, Save, Settings, AlertCircle, RefreshCcw, Wifi, WifiOff, Activity, Loader, CheckCircle, X, RotateCcw } from 'lucide-react';

const BACKEND_URL = `http://${window.location.hostname.replace(':3000', '')}:5000`;
const RECONNECT_DELAY = 3000;
const CONNECTION_CHECK_INTERVAL = 5000;

function OSIInterface() {
  const [isRunning, setIsRunning] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [elapsedTime, setElapsedTime] = useState(0);
  const [timeLimit, setTimeLimit] = useState(60);
  const [samplingRate, setSamplingRate] = useState(10);
  const [showConfigModal, setShowConfigModal] = useState(false);
  
  const [allDataPoints, setAllDataPoints] = useState([]);
  const [currentCount, setCurrentCount] = useState(0);
  const [maxCount, setMaxCount] = useState(0);
  
  const [esp32Connected, setEsp32Connected] = useState(false);
  const [esp32Info, setEsp32Info] = useState(null);
  const [isCheckingConnection, setIsCheckingConnection] = useState(false);
  const [error, setError] = useState(null);
  const [isConnecting, setIsConnecting] = useState(false);
  const [statusMessages, setStatusMessages] = useState([]);

  const token = localStorage.getItem('token');
  const eventSourceRef = useRef(null);
  const reconnectTimeoutRef = useRef(null);

  // Auto-dismiss status messages
  const addStatusMessage = (message, type = 'info') => {
    const id = Date.now();
    setStatusMessages(prev => [...prev, { id, message, type }]);
    setTimeout(() => {
      setStatusMessages(prev => prev.filter(msg => msg.id !== id));
    }, 3000);
  };

  // Check ESP32 connection
  useEffect(() => {
    const checkConnection = async () => {
      if (isRunning || !token) return;
      setIsCheckingConnection(true);
      try {
        const response = await fetch(`${BACKEND_URL}/api/osi/status`, {
          headers: { 'Authorization': `Bearer ${token}` },
        });
        if (response.ok) {
          const data = await response.json();
          const wasConnected = esp32Connected;
          setEsp32Connected(data.status.connected);
          setEsp32Info(data.status.device_info);
          setCurrentCount(data.status.current_count || 0);
          setError(data.status.connected ? null : 'ESP32 not connected');
          
          if (!wasConnected && data.status.connected) {
            addStatusMessage('OSI Sensor Connected', 'success');
          }
        } else {
          setEsp32Connected(false);
          setError('Failed to check sensor status');
        }
      } catch (err) {
        setEsp32Connected(false);
        setError('Cannot reach backend server');
      } finally {
        setIsCheckingConnection(false);
      }
    };

    checkConnection();
    const interval = setInterval(checkConnection, CONNECTION_CHECK_INTERVAL);
    return () => clearInterval(interval);
  }, [token, isRunning]);

  // Parse SSE data
  const parseReading = (sseData) => {
    try {
      const data = JSON.parse(sseData);
      return {
        time: data.time,
        count: data.count,
        sample: data.sample
      };
    } catch (err) {
      console.error('Error parsing SSE data:', err);
      return null;
    }
  };

  // SSE Connection
  useEffect(() => {
    const connectSSE = () => {
      if (!token || !esp32Connected || eventSourceRef.current) return;

      setIsConnecting(true);
      setError(null);

      const eventSource = new EventSource(`${BACKEND_URL}/api/osi/stream?token=${token}`);
      eventSourceRef.current = eventSource;

      eventSource.onmessage = (event) => {
        const reading = parseReading(event.data);
        if (!reading) return;

        setElapsedTime(reading.time * 1000);
        setCurrentCount(reading.count);
        setMaxCount(prev => Math.max(prev, reading.count));
        
        setAllDataPoints((prev) => [...prev, {
          time: reading.time,
          count: reading.count,
        }]);

        if (reading.time >= timeLimit) {
          handleStop();
        }
      };

      eventSource.onerror = (err) => {
        console.error('SSE error:', err);
        eventSource.close();
        eventSourceRef.current = null;
        setError('Connection lost. Reconnecting...');
        addStatusMessage('Connection lost. Reconnecting...', 'warning');
        setIsConnecting(false);
        reconnectTimeoutRef.current = setTimeout(connectSSE, RECONNECT_DELAY);
      };

      eventSource.onopen = () => {
        setIsConnecting(false);
        setError(null);
        addStatusMessage('Data stream connected', 'success');
      };
    };

    const disconnectSSE = () => {
      if (eventSourceRef.current) {
        eventSourceRef.current.close();
        eventSourceRef.current = null;
      }
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
        reconnectTimeoutRef.current = null;
      }
    };

    if (isRunning && !isPaused) {
      connectSSE();
    } else {
      disconnectSSE();
    }

    return disconnectSSE;
  }, [isRunning, isPaused, timeLimit, token, esp32Connected]);

  const handleConfigureExperiment = async () => {
    if (!esp32Connected) {
      addStatusMessage('ESP32 not connected. Please check hardware.', 'error');
      return false;
    }

    try {
      const response = await fetch(`${BACKEND_URL}/api/osi/configure`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${token}`,
        },
        body: JSON.stringify({
          frequency: samplingRate,
          duration: timeLimit
        }),
      });

      if (response.ok) {
        setShowConfigModal(false);
        addStatusMessage('Experiment configured successfully', 'success');
        return true;
      } else {
        addStatusMessage('Failed to configure experiment', 'error');
        return false;
      }
    } catch (err) {
      addStatusMessage('Error configuring experiment: ' + err.message, 'error');
      return false;
    }
  };

  const handleStart = async () => {
    if (!esp32Connected) {
      addStatusMessage('ESP32 not connected. Please check hardware.', 'error');
      return;
    }

    const configured = await handleConfigureExperiment();
    if (!configured) return;

    try {
      const response = await fetch(`${BACKEND_URL}/api/osi/start`, {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` },
      });

      if (!response.ok) {
        throw new Error('Failed to start experiment');
      }

      if (elapsedTime === 0) {
        setAllDataPoints([]);
        setCurrentCount(0);
        setMaxCount(0);
      }

      setIsRunning(true);
      setIsPaused(false);
      setError(null);
      addStatusMessage('Counting started', 'success');
    } catch (err) {
      setError('Failed to start: ' + err.message);
      addStatusMessage('Failed to start: ' + err.message, 'error');
    }
  };

  const handlePause = () => {
    setIsPaused(!isPaused);
    addStatusMessage(isPaused ? 'Counting resumed' : 'Counting paused', 'info');
  };

  const handleStop = async () => {
    try {
      await fetch(`${BACKEND_URL}/api/osi/stop`, {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` },
      });
      addStatusMessage(`Counting stopped. Final count: ${currentCount}`, 'info');
    } catch (err) {
      console.error('Error stopping:', err);
    }
    setIsRunning(false);
    setIsPaused(false);
    setElapsedTime(0);
    setError(null);
  };

  const handleReset = async () => {
    try {
      await fetch(`${BACKEND_URL}/api/osi/reset`, {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` },
      });
      setAllDataPoints([]);
      setCurrentCount(0);
      setMaxCount(0);
      setElapsedTime(0);
      setError(null);
      addStatusMessage('Counter reset', 'info');
    } catch (err) {
      addStatusMessage('Reset failed: ' + err.message, 'error');
    }
  };

  const handleExportCSV = () => {
    if (allDataPoints.length === 0) return;

    const headers = ['Time (s)', 'Count'];
    const csvContent = [
      headers.join(','),
      ...allDataPoints.map(row => `${row.time.toFixed(3)},${row.count}`)
    ].join('\n');

    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `oscillation_count_${new Date().toISOString()}.csv`;
    a.click();
    window.URL.revokeObjectURL(url);
    addStatusMessage('CSV exported successfully', 'success');
  };

  const handleSaveToProfile = async () => {
    if (!token || allDataPoints.length === 0) {
      addStatusMessage(allDataPoints.length === 0 ? 'No data to save.' : 'Please log in.', 'warning');
      return;
    }

    try {
      const response = await fetch(`${BACKEND_URL}/api/osi/save_data`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${token}`,
        },
        body: JSON.stringify({
          data: allDataPoints,
          metadata: {
            duration: timeLimit,
            samplingRate: samplingRate,
            timestamp: new Date().toISOString(),
            sensorType: 'OSI',
            finalCount: currentCount,
            maxCount: maxCount
          }
        }),
      });

      if (response.ok) {
        addStatusMessage('Data saved to your profile!', 'success');
      } else {
        addStatusMessage('Failed to save data.', 'error');
      }
    } catch (err) {
      addStatusMessage('Error saving: ' + err.message, 'error');
    }
  };

  const recentTableData = useMemo(() => {
    return allDataPoints.slice(-20).reverse();
  }, [allDataPoints]);

  const progress = Math.min((elapsedTime / (timeLimit * 1000)) * 100, 100);

  // Calculate oscillation frequency (counts per second)
  const oscillationFrequency = useMemo(() => {
    if (elapsedTime === 0) return 0;
    return (currentCount / (elapsedTime / 1000)).toFixed(2);
  }, [currentCount, elapsedTime]);

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 via-blue-50 to-slate-100 p-2 sm:p-4 pt-16 sm:pt-20">
      
      {/* Status Messages Toast */}
      <div className="fixed top-16 sm:top-20 right-2 sm:right-4 z-50 space-y-2 max-w-xs sm:max-w-sm">
        {statusMessages.map((msg) => (
          <div
            key={msg.id}
            className={`px-4 py-3 rounded-xl shadow-lg flex items-center gap-2 animate-slide-in ${
              msg.type === 'success' ? 'bg-green-50 border-2 border-green-300 text-green-800' :
              msg.type === 'error' ? 'bg-red-50 border-2 border-red-300 text-red-800' :
              msg.type === 'warning' ? 'bg-yellow-50 border-2 border-yellow-300 text-yellow-800' :
              'bg-blue-50 border-2 border-blue-300 text-blue-800'
            }`}
          >
            {msg.type === 'success' && <CheckCircle size={18} />}
            {msg.type === 'error' && <AlertCircle size={18} />}
            {msg.type === 'warning' && <AlertCircle size={18} />}
            {msg.type === 'info' && <Loader size={18} className="animate-spin" />}
            <span className="text-sm font-semibold flex-1">{msg.message}</span>
          </div>
        ))}
      </div>

      {/* Config Modal */}
      {showConfigModal && (
        <div className="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-3xl p-6 sm:p-8 max-w-lg w-full shadow-2xl">
            <h2 className="text-2xl sm:text-3xl font-bold text-slate-800 mb-4 sm:mb-6">Experiment Settings</h2>
            <div className="space-y-4 sm:space-y-6">
              <div>
                <label className="block text-sm font-bold text-slate-700 mb-2 sm:mb-3">
                  Time Limit (seconds)
                </label>
                <input
                  type="number"
                  value={timeLimit}
                  onChange={(e) => setTimeLimit(Number(e.target.value))}
                  disabled={isRunning}
                  className="w-full px-4 sm:px-5 py-3 sm:py-4 border-2 border-slate-200 rounded-xl focus:border-blue-500 focus:ring-2 focus:ring-blue-200 focus:outline-none disabled:bg-slate-100 transition-all text-base sm:text-lg font-semibold"
                  min="10"
                  max="300"
                />
              </div>
              <div>
                <label className="block text-sm font-bold text-slate-700 mb-2 sm:mb-3">
                  Sampling Rate (Hz)
                </label>
                <input
                  type="number"
                  value={samplingRate}
                  onChange={(e) => setSamplingRate(Number(e.target.value))}
                  disabled={isRunning}
                  className="w-full px-4 sm:px-5 py-3 sm:py-4 border-2 border-slate-200 rounded-xl focus:border-blue-500 focus:ring-2 focus:ring-blue-200 focus:outline-none disabled:bg-slate-100 transition-all text-base sm:text-lg font-semibold"
                  min="1"
                  max="100"
                />
              </div>
            </div>
            <div className="mt-6 sm:mt-8 flex flex-col sm:flex-row gap-3 sm:gap-4">
              <button
                onClick={handleConfigureExperiment}
                className="flex-1 px-4 sm:px-6 py-3 sm:py-4 bg-gradient-to-r from-blue-600 to-cyan-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-0.5 text-sm sm:text-base"
              >
                Save & Configure
              </button>
              <button
                onClick={() => setShowConfigModal(false)}
                className="flex-1 px-4 sm:px-6 py-3 sm:py-4 bg-slate-200 text-slate-700 rounded-xl font-bold hover:bg-slate-300 transition-all text-sm sm:text-base"
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      <div className="max-w-7xl mx-auto">
        {/* Header */}
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 sm:gap-4 mb-4 sm:mb-6">
          <h1 className="text-2xl sm:text-4xl font-bold bg-gradient-to-r from-blue-600 to-cyan-600 bg-clip-text text-transparent">
            Oscillation Counter
          </h1>
          <div className="flex items-center gap-2 sm:gap-3 bg-white px-3 sm:px-5 py-2 sm:py-3 rounded-xl shadow-md border border-slate-200">
            {esp32Connected ? (
              <Wifi size={20} className="text-green-500 flex-shrink-0" />
            ) : (
              <WifiOff size={20} className="text-red-500 flex-shrink-0" />
            )}
            <div className="min-w-0 flex-1">
              <div className="text-xs sm:text-sm font-bold text-slate-800 truncate">
                {esp32Connected ? 'OSI Sensor Ready' : 'Disconnected'}
              </div>
              {esp32Info && (
                <div className="text-xs text-slate-500 truncate">
                  {esp32Info.sensor_type}
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Big Counter Display */}
        <div className="bg-gradient-to-br from-white to-blue-50 rounded-3xl p-6 sm:p-10 mb-6 shadow-2xl border-2 border-blue-200">
          <div className="text-center">
            <div className="text-xs sm:text-sm font-bold text-slate-500 mb-2">CURRENT COUNT</div>
            <div className="text-6xl sm:text-9xl font-bold bg-gradient-to-r from-blue-600 to-cyan-600 bg-clip-text text-transparent mb-4 animate-pulse">
              {currentCount}
            </div>
            <div className="grid grid-cols-2 gap-4 max-w-md mx-auto">
              <div className="bg-white rounded-xl p-4 shadow-md">
                <div className="text-xs font-bold text-slate-500">MAX COUNT</div>
                <div className="text-2xl sm:text-3xl font-bold text-blue-600">{maxCount}</div>
              </div>
              <div className="bg-white rounded-xl p-4 shadow-md">
                <div className="text-xs font-bold text-slate-500">FREQUENCY</div>
                <div className="text-2xl sm:text-3xl font-bold text-cyan-600">{oscillationFrequency} Hz</div>
              </div>
            </div>
          </div>
        </div>

        {/* Control Panel */}
        <div className="bg-white rounded-2xl sm:rounded-3xl p-4 sm:p-6 mb-4 sm:mb-6 shadow-xl border border-slate-200">
          <div className="flex flex-col lg:flex-row items-stretch lg:items-center justify-between gap-4 sm:gap-6">
            <div className="flex items-center gap-4 sm:gap-8 justify-center lg:justify-start">
              <div className="text-center">
                <div className="text-3xl sm:text-5xl font-bold text-blue-600">
                  {Math.floor(elapsedTime / 1000)}s
                </div>
                <div className="text-xs sm:text-sm text-slate-500 font-semibold mt-1">Elapsed</div>
              </div>
              <div className="h-12 sm:h-16 w-px bg-slate-300" />
              <div className="text-center">
                <div className="text-2xl sm:text-3xl font-bold text-slate-800">
                  {timeLimit}s
                </div>
                <div className="text-xs sm:text-sm text-slate-500 font-semibold mt-1">Total</div>
              </div>
            </div>

            <div className="grid grid-cols-2 sm:flex gap-2 sm:gap-3">
              {!isRunning ? (
                <>
                  <button
                    onClick={() => setShowConfigModal(true)}
                    disabled={!esp32Connected}
                    className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-indigo-500 to-blue-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 disabled:opacity-50 text-xs sm:text-sm"
                  >
                    <Settings size={16} className="sm:w-5 sm:h-5" />
                    <span className="hidden sm:inline">Configure</span>
                  </button>
                  <button
                    onClick={handleStart}
                    disabled={!esp32Connected}
                    className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-green-500 to-emerald-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 disabled:opacity-50 text-xs sm:text-sm"
                  >
                    <Play size={16} className="sm:w-5 sm:h-5" />
                    Start
                  </button>
                </>
              ) : (
                <button
                  onClick={handlePause}
                  className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-yellow-500 to-orange-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 text-xs sm:text-sm"
                >
                  <Pause size={16} className="sm:w-5 sm:h-5" />
                  {isPaused ? 'Resume' : 'Pause'}
                </button>
              )}
              <button
                onClick={handleStop}
                disabled={!isRunning && elapsedTime === 0}
                className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-red-500 to-rose-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 disabled:opacity-50 text-xs sm:text-sm"
              >
                <Square size={16} className="sm:w-5 sm:h-5" />
                Stop
              </button>
              {!isRunning && (
                <button
                  onClick={handleReset}
                  disabled={currentCount === 0}
                  className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-slate-500 to-slate-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 disabled:opacity-50 text-xs sm:text-sm col-span-2 sm:col-span-1"
                >
                  <RotateCcw size={16} className="sm:w-5 sm:h-5" />
                  Reset
                </button>
              )}
            </div>
          </div>

          <div className="mt-4 sm:mt-6">
            <div className="h-2 sm:h-3 bg-slate-200 rounded-full overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-blue-600 via-cyan-600 to-blue-600 transition-all duration-300"
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>
        </div>

        {/* Main Content Grid */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 sm:gap-6">
          {/* Graph Area */}
          <div className="lg:col-span-2 bg-white rounded-2xl sm:rounded-3xl p-4 sm:p-6 shadow-xl border border-slate-200">
            <div className="flex items-center justify-between mb-4 sm:mb-6">
              <h3 className="text-xl sm:text-2xl font-bold text-slate-800">Count Over Time</h3>
            </div>

            <div className="h-64 sm:h-96 bg-gradient-to-br from-slate-50 to-blue-50 rounded-xl sm:rounded-2xl p-2 sm:p-4 border border-slate-200">
              {allDataPoints.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={allDataPoints} margin={{ top: 5, right: 10, left: 0, bottom: 5 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
                    <XAxis
                      dataKey="time"
                      label={{ value: 'Time (s)', position: 'insideBottom', offset: -5, style: { fontSize: '12px' } }}
                      stroke="#64748b"
                      tick={{ fontSize: 12 }}
                    />
                    <YAxis
                      label={{
                        value: 'Count',
                        angle: -90,
                        position: 'insideLeft',
                        style: { fontSize: '12px' }
                      }}
                      stroke="#64748b"
                      tick={{ fontSize: 12 }}
                    />
                    <Tooltip
                      contentStyle={{
                        backgroundColor: '#fff',
                        border: '2px solid #e2e8f0',
                        borderRadius: '12px',
                        boxShadow: '0 4px 6px rgba(0,0,0,0.1)',
                        fontSize: '12px'
                      }}
                    />
                    <Legend wrapperStyle={{ fontSize: '12px' }} />
                    <Line
                      type="stepAfter"
                      dataKey="count"
                      stroke="#3b82f6"
                      strokeWidth={3}
                      dot={{ r: 3, fill: '#3b82f6' }}
                      name="Oscillation Count"
                      isAnimationActive={false}
                    />
                  </LineChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex items-center justify-center">
                  <div className="text-center">
                    <Activity size={48} className="sm:w-16 sm:h-16 mx-auto mb-4 text-blue-300" />
                    <p className="text-base sm:text-xl font-semibold text-slate-600">
                      {!esp32Connected
                        ? 'Waiting for OSI sensor...'
                        : 'Start counting to see data'}
                    </p>
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* Data Table */}
          <div className="bg-white rounded-2xl sm:rounded-3xl p-4 sm:p-6 shadow-xl border border-slate-200">
            <div className="mb-4">
              <h3 className="text-xl sm:text-2xl font-bold text-slate-800 mb-2">Recent Counts</h3>
              <p className="text-xs sm:text-sm text-slate-500">Last 20 readings</p>
            </div>
            <div className="overflow-y-auto rounded-xl border-2 border-slate-200 max-h-64 sm:max-h-96">
              {recentTableData.length > 0 ? (
                <table className="w-full text-xs sm:text-sm">
                  <thead className="sticky top-0 bg-gradient-to-r from-blue-100 to-cyan-100 z-10">
                    <tr>
                      <th className="text-left p-2 sm:p-3 font-bold text-slate-700">Time (s)</th>
                      <th className="text-right p-2 sm:p-3 font-bold text-slate-700">Count</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-100">
                    {recentTableData.map((reading, idx) => (
                      <tr key={idx} className="hover:bg-blue-50 transition-colors">
                        <td className="p-2 sm:p-3 text-slate-700 font-semibold">
                          {reading.time.toFixed(2)}
                        </td>
                        <td className="text-right p-2 sm:p-3 font-mono text-sm sm:text-lg font-bold text-blue-700">
                          {reading.count}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              ) : (
                <div className="text-center py-8 sm:py-12">
                  <div className="text-slate-400 mb-2">
                    <Activity size={36} className="sm:w-12 sm:h-12 mx-auto opacity-30" />
                  </div>
                  <p className="text-slate-500 font-semibold text-sm sm:text-base">No data yet</p>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Export & Save Section */}
        <div className="mt-4 sm:mt-6 bg-gradient-to-br from-white to-blue-50 rounded-2xl sm:rounded-3xl p-4 sm:p-6 shadow-xl border border-slate-200">
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 sm:gap-4 mb-4">
            <div>
              <h3 className="text-xl sm:text-2xl font-bold text-slate-800">Export & Save</h3>
              <p className="text-xs sm:text-sm text-slate-500 mt-1">
                {allDataPoints.length > 0
                  ? `${allDataPoints.length} data points collected`
                  : 'No data to export'}
              </p>
            </div>
          </div>
          <div className="flex flex-col sm:flex-row gap-3 sm:gap-4">
            <button
              onClick={handleExportCSV}
              disabled={allDataPoints.length === 0}
              className="flex-1 flex items-center justify-center gap-2 sm:gap-3 px-4 sm:px-8 py-3 sm:py-4 bg-gradient-to-r from-blue-600 to-cyan-600 text-white rounded-xl font-bold hover:shadow-xl transition-all hover:-translate-y-1 disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:translate-y-0 text-sm sm:text-base"
            >
              <Download size={20} className="sm:w-6 sm:h-6" />
              Export CSV
            </button>
            <button
              onClick={handleSaveToProfile}
              disabled={allDataPoints.length === 0}
              className="flex-1 flex items-center justify-center gap-2 sm:gap-3 px-4 sm:px-8 py-3 sm:py-4 bg-gradient-to-r from-indigo-600 to-blue-600 text-white rounded-xl font-bold hover:shadow-xl transition-all hover:-translate-y-1 disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:translate-y-0 text-sm sm:text-base"
            >
              <Save size={20} className="sm:w-6 sm:h-6" />
              Save to Profile
            </button>
          </div>
        </div>
      </div>

      <style jsx>{`
        @keyframes slide-in {
          from {
            transform: translateX(100%);
            opacity: 0;
          }
          to {
            transform: translateX(0);
            opacity: 1;
          }
        }
        .animate-slide-in {
          animation: slide-in 0.3s ease-out;
        }
      `}</style>
    </div>
  );
}

export default OSIInterface;
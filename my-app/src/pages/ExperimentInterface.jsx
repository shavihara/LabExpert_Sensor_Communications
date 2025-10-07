import React, { useState, useEffect, useMemo, useRef } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, Brush } from 'recharts';
import { Play, Pause, Square, Download, Save, Settings, AlertCircle, RefreshCcw, Wifi, WifiOff, Zap, Loader, CheckCircle, X, Maximize2, Minimize2, ZoomIn, ZoomOut, Layers } from 'lucide-react';

const BACKEND_URL = `http://${window.location.hostname.replace(':3000', '')}:5000`;
const RECONNECT_DELAY = 3000;
const CONNECTION_CHECK_INTERVAL = 5000;

function ExperimentInterface() {
  const [isRunning, setIsRunning] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [activeGraph, setActiveGraph] = useState('displacement');
  const [elapsedTime, setElapsedTime] = useState(0);
  const [timeLimit, setTimeLimit] = useState(60);
  const [samplingRate, setSamplingRate] = useState(10);
  const [showConfigModal, setShowConfigModal] = useState(false);
  
  const [allDataPoints, setAllDataPoints] = useState([]);
  
  const [esp32Connected, setEsp32Connected] = useState(false);
  const [esp32Info, setEsp32Info] = useState(null);
  const [isCheckingConnection, setIsCheckingConnection] = useState(false);
  const [error, setError] = useState(null);
  const [isConnecting, setIsConnecting] = useState(false);
  
  const [availableExperiments, setAvailableExperiments] = useState([]);
  const [selectedExperiment, setSelectedExperiment] = useState(null);
  const [showExperimentSelector, setShowExperimentSelector] = useState(true);
  const [isUploadingFirmware, setIsUploadingFirmware] = useState(false);
  const [uploadProgress, setUploadProgress] = useState('');
  
  // New states for enhanced features
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [comparisonMode, setComparisonMode] = useState(false);
  const [zoomDomain, setZoomDomain] = useState(null);
  const [showAllData, setShowAllData] = useState(false);
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

  useEffect(() => {
    const fetchExperiments = async () => {
      if (!token) return;
      try {
        const response = await fetch(`${BACKEND_URL}/api/sensor/available_experiments`, {
          headers: { 'Authorization': `Bearer ${token}` }
        });
        if (response.ok) {
          const data = await response.json();
          setAvailableExperiments(data.experiments || []);
        }
      } catch (err) {
        console.error('Failed to fetch experiments:', err);
      }
    };
    fetchExperiments();
  }, [token]);

  useEffect(() => {
    const checkConnection = async () => {
      if (isRunning || !token) return;
      setIsCheckingConnection(true);
      try {
        const response = await fetch(`${BACKEND_URL}/api/sensor/status`, {
          headers: { 'Authorization': `Bearer ${token}` },
        });
        if (response.ok) {
          const data = await response.json();
          const wasConnected = esp32Connected;
          setEsp32Connected(data.status.connected);
          setEsp32Info(data.status.device_info);
          setError(data.status.connected ? null : 'ESP32 not connected');
          
          if (!wasConnected && data.status.connected) {
            addStatusMessage('ESP32 Connected Successfully', 'success');
          }
        } else {
          setEsp32Connected(false);
          setError('Failed to check ESP32 status');
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

  const handleSelectExperiment = async (experimentType) => {
    if (!esp32Connected) {
      addStatusMessage('ESP32 not connected. Please check hardware.', 'error');
      return;
    }

    setIsUploadingFirmware(true);
    setUploadProgress('Uploading firmware to ESP32...');
    addStatusMessage('Starting firmware upload...', 'info');
    
    try {
      const response = await fetch(`${BACKEND_URL}/api/sensor/select_experiment`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${token}`
        },
        body: JSON.stringify({ experiment_type: experimentType })
      });

      if (response.ok) {
        setUploadProgress('Firmware uploaded! ESP32 rebooting...');
        addStatusMessage('Firmware uploaded successfully', 'success');
        
        await new Promise(resolve => setTimeout(resolve, 3000));
        setUploadProgress('Verifying ESP32 connection...');
        addStatusMessage('Verifying connection...', 'info');
        
        let attempts = 0;
        const maxAttempts = 10;
        
        while (attempts < maxAttempts) {
          try {
            const statusRes = await fetch(`${BACKEND_URL}/api/sensor/status`, {
              headers: { 'Authorization': `Bearer ${token}` }
            });
            if (statusRes.ok) {
              const statusData = await statusRes.json();
              if (statusData.status.connected) {
                setUploadProgress('ESP32 ready!');
                addStatusMessage('ESP32 ready for experiments!', 'success');
                await new Promise(resolve => setTimeout(resolve, 1000));
                setSelectedExperiment(experimentType);
                setShowExperimentSelector(false);
                setIsUploadingFirmware(false);
                return;
              }
            }
          } catch (err) {
            // Continue trying
          }
          attempts++;
          await new Promise(resolve => setTimeout(resolve, 1000));
        }
        
        setUploadProgress('Firmware uploaded but ESP32 not responding. Please check connection.');
        addStatusMessage('ESP32 not responding', 'warning');
        setTimeout(() => {
          setIsUploadingFirmware(false);
          setShowExperimentSelector(false);
          setSelectedExperiment(experimentType);
        }, 3000);
        
      } else {
        const error = await response.json();
        setUploadProgress(`Upload failed: ${error.detail}`);
        addStatusMessage(`Upload failed: ${error.detail}`, 'error');
        setTimeout(() => setIsUploadingFirmware(false), 3000);
      }
    } catch (err) {
      setUploadProgress(`Error: ${err.message}`);
      addStatusMessage(`Error: ${err.message}`, 'error');
      setTimeout(() => setIsUploadingFirmware(false), 3000);
    }
  };

  const parseReading = (sseData) => {
    try {
      const data = JSON.parse(sseData);
      return {
        timestamp: data.time,
        displacement: parseFloat(data.displacement.toFixed(4)),
        velocity: parseFloat(data.velocity.toFixed(3)),
        acceleration: parseFloat(data.acceleration.toFixed(3)),
      };
    } catch (err) {
      console.error('Error parsing SSE data:', err);
      return null;
    }
  };

  useEffect(() => {
    const connectSSE = () => {
      if (!token || !esp32Connected || eventSourceRef.current) return;

      setIsConnecting(true);
      setError(null);

      const eventSource = new EventSource(`${BACKEND_URL}/api/sensor/stream?token=${token}`);
      eventSourceRef.current = eventSource;

      eventSource.onmessage = (event) => {
        const reading = parseReading(event.data);
        if (!reading) return;

        setElapsedTime(reading.timestamp * 1000);
        
        setAllDataPoints((prev) => [...prev, {
          time: reading.timestamp,
          velocity: reading.velocity,
          displacement: reading.displacement,
          acceleration: reading.acceleration,
        }]);

        if (reading.timestamp >= timeLimit) {
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
      const response = await fetch(`${BACKEND_URL}/api/sensor/configure`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${token}`,
        },
        body: JSON.stringify({
          frequency: samplingRate,
          duration: timeLimit,
          mode: 'distance'
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
      addStatusMessage('ESP32 not connected. Please check hardware connection.', 'error');
      return;
    }

    const configured = await handleConfigureExperiment();
    if (!configured) return;

    try {
      const response = await fetch(`${BACKEND_URL}/api/sensor/start`, {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` },
      });

      if (!response.ok) {
        throw new Error('Failed to start experiment on ESP32');
      }

      if (elapsedTime === 0) {
        setAllDataPoints([]);
      }

      setIsRunning(true);
      setIsPaused(false);
      setError(null);
      addStatusMessage('Experiment started', 'success');
    } catch (err) {
      setError('Failed to start experiment: ' + err.message);
      addStatusMessage('Failed to start experiment: ' + err.message, 'error');
    }
  };

  const handlePause = () => {
    setIsPaused(!isPaused);
    addStatusMessage(isPaused ? 'Experiment resumed' : 'Experiment paused', 'info');
  };

  const handleStop = async () => {
    try {
      await fetch(`${BACKEND_URL}/api/sensor/stop`, {
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` },
      });
      addStatusMessage('Experiment stopped', 'info');
    } catch (err) {
      console.error('Error stopping experiment:', err);
    }
    setIsRunning(false);
    setIsPaused(false);
    setElapsedTime(0);
    setError(null);
  };

  const handleReset = () => {
    setAllDataPoints([]);
    setElapsedTime(0);
    setError(null);
    setZoomDomain(null);
    addStatusMessage('Data cleared', 'info');
  };

  const handleExportCSV = () => {
    if (allDataPoints.length === 0) return;

    const headers = ['Time (s)', 'Displacement (m)', 'Velocity (m/s)', 'Acceleration (m/s²)'];
    const csvContent = [
      headers.join(','),
      ...allDataPoints.map(row =>
        `${row.time.toFixed(3)},${row.displacement},${row.velocity},${row.acceleration}`
      )
    ].join('\n');

    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `experiment_${new Date().toISOString()}.csv`;
    a.click();
    window.URL.revokeObjectURL(url);
    addStatusMessage('CSV exported successfully', 'success');
  };

  const handleSaveToProfile = async () => {
    if (!token || allDataPoints.length === 0) {
      addStatusMessage(allDataPoints.length === 0 ? 'No data to save.' : 'Please log in to save data.', 'warning');
      return;
    }

    try {
      const response = await fetch(`${BACKEND_URL}/api/sensor/save_data`, {
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
            sensorType: esp32Info?.sensor_type || 'unknown',
            experimentType: selectedExperiment
          }
        }),
      });

      if (response.ok) {
        addStatusMessage('Data saved to your profile!', 'success');
      } else {
        addStatusMessage('Failed to save data.', 'error');
      }
    } catch (err) {
      addStatusMessage('Error saving data: ' + err.message, 'error');
    }
  };

  const getGraphColor = (type) => {
    switch (type) {
      case 'velocity': return '#8b5cf6';
      case 'displacement': return '#10b981';
      case 'acceleration': return '#f59e0b';
      default: return '#8b5cf6';
    }
  };

  const getGraphLabel = (type) => {
    switch (type) {
      case 'velocity': return 'Velocity (m/s)';
      case 'displacement': return 'Displacement (m)';
      case 'acceleration': return 'Acceleration (m/s²)';
      default: return '';
    }
  };

  const recentTableData = useMemo(() => {
    return showAllData ? allDataPoints.slice().reverse() : allDataPoints.slice(-20).reverse();
  }, [allDataPoints, showAllData]);

  const progress = Math.min((elapsedTime / (timeLimit * 1000)) * 100, 100);

  const handleZoomIn = () => {
    if (allDataPoints.length === 0) return;
    const dataLength = allDataPoints.length;
    const newStart = Math.floor(dataLength * 0.25);
    const newEnd = Math.floor(dataLength * 0.75);
    setZoomDomain([allDataPoints[newStart]?.time || 0, allDataPoints[newEnd]?.time || 1]);
  };

  const handleZoomOut = () => {
    setZoomDomain(null);
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 via-purple-50 to-slate-100 p-2 sm:p-4 pt-16 sm:pt-20">
      
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

      {/* Experiment Selector Modal */}
      {showExperimentSelector && (
        <div className="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center z-50 p-4">
          <div className="bg-white rounded-3xl p-4 sm:p-8 max-w-3xl w-full shadow-2xl transform transition-all max-h-[90vh] overflow-y-auto">
            <div className="flex items-start justify-between mb-4 sm:mb-6">
              <div>
                <h2 className="text-2xl sm:text-4xl font-bold bg-gradient-to-r from-purple-600 to-indigo-600 bg-clip-text text-transparent mb-2">
                  Select Experiment
                </h2>
                <p className="text-slate-600 text-sm sm:text-lg">
                  Choose your experiment type. Firmware will be automatically uploaded to your ESP32.
                </p>
              </div>
              <button
                onClick={() => !isUploadingFirmware && setShowExperimentSelector(false)}
                className="p-2 hover:bg-slate-100 rounded-lg transition-colors"
                disabled={isUploadingFirmware}
              >
                <X size={24} className="text-slate-600" />
              </button>
            </div>

            {isUploadingFirmware ? (
              <div className="text-center py-8 sm:py-12 px-4 sm:px-6 bg-gradient-to-br from-purple-50 to-indigo-50 rounded-2xl">
                {uploadProgress.includes('ready') ? (
                  <CheckCircle className="h-16 sm:h-20 w-16 sm:w-20 text-green-500 mx-auto mb-4" />
                ) : (
                  <Loader className="animate-spin h-16 sm:h-20 w-16 sm:w-20 text-purple-600 mx-auto mb-4" />
                )}
                <p className="text-lg sm:text-xl font-semibold text-slate-800 mb-2">{uploadProgress}</p>
                {!uploadProgress.includes('ready') && !uploadProgress.includes('failed') && (
                  <p className="text-xs sm:text-sm text-slate-500">Please wait, do not refresh...</p>
                )}
              </div>
            ) : !esp32Connected ? (
              <div className="text-center py-8 sm:py-12 px-4 sm:px-6 bg-red-50 rounded-2xl">
                <WifiOff size={48} className="sm:w-16 sm:h-16 mx-auto mb-4 text-red-400" />
                <p className="text-lg sm:text-xl font-semibold text-slate-800 mb-2">ESP32 Not Connected</p>
                <p className="text-sm sm:text-base text-slate-600 mb-6">Please check your hardware connection</p>
                <button
                  onClick={() => window.location.reload()}
                  className="px-6 sm:px-8 py-2 sm:py-3 bg-gradient-to-r from-purple-600 to-indigo-600 text-white rounded-xl hover:shadow-lg transition-all font-semibold text-sm sm:text-base"
                >
                  Retry Connection
                </button>
              </div>
            ) : (
              <div className="grid grid-cols-1 md:grid-cols-2 gap-3 sm:gap-5">
                {availableExperiments.map((exp) => (
                  <button
                    key={exp.type}
                    onClick={() => handleSelectExperiment(exp.type)}
                    className="group p-4 sm:p-8 bg-gradient-to-br from-white to-purple-50 rounded-2xl border-2 border-slate-200 hover:border-purple-400 hover:shadow-xl transition-all duration-300 hover:-translate-y-1"
                  >
                    <Zap className="w-8 h-8 sm:w-12 sm:h-12 text-purple-600 mb-2 sm:mb-4 group-hover:scale-110 transition-transform" />
                    <h3 className="text-lg sm:text-2xl font-bold text-slate-800 mb-1 sm:mb-2">{exp.name}</h3>
                    <p className="text-slate-500 text-xs sm:text-sm font-mono">{exp.firmware}</p>
                  </button>
                ))}
              </div>
            )}
          </div>
        </div>
      )}

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
                  className="w-full px-4 sm:px-5 py-3 sm:py-4 border-2 border-slate-200 rounded-xl focus:border-purple-500 focus:ring-2 focus:ring-purple-200 focus:outline-none disabled:bg-slate-100 transition-all text-base sm:text-lg font-semibold"
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
                  className="w-full px-4 sm:px-5 py-3 sm:py-4 border-2 border-slate-200 rounded-xl focus:border-purple-500 focus:ring-2 focus:ring-purple-200 focus:outline-none disabled:bg-slate-100 transition-all text-base sm:text-lg font-semibold"
                  min="1"
                  max="100"
                />
              </div>
            </div>
            <div className="mt-6 sm:mt-8 flex flex-col sm:flex-row gap-3 sm:gap-4">
              <button
                onClick={handleConfigureExperiment}
                className="flex-1 px-4 sm:px-6 py-3 sm:py-4 bg-gradient-to-r from-purple-600 to-indigo-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-0.5 text-sm sm:text-base"
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
        {/* Header - Responsive */}
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 sm:gap-4 mb-4 sm:mb-6">
          <h1 className="text-2xl sm:text-4xl font-bold bg-gradient-to-r from-purple-600 to-indigo-600 bg-clip-text text-transparent">
            Physics Lab Interface
          </h1>
          <div className="flex flex-col sm:flex-row items-stretch sm:items-center gap-2 sm:gap-4 w-full sm:w-auto">
            <div className="flex items-center gap-2 sm:gap-3 bg-white px-3 sm:px-5 py-2 sm:py-3 rounded-xl shadow-md border border-slate-200">
              {esp32Connected ? (
                <Wifi size={20} className="text-green-500 flex-shrink-0" />
              ) : (
                <WifiOff size={20} className="text-red-500 flex-shrink-0" />
              )}
              <div className="min-w-0 flex-1">
                <div className="text-xs sm:text-sm font-bold text-slate-800 truncate">
                  {esp32Connected ? 'ESP32 Connected' : 'Disconnected'}
                </div>
                {esp32Info && (
                  <div className="text-xs text-slate-500 truncate">
                    {esp32Info.sensor_type}
                  </div>
                )}
              </div>
            </div>
            <button
              onClick={() => setShowExperimentSelector(true)}
              className="px-3 sm:px-5 py-2 sm:py-3 bg-gradient-to-r from-indigo-500 to-purple-600 text-white rounded-xl hover:shadow-lg transition-all font-semibold text-xs sm:text-sm whitespace-nowrap"
            >
              Change Experiment
            </button>
          </div>
        </div>

        {/* Control Panel - Responsive */}
        <div className="bg-white rounded-2xl sm:rounded-3xl p-4 sm:p-6 mb-4 sm:mb-6 shadow-xl border border-slate-200">
          <div className="flex flex-col lg:flex-row items-stretch lg:items-center justify-between gap-4 sm:gap-6">
            <div className="flex items-center gap-4 sm:gap-8 justify-center lg:justify-start">
              <div className="text-center">
                <div className="text-3xl sm:text-5xl font-bold text-purple-600">
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
                    className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-indigo-500 to-purple-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 disabled:opacity-50 text-xs sm:text-sm"
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
              {!isRunning && allDataPoints.length > 0 && (
                <button
                  onClick={handleReset}
                  className="flex items-center justify-center gap-2 px-3 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-slate-500 to-slate-600 text-white rounded-xl font-bold hover:shadow-lg transition-all hover:-translate-y-1 text-xs sm:text-sm col-span-2 sm:col-span-1"
                >
                  <RefreshCcw size={16} className="sm:w-5 sm:h-5" />
                  Reset
                </button>
              )}
            </div>
          </div>

          <div className="mt-4 sm:mt-6">
            <div className="h-2 sm:h-3 bg-slate-200 rounded-full overflow-hidden">
              <div
                className="h-full bg-gradient-to-r from-purple-600 via-indigo-600 to-purple-600 transition-all duration-300"
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>
        </div>

        {/* Main Content - Responsive Grid */}
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-4 sm:gap-6">
          {/* Graph Area */}
          <div className={`${isFullscreen ? 'fixed inset-0 z-40 bg-white p-4 sm:p-6' : 'lg:col-span-2'} bg-white rounded-2xl sm:rounded-3xl p-4 sm:p-6 shadow-xl border border-slate-200`}>
            <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 sm:gap-4 mb-4 sm:mb-6">
              <h3 className="text-xl sm:text-2xl font-bold text-slate-800">Live Visualization</h3>
              <div className="flex flex-wrap items-center gap-2 w-full sm:w-auto">
                {/* Graph Type Buttons */}
                <div className="flex gap-2">
                  <button
                    onClick={() => setActiveGraph('velocity')}
                    className={`px-3 sm:px-5 py-1.5 sm:py-2 rounded-lg sm:rounded-xl font-bold transition-all text-xs sm:text-sm ${
                      activeGraph === 'velocity'
                        ? 'bg-purple-600 text-white shadow-lg scale-105'
                        : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                    }`}
                  >
                    v-t
                  </button>
                  <button
                    onClick={() => setActiveGraph('displacement')}
                    className={`px-3 sm:px-5 py-1.5 sm:py-2 rounded-lg sm:rounded-xl font-bold transition-all text-xs sm:text-sm ${
                      activeGraph === 'displacement'
                        ? 'bg-green-600 text-white shadow-lg scale-105'
                        : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                    }`}
                  >
                    s-t
                  </button>
                  <button
                    onClick={() => setActiveGraph('acceleration')}
                    className={`px-3 sm:px-5 py-1.5 sm:py-2 rounded-lg sm:rounded-xl font-bold transition-all text-xs sm:text-sm ${
                      activeGraph === 'acceleration'
                        ? 'bg-orange-600 text-white shadow-lg scale-105'
                        : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                    }`}
                  >
                    a-t
                  </button>
                </div>

                {/* Graph Controls */}
                <div className="flex gap-2 ml-auto">
                  <button
                    onClick={() => setComparisonMode(!comparisonMode)}
                    className={`p-1.5 sm:p-2 rounded-lg transition-all ${
                      comparisonMode
                        ? 'bg-indigo-600 text-white'
                        : 'bg-slate-100 text-slate-600 hover:bg-slate-200'
                    }`}
                    title="Comparison Mode"
                  >
                    <Layers size={16} className="sm:w-5 sm:h-5" />
                  </button>
                  <button
                    onClick={handleZoomIn}
                    disabled={allDataPoints.length === 0}
                    className="p-1.5 sm:p-2 bg-slate-100 text-slate-600 rounded-lg hover:bg-slate-200 transition-all disabled:opacity-50"
                    title="Zoom In"
                  >
                    <ZoomIn size={16} className="sm:w-5 sm:h-5" />
                  </button>
                  <button
                    onClick={handleZoomOut}
                    disabled={!zoomDomain}
                    className="p-1.5 sm:p-2 bg-slate-100 text-slate-600 rounded-lg hover:bg-slate-200 transition-all disabled:opacity-50"
                    title="Zoom Out"
                  >
                    <ZoomOut size={16} className="sm:w-5 sm:h-5" />
                  </button>
                  <button
                    onClick={() => setIsFullscreen(!isFullscreen)}
                    className="p-1.5 sm:p-2 bg-slate-100 text-slate-600 rounded-lg hover:bg-slate-200 transition-all"
                    title={isFullscreen ? "Exit Fullscreen" : "Fullscreen"}
                  >
                    {isFullscreen ? (
                      <Minimize2 size={16} className="sm:w-5 sm:h-5" />
                    ) : (
                      <Maximize2 size={16} className="sm:w-5 sm:h-5" />
                    )}
                  </button>
                </div>
              </div>
            </div>

            <div className={`${isFullscreen ? 'h-[calc(100vh-150px)]' : 'h-64 sm:h-96'} bg-gradient-to-br from-slate-50 to-purple-50 rounded-xl sm:rounded-2xl p-2 sm:p-4 border border-slate-200`}>
              {allDataPoints.length > 0 ? (
                <ResponsiveContainer width="100%" height="100%">
                  <LineChart data={allDataPoints} margin={{ top: 5, right: 10, left: 0, bottom: 5 }}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
                    <XAxis
                      dataKey="time"
                      label={{ value: 'Time (s)', position: 'insideBottom', offset: -5, style: { fontSize: '12px' } }}
                      stroke="#64748b"
                      domain={zoomDomain || [0, 'dataMax']}
                      tick={{ fontSize: 12 }}
                    />
                    <YAxis
                      label={{
                        value: comparisonMode ? 'Value' : getGraphLabel(activeGraph),
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
                      formatter={(value) => value.toFixed(4)}
                    />
                    <Legend wrapperStyle={{ fontSize: '12px' }} />

                    {comparisonMode ? (
                      <>
                        <Line
                          type="monotone"
                          dataKey="velocity"
                          stroke={getGraphColor('velocity')}
                          strokeWidth={2}
                          dot={false}
                          name="Velocity (m/s)"
                          isAnimationActive={false}
                        />
                        <Line
                          type="monotone"
                          dataKey="displacement"
                          stroke={getGraphColor('displacement')}
                          strokeWidth={2}
                          dot={false}
                          name="Displacement (m)"
                          isAnimationActive={false}
                        />
                        <Line
                          type="monotone"
                          dataKey="acceleration"
                          stroke={getGraphColor('acceleration')}
                          strokeWidth={2}
                          dot={false}
                          name="Acceleration (m/s²)"
                          isAnimationActive={false}
                        />
                      </>
                    ) : (
                      <Line
                        type="monotone"
                        dataKey={activeGraph}
                        stroke={getGraphColor(activeGraph)}
                        strokeWidth={3}
                        dot={false}
                        name={activeGraph.charAt(0).toUpperCase() + activeGraph.slice(1)}
                        isAnimationActive={false}
                      />
                    )}

                    {!isFullscreen && allDataPoints.length > 50 && (
                      <Brush
                        dataKey="time"
                        height={30}
                        stroke="#8b5cf6"
                        fill="#f3f4f6"
                      />
                    )}
                  </LineChart>
                </ResponsiveContainer>
              ) : (
                <div className="h-full flex items-center justify-center">
                  <div className="text-center">
                    <Zap size={48} className="sm:w-16 sm:h-16 mx-auto mb-4 text-purple-300" />
                    <p className="text-base sm:text-xl font-semibold text-slate-600">
                      {!esp32Connected
                        ? 'Waiting for ESP32...'
                        : !selectedExperiment
                        ? 'Select experiment type'
                        : 'Start experiment to see data'}
                    </p>
                  </div>
                </div>
              )}
            </div>
          </div>

          {/* Data Table */}
          <div className="bg-white rounded-2xl sm:rounded-3xl p-4 sm:p-6 shadow-xl border border-slate-200">
            <div className="mb-4">
              <div className="flex items-center justify-between mb-2">
                <h3 className="text-xl sm:text-2xl font-bold text-slate-800">
                  {activeGraph.charAt(0).toUpperCase() + activeGraph.slice(1)} Data
                </h3>
                {allDataPoints.length > 20 && (
                  <button
                    onClick={() => setShowAllData(!showAllData)}
                    className="text-xs sm:text-sm px-2 sm:px-3 py-1 bg-purple-100 text-purple-700 rounded-lg font-semibold hover:bg-purple-200 transition-all"
                  >
                    {showAllData ? `Show Last 20` : `Show All (${allDataPoints.length})`}
                  </button>
                )}
              </div>
              <p className="text-xs sm:text-sm text-slate-500">
                {showAllData ? `All ${allDataPoints.length} readings` : 'Last 20 readings'}
              </p>
            </div>
            <div className={`overflow-y-auto rounded-xl border-2 border-slate-200 ${showAllData ? 'max-h-[500px]' : 'max-h-64 sm:max-h-96'}`}>
              {recentTableData.length > 0 ? (
                <table className="w-full text-xs sm:text-sm">
                  <thead className="sticky top-0 bg-gradient-to-r from-purple-100 to-indigo-100 z-10">
                    <tr>
                      <th className="text-left p-2 sm:p-3 font-bold text-slate-700">Time (s)</th>
                      <th className="text-right p-2 sm:p-3 font-bold text-slate-700">
                        {activeGraph === 'velocity' && 'v (m/s)'}
                        {activeGraph === 'displacement' && 's (m)'}
                        {activeGraph === 'acceleration' && 'a (m/s²)'}
                      </th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-100">
                    {recentTableData.map((reading, idx) => (
                      <tr
                        key={idx}
                        className={`transition-colors ${
                          activeGraph === 'velocity' ? 'hover:bg-purple-50' :
                          activeGraph === 'displacement' ? 'hover:bg-green-50' :
                          'hover:bg-orange-50'
                        }`}
                      >
                        <td className="p-2 sm:p-3 text-slate-700 font-semibold">
                          {reading.time.toFixed(2)}
                        </td>
                        <td className={`text-right p-2 sm:p-3 font-mono text-sm sm:text-lg font-bold ${
                          activeGraph === 'velocity' ? 'text-purple-700' :
                          activeGraph === 'displacement' ? 'text-green-700' :
                          'text-orange-700'
                        }`}>
                          {activeGraph === 'velocity' && reading.velocity.toFixed(3)}
                          {activeGraph === 'displacement' && reading.displacement.toFixed(4)}
                          {activeGraph === 'acceleration' && reading.acceleration.toFixed(3)}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              ) : (
                <div className="text-center py-8 sm:py-12">
                  <div className="text-slate-400 mb-2">
                    <Zap size={36} className="sm:w-12 sm:h-12 mx-auto opacity-30" />
                  </div>
                  <p className="text-slate-500 font-semibold text-sm sm:text-base">No data yet</p>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* Export & Save Section - Responsive */}
        <div className="mt-4 sm:mt-6 bg-gradient-to-br from-white to-purple-50 rounded-2xl sm:rounded-3xl p-4 sm:p-6 shadow-xl border border-slate-200">
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
              className="flex-1 flex items-center justify-center gap-2 sm:gap-3 px-4 sm:px-8 py-3 sm:py-4 bg-gradient-to-r from-purple-600 to-indigo-600 text-white rounded-xl font-bold hover:shadow-xl transition-all hover:-translate-y-1 disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:translate-y-0 text-sm sm:text-base"
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

export default ExperimentInterface;
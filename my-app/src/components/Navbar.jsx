import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';

function UserDashboard() {
  const [activeSection, setActiveSection] = useState('experiments');
  const [experiments, setExperiments] = useState([]);
  const [recentExperiments, setRecentExperiments] = useState([]);
  const [userProfile, setUserProfile] = useState({});
  const [loading, setLoading] = useState(true);
  const navigate = useNavigate();

  const currentUser = JSON.parse(localStorage.getItem('user') || '{}');

  const availableExperiments = [
    {
      id: 1,
      name: 'Distance Measure',
      description: 'Measure distance using ultrasonic sensors',
      icon: '📏',
      difficulty: 'Beginner',
      duration: '15 min',
      category: 'Sensors',
      color: '#4f46e5'
    },
    {
      id: 2,
      name: 'Oscillation Counter',
      description: 'Count oscillations and measure frequency',
      icon: '🌊',
      difficulty: 'Intermediate',
      duration: '25 min',
      category: 'Physics',
      color: '#059669'
    },
    {
      id: 3,
      name: 'Temperature Monitoring',
      description: 'Monitor temperature changes over time',
      icon: '🌡️',
      difficulty: 'Beginner',
      duration: '20 min',
      category: 'Environmental',
      color: '#dc2626'
    },
    {
      id: 4,
      name: 'Light Intensity Analysis',
      description: 'Analyze light intensity variations',
      icon: '💡',
      difficulty: 'Beginner',
      duration: '18 min',
      category: 'Optics',
      color: '#d97706'
    },
    {
      id: 5,
      name: 'Motion Detection',
      description: 'Detect and track motion patterns',
      icon: '🎯',
      difficulty: 'Advanced',
      duration: '35 min',
      category: 'Sensors',
      color: '#7c3aed'
    },
    {
      id: 6,
      name: 'Sound Wave Analysis',
      description: 'Analyze sound frequencies and amplitudes',
      icon: '📊',
      difficulty: 'Intermediate',
      duration: '30 min',
      category: 'Acoustics',
      color: '#0891b2'
    }
  ];

  const recentExperimentsData = [
    {
      id: 1,
      name: 'Distance Measure',
      date: '2024-06-18',
      status: 'Completed',
      result: 'Success',
      duration: '14 min'
    },
    {
      id: 2,
      name: 'Temperature Monitoring',
      date: '2024-06-17',
      status: 'Completed',
      result: 'Success',
      duration: '19 min'
    },
    {
      id: 3,
      name: 'Oscillation Counter',
      date: '2024-06-16',
      status: 'In Progress',
      result: 'Pending',
      duration: '12 min'
    }
  ];

  const usageStats = {
    totalExperiments: 24,
    completedExperiments: 21,
    totalTime: '8h 45m',
    averageScore: 87,
    weeklyProgress: [65, 78, 82, 75, 89, 91, 87]
  };

  useEffect(() => {
    setTimeout(() => {
      setExperiments(availableExperiments);
      setRecentExperiments(recentExperimentsData);
      setUserProfile(currentUser);
      setLoading(false);
    }, 1000);
  }, []);

  const startExperiment = (experiment) => {
    console.log('Starting experiment:', experiment.name);
    navigate('/experiment', { state: { experiment } });
  };

  const downloadReport = (type) => {
    console.log('Downloading', type, 'report');
    alert(`Downloading ${type} report...`);
  };

  if (loading) {
    return (
      <div className="flex justify-center items-center min-h-screen bg-gradient-to-br from-purple-600 to-purple-800 text-white mt-[70px]">
        <div className="text-center">
          <div className="w-[60px] h-[60px] border-4 border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4"></div>
          <p className="text-lg">Loading your lab dashboard...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-200 pt-[70px]">
      {/* Welcome Section */}
      <div className="relative bg-gradient-to-br from-purple-600 to-purple-800 text-white py-12 px-8 text-center overflow-hidden">
        <div className="absolute inset-0 opacity-30" style={{
          backgroundImage: `url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><defs><pattern id="grid" width="10" height="10" patternUnits="userSpaceOnUse"><path d="M 10 0 L 0 0 0 10" fill="none" stroke="rgba(255,255,255,0.1)" stroke-width="1"/></pattern></defs><rect width="100" height="100" fill="url(%23grid)"/></svg>')`
        }}></div>
        <div className="relative z-10 max-w-4xl mx-auto">
          <h1 className="text-3xl md:text-4xl font-extrabold mb-4 drop-shadow-lg">
            Welcome back, {currentUser.name}! 🧪
          </h1>
          <p className="text-lg md:text-xl opacity-90">Ready to explore some amazing experiments?</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="sticky top-[70px] z-50 bg-white border-b border-slate-200 shadow-sm">
        <div className="max-w-7xl mx-auto flex overflow-x-auto px-4 md:px-8">
          {[
            { id: 'experiments', label: '🧪 Experiments' },
            { id: 'recent', label: '📊 Recent' },
            { id: 'profile', label: '👤 Profile' },
            { id: 'sensors', label: '📡 Sensors' },
            { id: 'reports', label: '📈 Reports' }
          ].map((item) => (
            <button
              key={item.id}
              className={`px-6 py-4 font-semibold whitespace-nowrap border-b-3 transition-all duration-300 ${
                activeSection === item.id
                  ? 'text-purple-600 border-purple-600 bg-purple-50'
                  : 'text-slate-500 border-transparent hover:text-purple-600 hover:bg-purple-50'
              }`}
              onClick={() => setActiveSection(item.id)}
            >
              {item.label}
            </button>
          ))}
        </div>
      </nav>

      {/* Main Content */}
      <main className="max-w-7xl mx-auto px-4 md:px-8 py-8">
        {/* Experiments Section */}
        {activeSection === 'experiments' && (
          <div className="space-y-8">
            <div className="text-center mb-8">
              <h2 className="text-2xl md:text-3xl font-bold text-slate-800 mb-2">🧪 Available Experiments</h2>
              <p className="text-slate-600 text-lg">Choose an experiment to get started</p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {experiments.map((experiment) => (
                <div
                  key={experiment.id}
                  className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200 transition-all duration-300 hover:-translate-y-1 hover:shadow-2xl relative overflow-hidden"
                >
                  <div className="absolute top-0 left-0 right-0 h-1" style={{ background: experiment.color }}></div>

                  <div className="flex justify-between items-start mb-4">
                    <div className="text-4xl">{experiment.icon}</div>
                    <div className="flex flex-col items-end gap-2">
                      <span className={`px-3 py-1 rounded-full text-xs font-bold uppercase tracking-wide ${
                        experiment.difficulty === 'Beginner' ? 'bg-green-100 text-green-700' :
                        experiment.difficulty === 'Intermediate' ? 'bg-yellow-100 text-yellow-700' :
                        'bg-red-100 text-red-700'
                      }`}>
                        {experiment.difficulty}
                      </span>
                      <span className="text-sm text-slate-600 font-medium">⏱️ {experiment.duration}</span>
                    </div>
                  </div>

                  <div className="mb-4">
                    <h3 className="text-xl font-bold text-slate-800 mb-2">{experiment.name}</h3>
                    <p className="text-slate-600 leading-relaxed mb-3">{experiment.description}</p>
                    <span className="inline-block px-3 py-1 bg-purple-100 text-purple-700 rounded-lg text-sm font-semibold">
                      {experiment.category}
                    </span>
                  </div>

                  <button
                    className="w-full py-3 rounded-xl font-semibold text-white transition-all duration-300 hover:-translate-y-0.5 hover:shadow-xl mt-4"
                    style={{ background: experiment.color }}
                    onClick={() => startExperiment(experiment)}
                  >
                    Start Experiment
                  </button>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Recent Experiments Section */}
        {activeSection === 'recent' && (
          <div className="space-y-8">
            <div className="text-center mb-8">
              <h2 className="text-2xl md:text-3xl font-bold text-slate-800 mb-2">📊 Recent Experiments</h2>
              <p className="text-slate-600 text-lg">Your recent experimental activities</p>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
              {[
                { icon: '🎯', value: usageStats.totalExperiments, label: 'Total Experiments' },
                { icon: '✅', value: usageStats.completedExperiments, label: 'Completed' },
                { icon: '⏱️', value: usageStats.totalTime, label: 'Total Time' },
                { icon: '⭐', value: `${usageStats.averageScore}%`, label: 'Average Score' }
              ].map((stat, index) => (
                <div
                  key={index}
                  className="bg-white p-6 rounded-2xl shadow-lg border border-slate-200 flex items-center gap-4 transition-all duration-300 hover:-translate-y-1 hover:shadow-xl"
                >
                  <div className="text-4xl p-3 bg-purple-100 rounded-xl">{stat.icon}</div>
                  <div>
                    <h3 className="text-3xl font-bold text-slate-800">{stat.value}</h3>
                    <p className="text-slate-600 font-medium">{stat.label}</p>
                  </div>
                </div>
              ))}
            </div>

            <div className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
              <h3 className="text-xl font-bold text-slate-800 mb-6">Recent Activities</h3>
              {recentExperiments.map((experiment) => (
                <div
                  key={experiment.id}
                  className="flex flex-col sm:flex-row justify-between items-start sm:items-center py-4 border-b border-slate-200 last:border-0 gap-3"
                >
                  <div>
                    <h4 className="font-semibold text-slate-800 mb-1">{experiment.name}</h4>
                    <p className="text-sm text-slate-600">
                      Performed on {new Date(experiment.date).toLocaleDateString()}
                    </p>
                  </div>
                  <div className="flex gap-3 items-center">
                    <span className={`px-3 py-1 rounded-lg text-sm font-semibold ${
                      experiment.status === 'Completed' ? 'bg-green-100 text-green-700' : 'bg-yellow-100 text-yellow-700'
                    }`}>
                      {experiment.status}
                    </span>
                    <span className="text-sm text-slate-600">{experiment.duration}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Profile Section */}
        {activeSection === 'profile' && (
          <div className="space-y-8">
            <div className="text-center mb-8">
              <h2 className="text-2xl md:text-3xl font-bold text-slate-800 mb-2">👤 Profile Settings</h2>
              <p className="text-slate-600 text-lg">Manage your account and preferences</p>
            </div>

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              <div className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
                <h3 className="text-xl font-bold text-slate-800 mb-6">Personal Information</h3>
                <div className="space-y-4">
                  <div>
                    <label className="block text-sm font-semibold text-gray-700 mb-2">Name</label>
                    <input
                      type="text"
                      value={currentUser.name || ''}
                      readOnly
                      className="w-full px-4 py-3 border-2 border-slate-200 rounded-lg bg-slate-50 text-slate-600"
                    />
                  </div>
                  <div>
                    <label className="block text-sm font-semibold text-gray-700 mb-2">Email</label>
                    <input
                      type="email"
                      value={currentUser.email || ''}
                      readOnly
                      className="w-full px-4 py-3 border-2 border-slate-200 rounded-lg bg-slate-50 text-slate-600"
                    />
                  </div>
                  <div>
                    <label className="block text-sm font-semibold text-gray-700 mb-2">Role</label>
                    <input
                      type="text"
                      value={currentUser.role || 'User'}
                      readOnly
                      className="w-full px-4 py-3 border-2 border-slate-200 rounded-lg bg-slate-50 text-slate-600"
                    />
                  </div>
                  <button className="bg-gradient-to-br from-purple-600 to-purple-800 text-white px-6 py-3 rounded-lg font-semibold transition-all duration-300 hover:-translate-y-0.5 hover:shadow-xl">
                    Edit Profile
                  </button>
                </div>
              </div>

              <div className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
                <h3 className="text-xl font-bold text-slate-800 mb-6">Preferences</h3>
                <div className="space-y-4">
                  {['Email Notifications', 'Auto-save Results', 'Dark Mode'].map((pref, index) => (
                    <div key={index} className="flex justify-between items-center p-4 bg-slate-50 rounded-lg border border-slate-200">
                      <span className="font-medium text-slate-700">{pref}</span>
                      <label className="relative inline-block w-12 h-6">
                        <input type="checkbox" defaultChecked={index < 2} className="opacity-0 w-0 h-0 peer" />
                        <span className="absolute cursor-pointer inset-0 bg-slate-300 rounded-full transition-all duration-300 peer-checked:bg-purple-600 before:absolute before:content-[''] before:h-5 before:w-5 before:left-0.5 before:bottom-0.5 before:bg-white before:rounded-full before:transition-all before:duration-300 peer-checked:before:translate-x-6"></span>
                      </label>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Sensors Section */}
        {activeSection === 'sensors' && (
          <div className="space-y-8">
            <div className="text-center mb-8">
              <h2 className="text-2xl md:text-3xl font-bold text-slate-800 mb-2">📡 Sensor Settings</h2>
              <p className="text-slate-600 text-lg">Configure and calibrate your sensors</p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
              {[
                { icon: '📏', name: 'Ultrasonic Sensor', range: '2cm - 400cm', accuracy: '±3mm', status: 'online' },
                { icon: '🌡️', name: 'Temperature Sensor', range: '-40°C to 85°C', accuracy: '±0.5°C', status: 'online' },
                { icon: '💡', name: 'Light Sensor', range: '0-65535 lux', accuracy: '±10%', status: 'offline' },
                { icon: '📊', name: 'Sound Sensor', range: '30dB - 130dB', accuracy: '20Hz - 20kHz', status: 'online' }
              ].map((sensor, index) => (
                <div key={index} className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
                  <div className="flex items-center justify-between mb-4">
                    <div className="flex items-center gap-3">
                      <span className="text-2xl">{sensor.icon}</span>
                      <h3 className="font-semibold text-slate-800">{sensor.name}</h3>
                    </div>
                    <span className={`px-3 py-1 rounded-lg text-xs font-semibold ${
                      sensor.status === 'online' ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'
                    }`}>
                      {sensor.status === 'online' ? 'Online' : 'Offline'}
                    </span>
                  </div>
                  <div className="space-y-2 mb-4">
                    <p className="text-sm text-slate-600">Range: {sensor.range}</p>
                    <p className="text-sm text-slate-600">Accuracy: {sensor.accuracy}</p>
                  </div>
                  <button
                    className="w-full bg-gradient-to-br from-purple-600 to-purple-800 text-white py-2 rounded-lg font-semibold transition-all duration-300 hover:-translate-y-0.5 hover:shadow-xl disabled:bg-gray-400 disabled:cursor-not-allowed disabled:transform-none"
                    disabled={sensor.status === 'offline'}
                  >
                    Calibrate
                  </button>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Reports Section */}
        {activeSection === 'reports' && (
          <div className="space-y-8">
            <div className="text-center mb-8">
              <h2 className="text-2xl md:text-3xl font-bold text-slate-800 mb-2">📈 Usage & Reports</h2>
              <p className="text-slate-600 text-lg">Download and analyze your experimental data</p>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
              {[
                { icon: '📊', title: 'Experiment Summary', desc: 'Comprehensive report of all your experiments', type: 'summary' },
                { icon: '📈', title: 'Performance Analytics', desc: 'Detailed performance metrics and trends', type: 'analytics' },
                { icon: '📋', title: 'Raw Data Export', desc: 'Export raw sensor data in CSV format', type: 'raw-data' },
                { icon: '🎯', title: 'Custom Report', desc: 'Generate custom reports with specific parameters', type: 'custom' }
              ].map((report, index) => (
                <div
                  key={index}
                  className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200 text-center transition-all duration-300 hover:-translate-y-1 hover:shadow-xl"
                >
                  <div className="text-5xl mb-4">{report.icon}</div>
                  <h3 className="text-lg font-bold text-slate-800 mb-2">{report.title}</h3>
                  <p className="text-slate-600 leading-relaxed mb-6">{report.desc}</p>
                  <button
                    className="bg-gradient-to-br from-purple-600 to-purple-800 text-white px-6 py-3 rounded-lg font-semibold transition-all duration-300 hover:-translate-y-0.5 hover:shadow-xl"
                    onClick={() => downloadReport(report.type)}
                  >
                    {report.type === 'raw-data' ? 'Download CSV' : report.type === 'custom' ? 'Create Report' : 'Download PDF'}
                  </button>
                </div>
              ))}
            </div>

            <div className="bg-white rounded-2xl p-6 shadow-lg border border-slate-200">
              <h3 className="text-xl font-bold text-slate-800 mb-6 text-center">Weekly Progress</h3>
              <div className="h-64 p-4">
                <div className="flex items-end justify-between h-full gap-2">
                  {usageStats.weeklyProgress.map((value, index) => (
                    <div key={index} className="flex-1 flex flex-col items-center h-full">
                      <div
                        className="w-full bg-gradient-to-t from-purple-600 to-purple-400 rounded-t transition-all duration-300 hover:opacity-80 relative group"
                        style={{ height: `${value}%`, minHeight: '10px' }}
                      >
                        <span className="absolute -top-8 left-1/2 -translate-x-1/2 font-semibold text-slate-800 opacity-0 group-hover:opacity-100 transition-opacity duration-300">
                          {value}%
                        </span>
                      </div>
                      <span className="mt-2 text-sm text-slate-600 font-medium">
                        {['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'][index]}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}
      </main>

      <style jsx>{`
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
}

export default UserDashboard;
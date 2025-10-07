import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import '../styles/UserDashboard.css';

function UserDashboard() {
  const [activeSection, setActiveSection] = useState('experiments');
  const [experiments, setExperiments] = useState([]);
  const [recentExperiments, setRecentExperiments] = useState([]);
  const [userProfile, setUserProfile] = useState({});
  const [loading, setLoading] = useState(true);
  const [deferredPrompt, setDeferredPrompt] = useState(null);
  const [showInstallButton, setShowInstallButton] = useState(false);
  const navigate = useNavigate();

  // Get current user
  const currentUser = JSON.parse(localStorage.getItem('user') || '{}');

  // Available experiments
  const availableExperiments = [
    {
      id: 1,
      name: 'Distance Measure',
      description: 'Measure distance using TOF sensor',
      icon: '📏',
      difficulty: 'Available',
      duration: '15+ min',
      category: 'Motion',
      color: '#4f46e5'
    },
    {
      id: 2,
      name: 'Oscillation Counter',
      description: 'Count oscillations and measure frequency',
      icon: '🌊',
      difficulty: 'Available',
      duration: '25+ min',
      category: 'Physics',
      color: '#059669'
    },
    {
      id: 3,
      name: 'Temperature Monitoring',
      description: 'Monitor temperature changes over time',
      icon: '🌡️',
      difficulty: 'Unavailable',
      duration: '20 min',
      category: 'Environmental',
      color: '#dc2626'
    },
    {
      id: 4,
      name: 'Light Intensity Analysis',
      description: 'Analyze light intensity variations',
      icon: '💡',
      difficulty: 'Unavailable',
      duration: '18 min',
      category: 'Optics',
      color: '#d97706'
    },
    {
      id: 5,
      name: 'Motion Detection',
      description: 'Detect and track motion patterns',
      icon: '🎯',
      difficulty: 'Available',
      duration: '35 min',
      category: 'Sensors',
      color: '#7c3aed'
    },
    {
      id: 6,
      name: 'Sound Wave Analysis',
      description: 'Analyze sound frequencies and amplitudes',
      icon: '🔊',
      difficulty: 'Unavailable',
      duration: '30 min',
      category: 'Acoustics',
      color: '#0891b2'
    }
  ];

  // Recent experiments (dummy data)
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

  // Usage statistics (dummy data)
  const usageStats = {
    totalExperiments: 24,
    completedExperiments: 21,
    totalTime: '8h 45m',
    averageScore: 87,
    weeklyProgress: [65, 78, 82, 75, 89, 91, 87]
  };

  // Handle PWA install prompt
  useEffect(() => {
    const handleBeforeInstallPrompt = (e) => {
      e.preventDefault();
      setDeferredPrompt(e);
      setShowInstallButton(true);
    };

    window.addEventListener('beforeinstallprompt', handleBeforeInstallPrompt);

    return () => {
      window.removeEventListener('beforeinstallprompt', handleBeforeInstallPrompt);
    };
  }, []);

  const handleInstallClick = async () => {
    if (deferredPrompt) {
      deferredPrompt.prompt();
      const { outcome } = await deferredPrompt.userChoice;
      console.log(`User ${outcome === 'accepted' ? 'accepted' : 'dismissed'} the install prompt`);
      setDeferredPrompt(null);
      setShowInstallButton(false);
    }
  };

  useEffect(() => {
    // Simulate loading user data
    setTimeout(() => {
      setExperiments(availableExperiments);
      setRecentExperiments(recentExperimentsData);
      setUserProfile(currentUser);
      setLoading(false);
    }, 1000);
  }, [currentUser]);

  // Updated startExperiment function with routing
  const startExperiment = (experiment) => {
    // Check if experiment is available
    if (experiment.difficulty !== 'Available') {
      alert('This experiment is currently unavailable');
      return;
    }

    console.log('Starting experiment:', experiment.name);

    // Navigate to the specific experiment page based on ID
    navigate(`/experiment/${experiment.id}`, {
      state: { experiment }
    });
  };

  const downloadReport = (type) => {
    console.log('Downloading', type, 'report');
    alert(`Downloading ${type} report...`);
  };

  if (loading) {
    return (
      <div className="dashboard-loading">
        <div className="loading-animation">
          <div className="loading-spinner"></div>
          <p>Loading your lab dashboard...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="user-dashboard">
      {/* Welcome Section */}
      <div className="dashboard-welcome">
        <div className="welcome-content">
          <h1 className="welcome-title">Welcome back, {currentUser.name}! 🧪</h1>
          <p className="welcome-subtitle">Ready to explore some amazing experiments?</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="dashboard-nav">
        <div className="nav-container">
          <button
            className={`nav-item ${activeSection === 'experiments' ? 'active' : ''}`}
            onClick={() => setActiveSection('experiments')}
          >
            🧪 Experiments
          </button>
          <button
            className={`nav-item ${activeSection === 'recent' ? 'active' : ''}`}
            onClick={() => setActiveSection('recent')}
          >
            📊 Recent
          </button>
          <button
            className={`nav-item ${activeSection === 'profile' ? 'active' : ''}`}
            onClick={() => setActiveSection('profile')}
          >
            👤 Profile
          </button>
          <button
            className={`nav-item ${activeSection === 'sensors' ? 'active' : ''}`}
            onClick={() => setActiveSection('sensors')}
          >
            📡 Sensors
          </button>
          <button
            className={`nav-item ${activeSection === 'reports' ? 'active' : ''}`}
            onClick={() => setActiveSection('reports')}
          >
            📈 Reports
          </button>
        </div>
      </nav>

      {/* Main Content */}
      <main className="dashboard-main">
        {/* Experiments Section */}
        {activeSection === 'experiments' && (
          <div className="section experiments-section">
            <div className="section-header">
              <h2>🧪 Experiments List</h2>
              <p>Choose an experiment to get started</p>
            </div>

            <div className="experiments-grid">
              {experiments.map((experiment) => (
                <div
                  key={experiment.id}
                  className="experiment-card"
                  style={{ '--accent-color': experiment.color }}
                >
                  <div className="experiment-header">
                    <div className="experiment-icon">{experiment.icon}</div>
                    <div className="experiment-meta">
                      <span className={`sensor-status online ${experiment.difficulty.toLowerCase()}`}>
                        {experiment.difficulty}
                      </span>
                      <span className="duration">⏱️ {experiment.duration}</span>
                    </div>
                  </div>

                  <div className="experiment-content">
                    <h3>{experiment.name}</h3>
                    <p>{experiment.description}</p>
                    <span className="category">{experiment.category}</span>
                  </div>

                  <button
                    className="start-btn"
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
          <div className="section recent-section">
            <div className="section-header">
              <h2>📊 Recent Experiments</h2>
              <p>Your recent experimental activities</p>
            </div>

            <div className="stats-overview">
              <div className="stat-card">
                <div className="stat-icon">🎯</div>
                <div className="stat-info">
                  <h3>{usageStats.totalExperiments}</h3>
                  <p>Total Experiments</p>
                </div>
              </div>
              <div className="stat-card">
                <div className="stat-icon">✅</div>
                <div className="stat-info">
                  <h3>{usageStats.completedExperiments}</h3>
                  <p>Completed</p>
                </div>
              </div>
              <div className="stat-card">
                <div className="stat-icon">⏱️</div>
                <div className="stat-info">
                  <h3>{usageStats.totalTime}</h3>
                  <p>Total Time</p>
                </div>
              </div>
              <div className="stat-card">
                <div className="stat-icon">⭐</div>
                <div className="stat-info">
                  <h3>{usageStats.averageScore}%</h3>
                  <p>Average Score</p>
                </div>
              </div>
            </div>

            <div className="recent-experiments-list">
              <h3>Recent Activities</h3>
              {recentExperiments.map((experiment) => (
                <div key={experiment.id} className="recent-item">
                  <div className="recent-info">
                    <h4>{experiment.name}</h4>
                    <p>Performed on {new Date(experiment.date).toLocaleDateString()}</p>
                  </div>
                  <div className="recent-details">
                    <span className={`status ${experiment.status.toLowerCase().replace(' ', '-')}`}>
                      {experiment.status}
                    </span>
                    <span className="duration">{experiment.duration}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Profile Section */}
        {activeSection === 'profile' && (
          <div className="section profile-section">
            <div className="section-header">
              <h2>👤 Profile Settings</h2>
              <p>Manage your account and preferences</p>
            </div>

            <div className="profile-grid">
              <div className="profile-card">
                <h3>Personal Information</h3>
                <div className="profile-form">
                  <div className="form-group">
                    <label>Name</label>
                    <input type="text" value={currentUser.name || ''} readOnly />
                  </div>
                  <div className="form-group">
                    <label>Email</label>
                    <input type="email" value={currentUser.email || ''} readOnly />
                  </div>
                  <div className="form-group">
                    <label>Role</label>
                    <input type="text" value={currentUser.role || 'User'} readOnly />
                  </div>
                  <button className="edit-btn">Edit Profile</button>
                </div>
              </div>

              <div className="profile-card">
                <h3>Preferences</h3>
                <div className="preferences-list">
                  <div className="preference-item">
                    <span>Email Notifications</span>
                    <label className="switch">
                      <input type="checkbox" defaultChecked />
                      <span className="slider"></span>
                    </label>
                  </div>
                  <div className="preference-item">
                    <span>Auto-save Results</span>
                    <label className="switch">
                      <input type="checkbox" defaultChecked />
                      <span className="slider"></span>
                    </label>
                  </div>
                  <div className="preference-item">
                    <span>Dark Mode</span>
                    <label className="switch">
                      <input type="checkbox" />
                      <span className="slider"></span>
                    </label>
                  </div>
                  <div className="preference-item">
                    <span>Install App</span>
                    {showInstallButton ? (
                      <button
                        onClick={handleInstallClick}
                        className="install-btn bg-indigo-600 text-white px-4 py-2 rounded-md hover:bg-indigo-700 transition-colors"
                      >
                        Install App
                      </button>
                    ) : (
                      <span className="text-gray-500">Install option unavailable</span>
                    )}
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Sensor Settings Section */}
        {activeSection === 'sensors' && (
          <div className="section sensors-section">
            <div className="section-header">
              <h2>📡 Sensor Settings</h2>
              <p>Configure and calibrate your sensors</p>
            </div>

            <div className="sensors-grid">
              <div className="sensor-card">
                <div className="sensor-header">
                  <div className="sensor-icon">📏</div>
                  <h3>TOF Distance Measuring Sensor</h3>
                  <span className="sensor-status online">Online</span>
                </div>
                <div className="sensor-details">
                  <p>Range: 5cm - 800cm</p>
                  <p>Accuracy: ±3mm</p>
                  <button className="calibrate-btn">Calibrate</button>
                </div>
              </div>

              <div className="sensor-card">
                <div className="sensor-header">
                  <div className="sensor-icon">📏</div>
                  <h3>Ultrasonic Sensor</h3>
                  <span className="sensor-status online">Online</span>
                </div>
                <div className="sensor-details">
                  <p>Range: 5cm - 100cm</p>
                  <p>Accuracy: ±4mm</p>
                  <button className="calibrate-btn">Calibrate</button>
                </div>
              </div>

              <div className="sensor-card">
                <div className="sensor-header">
                  <div className="sensor-icon">🌡️</div>
                  <h3>Temperature Sensor</h3>
                  <span className="sensor-status online">Online</span>
                </div>
                <div className="sensor-details">
                  <p>Range: -40°C to 85°C</p>
                  <p>Accuracy: ±0.5°C</p>
                  <button className="calibrate-btn">Calibrate</button>
                </div>
              </div>

              <div className="sensor-card">
                <div className="sensor-header">
                  <div className="sensor-icon">💡</div>
                  <h3>Light Sensor</h3>
                  <span className="sensor-status offline">Offline</span>
                </div>
                <div className="sensor-details">
                  <p>Range: 0-65535 lux</p>
                  <p>Accuracy: ±10%</p>
                  <button className="calibrate-btn" disabled>Calibrate</button>
                </div>
              </div>

              <div className="sensor-card">
                <div className="sensor-header">
                  <div className="sensor-icon">🔊</div>
                  <h3>Sound Sensor</h3>
                  <span className="sensor-status online">Online</span>
                </div>
                <div className="sensor-details">
                  <p>Range: 30dB - 130dB</p>
                  <p>Frequency: 20Hz - 20kHz</p>
                  <button className="calibrate-btn">Calibrate</button>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* Reports Section */}
        {activeSection === 'reports' && (
          <div className="section reports-section">
            <div className="section-header">
              <h2>📈 Usage & Reports</h2>
              <p>Download and analyze your experimental data</p>
            </div>

            <div className="reports-grid">
              <div className="report-card">
                <div className="report-icon">📊</div>
                <h3>Experiment Summary</h3>
                <p>Comprehensive report of all your experiments</p>
                <button
                  className="download-btn"
                  onClick={() => downloadReport('summary')}
                >
                  Download PDF
                </button>
              </div>

              <div className="report-card">
                <div className="report-icon">📈</div>
                <h3>Performance Analytics</h3>
                <p>Detailed performance metrics and trends</p>
                <button
                  className="download-btn"
                  onClick={() => downloadReport('analytics')}
                >
                  Download PDF
                </button>
              </div>

              <div className="report-card">
                <div className="report-icon">📋</div>
                <h3>Raw Data Export</h3>
                <p>Export raw sensor data in CSV format</p>
                <button
                  className="download-btn"
                  onClick={() => downloadReport('raw-data')}
                >
                  Download CSV
                </button>
              </div>

              <div className="report-card">
                <div className="report-icon">🎯</div>
                <h3>Custom Report</h3>
                <p>Generate custom reports with specific parameters</p>
                <button
                  className="download-btn"
                  onClick={() => downloadReport('custom')}
                >
                  Create Report
                </button>
              </div>
            </div>

            <div className="usage-chart">
              <h3>Weekly Progress</h3>
              <div className="chart-container">
                <div className="chart-bars">
                  {usageStats.weeklyProgress.map((value, index) => (
                    <div key={index} className="chart-bar">
                      <div
                        className="bar-fill"
                        style={{ height: `${value}%` }}
                        data-value={value}
                      ></div>
                      <span className="bar-label">
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
    </div>
  );
}

export default UserDashboard;
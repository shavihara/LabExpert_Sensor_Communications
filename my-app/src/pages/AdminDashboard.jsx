import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import '../styles/AdminDashboard.css';

function AdminDashboard() {
  const [dashboardData, setDashboardData] = useState(null);
  const [users, setUsers] = useState([]);
  const [systemInfo, setSystemInfo] = useState(null);
  const [loading, setLoading] = useState(true);
  const [activeTab, setActiveTab] = useState('dashboard');
  const [error, setError] = useState(null);
  const navigate = useNavigate();

  // Get current user
  const currentUser = JSON.parse(localStorage.getItem('user') || '{}');

  useEffect(() => {
    // Check if user is admin
    if (currentUser.email !== 'labexpert.us@gmail.com') {
      console.log('❌ Not admin user, redirecting...');
      navigate('/home');
      return;
    }

    fetchData();
  }, [navigate, currentUser.email]);

  const fetchData = async () => {
    try {
      setLoading(true);
      setError(null);

      const token = localStorage.getItem('token');
      if (!token) {
        throw new Error('No authentication token');
      }

      const headers = {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/json'
      };

      console.log('🔄 Fetching admin data...');

      // Fetch dashboard data
      const dashboardResponse = await fetch('http://localhost:5000/api/admin/dashboard', {
        method: 'GET',
        headers,
        credentials: 'include'
      });

      if (!dashboardResponse.ok) {
        throw new Error(`Dashboard fetch failed: ${dashboardResponse.status}`);
      }

      const dashboardResult = await dashboardResponse.json();
      console.log('📊 Dashboard data:', dashboardResult);

      if (dashboardResult.success) {
        setDashboardData(dashboardResult.data);
      }

      // Fetch users data
      const usersResponse = await fetch('http://localhost:5000/api/admin/users', {
        method: 'GET',
        headers,
        credentials: 'include'
      });

      if (usersResponse.ok) {
        const usersResult = await usersResponse.json();
        console.log('👥 Users data:', usersResult);
        if (usersResult.success) {
          setUsers(usersResult.users);
        }
      }

      // Fetch system info
      const systemResponse = await fetch('http://localhost:5000/api/admin/system', {
        method: 'GET',
        headers,
        credentials: 'include'
      });

      if (systemResponse.ok) {
        const systemResult = await systemResponse.json();
        console.log('⚙️ System data:', systemResult);
        if (systemResult.success) {
          setSystemInfo(systemResult.system);
        }
      }

    } catch (error) {
      console.error('❌ Error fetching admin data:', error);
      setError(error.message);

      if (error.message.includes('403') || error.message.includes('Forbidden')) {
        navigate('/home');
      }
    } finally {
      setLoading(false);
    }
  };

  const formatUptime = (seconds) => {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    return `${hours}h ${minutes}m`;
  };

  const formatMemory = (bytes) => {
    return `${Math.round(bytes / 1024 / 1024)}MB`;
  };

  if (loading) {
    return (
      <div className="admin-loading">
        <div className="loading-spinner"></div>
        <p>Loading Admin Dashboard...</p>
      </div>
    );
  }

  if (error) {
    return (
      <div className="admin-error">
        <h2>❌ Error Loading Dashboard</h2>
        <p>{error}</p>
        <button onClick={fetchData} className="retry-btn">🔄 Retry</button>
      </div>
    );
  }

  return (
    <div className="admin-container">
      {/* Welcome Section */}
      <div className="admin-welcome">
        <div className="welcome-content">
          <h1 className="welcome-title">🏥 Lab Expert Admin Dashboard</h1>
          <p className="welcome-subtitle">Monitor system to oversee operations</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="admin-nav">
        <div className="nav-tabs">
          <button
            className={`nav-tab ${activeTab === 'dashboard' ? 'active' : ''}`}
            onClick={() => setActiveTab('dashboard')}
          >
            📊 Dashboard
          </button>
          <button
            className={`nav-tab ${activeTab === 'users' ? 'active' : ''}`}
            onClick={() => setActiveTab('users')}
          >
            👥 Users ({users.length})
          </button>
          <button
            className={`nav-tab ${activeTab === 'system' ? 'active' : ''}`}
            onClick={() => setActiveTab('system')}
          >
            ⚙️ System
          </button>
        </div>
      </nav>

      {/* Content */}
      <main className="admin-content">
        {activeTab === 'dashboard' && (
          <div className="dashboard-tab">
            <h2>📈 Dashboard Overview</h2>

            {/* Stats Cards */}
            {dashboardData && (
              <div className="stats-grid">
                <div className="stat-card blue">
                  <div className="stat-icon">👥</div>
                  <div className="stat-info">
                    <h3>{dashboardData.stats.totalUsers}</h3>
                    <p>Total Users</p>
                  </div>
                </div>

                <div className="stat-card green">
                  <div className="stat-icon">✅</div>
                  <div className="stat-info">
                    <h3>{dashboardData.stats.verifiedUsers}</h3>
                    <p>Verified Users</p>
                  </div>
                </div>

                <div className="stat-card orange">
                  <div className="stat-icon">📈</div>
                  <div className="stat-info">
                    <h3>{dashboardData.stats.recentSignups}</h3>
                    <p>New This Month</p>
                  </div>
                </div>

                <div className="stat-card red">
                  <div className="stat-icon">🔐</div>
                  <div className="stat-info">
                    <h3>{dashboardData.stats.activeSessions}</h3>
                    <p>Active Sessions</p>
                  </div>
                </div>
              </div>
            )}

            {/* Recent Users and Sessions */}
            <div className="dashboard-grid">
              <div className="dashboard-card">
                <h3>🆕 Recent Users</h3>
                <div className="recent-list">
                  {dashboardData?.recentUsers?.map((user, index) => (
                    <div key={index} className="recent-item">
                      <div className="user-info">
                        <span className="user-name">{user.name}</span>
                        <span className="user-email">{user.email}</span>
                      </div>
                      <span className="user-date">
                        {new Date(user.created_at).toLocaleDateString()}
                      </span>
                    </div>
                  ))}
                </div>
              </div>

              <div className="dashboard-card">
                <h3>🔓 Active Sessions</h3>
                <div className="recent-list">
                  {dashboardData?.activeSessions_list?.map((session, index) => (
                    <div key={index} className="recent-item">
                      <div className="user-info">
                        <span className="user-name">{session.name}</span>
                        <span className="user-email">{session.email}</span>
                      </div>
                      <span className="user-date">
                        {new Date(session.last_activity).toLocaleString()}
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === 'users' && (
          <div className="users-tab">
            <h2>👥 All Users</h2>
            <div className="users-table-container">
              <table className="users-table">
                <thead>
                  <tr>
                    <th>Name</th>
                    <th>Email</th>
                    <th>Role</th>
                    <th>Verified</th>
                    <th>Status</th>
                    <th>Last Login</th>
                    <th>Joined</th>
                  </tr>
                </thead>
                <tbody>
                  {users.map((user) => (
                    <tr key={user.id}>
                      <td className="user-name">{user.name}</td>
                      <td className="user-email">{user.email}</td>
                      <td>
                        <span className={`role-badge ${user.role}`}>
                          {user.role}
                        </span>
                      </td>
                      <td>
                        <span className={`status-badge ${user.is_email_verified ? 'verified' : 'unverified'}`}>
                          {user.is_email_verified ? '✅ Yes' : '❌ No'}
                        </span>
                      </td>
                      <td>
                        <span className={`status-badge ${user.is_active ? 'active' : 'inactive'}`}>
                          {user.is_active ? '🟢 Active' : '🔴 Inactive'}
                        </span>
                      </td>
                      <td>
                        {user.last_login
                          ? new Date(user.last_login).toLocaleDateString()
                          : 'Never'
                        }
                      </td>
                      <td>{new Date(user.created_at).toLocaleDateString()}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {activeTab === 'system' && (
          <div className="system-tab">
            <h2>⚙️ System Information</h2>
            {systemInfo && (
              <div className="system-grid">
                <div className="system-card">
                  <h3>💾 Database</h3>
                  <p><strong>Type:</strong> {systemInfo.database}</p>
                  <p><strong>Status:</strong> <span className="status-online">🟢 Connected</span></p>
                </div>

                <div className="system-card">
                  <h3>🖥️ Server</h3>
                  <p><strong>Version:</strong> {systemInfo.version}</p>
                  <p><strong>Node.js:</strong> {systemInfo.nodeVersion}</p>
                  <p><strong>Platform:</strong> {systemInfo.platform}</p>
                  <p><strong>Uptime:</strong> {formatUptime(systemInfo.uptime)}</p>
                </div>

                <div className="system-card">
                  <h3>🧠 Memory Usage</h3>
                  <p><strong>RSS:</strong> {formatMemory(systemInfo.memoryUsage.rss)}</p>
                  <p><strong>Heap Used:</strong> {formatMemory(systemInfo.memoryUsage.heapUsed)}</p>
                  <p><strong>Heap Total:</strong> {formatMemory(systemInfo.memoryUsage.heapTotal)}</p>
                </div>

                <div className="system-card">
                  <h3>📧 Services</h3>
                  <p><strong>Email Service:</strong> <span className="status-online">🟢 Active</span></p>
                  <p><strong>Authentication:</strong> <span className="status-online">🟢 Working</span></p>
                  <p><strong>File Storage:</strong> <span className="status-online">🟢 Available</span></p>
                </div>
              </div>
            )}
          </div>
        )}
      </main>
    </div>
  );
}

export default AdminDashboard;
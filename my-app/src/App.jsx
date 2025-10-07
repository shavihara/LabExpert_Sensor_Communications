import { useState, useEffect } from 'react';
import { Routes, Route, Navigate, useLocation } from 'react-router-dom';
import Login from './components/Login';
import Signup from './components/Signup';
import ForgotPassword from './components/ForgotPassword';
import Home from './components/Home';
import Header from './components/Header';
import AdminDashboard from './pages/AdminDashboard';
import UserDashboard from './pages/UserDashboard';
import ExperimentInterface from './pages/ExperimentInterface';
import OSIInterface from './components/OSIInterface';
import ExperimentRouter from './ExperimentRouter';
import PrivateRoute from './components/PrivateRoute';
import { getCurrentUser } from './utils/api';
import { ThemeProvider } from './context/ThemeContext';
import './App.css';

function AppContent() {
  const [currentUser, setCurrentUser] = useState(null);
  const [loading, setLoading] = useState(true);
  const location = useLocation();

  useEffect(() => {
    const fetchUser = async () => {
      try {
        console.log('🎯 App: Fetching current user...');
        const user = await getCurrentUser();
        console.log('🎯 App: Current user:', user);
        setCurrentUser(user);
      } catch (error) {
        console.error('❌ App: Error fetching current user:', error);
      } finally {
        setLoading(false);
      }
    };
    fetchUser();
  }, []);

  if (loading) {
    return (
      <div style={{
        display: 'flex',
        justifyContent: 'center',
        alignItems: 'center',
        minHeight: '100vh',
        background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
        color: 'white',
        fontSize: '1.5rem'
      }}>
        Loading App...
      </div>
    );
  }

  return (
    <div className="app">
      <Header />
      <Routes>
        <Route path="/" element={<Navigate to="/login" />} />
        <Route path="/login" element={<Login setCurrentUser={setCurrentUser} />} />
        <Route path="/signup" element={<Signup />} />
        <Route path="/forgot-password" element={<ForgotPassword />} />
        <Route
          path="/home"
          element={
            <PrivateRoute>
              <Home />
            </PrivateRoute>
          }
        />
        <Route
          path="/dashboard"
          element={
            <PrivateRoute>
              <UserDashboard />
            </PrivateRoute>
          }
        />

        {/* Dynamic route for experiments - This handles /experiment/1, /experiment/2, etc. */}
        <Route
          path="/experiment/:id"
          element={
            <PrivateRoute>
              <ExperimentRouter />
            </PrivateRoute>
          }
        />

        {/* Keep the old /experiment route for backward compatibility */}
        <Route
          path="/experiment"
          element={
            <PrivateRoute>
              <ExperimentInterface />
            </PrivateRoute>
          }
        />

        <Route
          path="/admin"
          element={
            <PrivateRoute requiredRole="admin">
              <AdminDashboard />
            </PrivateRoute>
          }
        />
      </Routes>
    </div>
  );
}

function App() {
  return (
    <ThemeProvider>
      <AppContent />
    </ThemeProvider>
  );
}

export default App;
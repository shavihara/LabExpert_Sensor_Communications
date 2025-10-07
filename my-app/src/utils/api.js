import axios from 'axios';

// Determine the API URL based on the current hostname
const getApiUrl = () => {
  // If running in development (localhost/127.0.0.1)
  if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
    return import.meta.env.VITE_API_URL || 'http://localhost:5000';
  }

  // If accessing from network (e.g., 192.168.1.198)
  // Use the same host but with port 5000
  const networkUrl = `http://${window.location.hostname.replace(':3000', '')}:5000`;

  // Allow override via environment variable
  return import.meta.env.VITE_API_URL || networkUrl;
};

const API_URL = getApiUrl();

console.log('🌐 API URL:', API_URL);
console.log('📍 Current Location:', window.location.href);

const api = axios.create({
  baseURL: API_URL,
  withCredentials: true,
  headers: { 'Content-Type': 'application/json' },
});

// Request Interceptor
api.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('token');
    if (token && token !== 'undefined' && token !== null) {
      config.headers.Authorization = `Bearer ${token}`;
    } else {
      console.warn('Skipping Authorization header due to invalid token:', token, 'for URL:', config.url);
    }
    return config;
  },
  (error) => {
    console.error('Request interceptor error:', error);
    return Promise.reject(error);
  }
);

// Response Interceptor (for debugging)
api.interceptors.response.use(
  (response) => {
    console.log('✅ API Response:', response.config.url, response.status);
    return response;
  },
  (error) => {
    console.error('❌ API Error:', error.config?.url, error.response?.status, error.message);

    // Network error (backend not reachable)
    if (!error.response) {
      console.error('🔴 Cannot connect to backend at:', API_URL);
      console.error('💡 Make sure backend is running and accessible from this device');
    }

    return Promise.reject(error);
  }
);

// AUTH & USER FUNCTIONS

export const findUser = async (email, password) => {
  try {
    const response = await api.post('/api/auth/login', { email, password });
    console.log('findUser response:', response.data);
    if (response.data.success) {
      localStorage.setItem('token', response.data.token);
      console.log('Stored token:', response.data.token);
      return response.data.user || null;
    }
    return null;
  } catch (error) {
    console.error('Login error:', error);
    throw error;
  }
};

export const saveUser = async (userData) => {
  try {
    const response = await api.post('/api/auth/signup', userData);
    console.log('saveUser response:', response.data);
    if (response.data.success) {
      localStorage.setItem('token', response.data.token);
      console.log('Stored token:', response.data.token);
      return response.data.user || null;
    }
    throw new Error(response.data.message || 'Signup failed');
  } catch (error) {
    console.error('Signup error:', error);
    throw new Error(error.response?.data?.message || error.message || 'Signup failed. Please try again.');
  }
};

export const checkEmailExists = async (email) => {
  try {
    const response = await api.get('/api/auth/check-email', { params: { email } });
    return response.data.exists;
  } catch (error) {
    console.error('Check email error:', error);
    return false;
  }
};

export const getCurrentUser = async () => {
  try {
    const token = localStorage.getItem('token');
    if (!token || token === 'undefined' || token === null) {
      console.error('Invalid or undefined token in getCurrentUser, skipping request');
      return null;
    }
    const response = await api.get('/api/user/me');
    console.log('getCurrentUser response:', response.data);
    return response.data.user || null;
  } catch (error) {
    console.error('Get current user error:', error);
    return null;
  }
};

export const updateUserPassword = async (email, newPassword, otp, verifyOnly = false) => {
  try {
    const endpoint = verifyOnly ? '/api/auth/verify-otp' : '/api/auth/verify-otp';
    const payload = verifyOnly ? { email, otp } : { email, otp, newPassword };
    const response = await api.post(endpoint, payload);
    return response.data.success;
  } catch (error) {
    console.error('Update password error:', error);
    throw error;
  }
};

export const logoutUser = async () => {
  try {
    await api.post('/api/auth/logout');
    localStorage.removeItem('token');
    return true;
  } catch (error) {
    console.error('Logout error:', error);
    return false;
  }
};

export const sendForgotPasswordOTP = async (email) => {
  try {
    const response = await api.post('/api/auth/forgot-password', { email });
    return response.data;
  } catch (error) {
    console.error('Forgot password error:', error);
    throw error;
  }
};

// Health check function
export const checkBackendHealth = async () => {
  try {
    const response = await api.get('/api/health');
    console.log('✅ Backend is healthy:', response.data);
    return response.data;
  } catch (error) {
    console.error('❌ Backend health check failed:', error.message);
    return null;
  }
};

// ADDITIONAL APIs

export const userAPI = {
  uploadProfilePicture: async (formData) =>
    api.post('/api/files/profile-picture', formData, {
      headers: { 'Content-Type': 'multipart/form-data' }
    }),
};

export const fileAPI = {
  getFile: (fileId) => `${API_URL}/files/${fileId}`,
};

// Export API_URL for use in components
export { API_URL, api };
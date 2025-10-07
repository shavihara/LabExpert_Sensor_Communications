import { useState, useEffect } from 'react'
import { userAPI, fileAPI, getCurrentUser, authAPI } from '../utils/api'
import '../styles/Profile.css'

function Profile() {
  const [user, setUser] = useState(getCurrentUser())
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [profileData, setProfileData] = useState({
    name: user?.name || '',
    email: user?.email || ''
  })
  const [passwordData, setPasswordData] = useState({
    currentPassword: '',
    newPassword: '',
    confirmPassword: ''
  })
  const [verificationOtp, setVerificationOtp] = useState('')

  useEffect(() => {
    fetchProfile()
  }, [])

  const fetchProfile = async () => {
    try {
      const response = await userAPI.getProfile()
      if (response.success) {
        setUser(response.user)
        setProfileData({
          name: response.user.name,
          email: response.user.email
        })
      }
    } catch (err) {
      console.error('Failed to fetch profile:', err)
    }
  }

  const handleProfileUpdate = async (e) => {
    e.preventDefault()
    setError('')
    setSuccess('')
    setLoading(true)

    try {
      const response = await userAPI.updateProfile(profileData)
      if (response.success) {
        setUser(response.user)
        setSuccess('Profile updated successfully!')
        if (profileData.email !== user.email) {
          setSuccess('Profile updated! Please verify your new email.')
        }
      }
    } catch (err) {
      setError(err.response?.data?.message || 'Failed to update profile')
    } finally {
      setLoading(false)
    }
  }

  const handlePasswordChange = async (e) => {
    e.preventDefault()
    setError('')
    setSuccess('')

    if (passwordData.newPassword !== passwordData.confirmPassword) {
      setError('Passwords do not match')
      return
    }

    if (passwordData.newPassword.length < 6) {
      setError('Password must be at least 6 characters')
      return
    }

    setLoading(true)

    try {
      const response = await userAPI.changePassword(
        passwordData.currentPassword,
        passwordData.newPassword
      )
      if (response.success) {
        setSuccess('Password changed successfully!')
        setPasswordData({
          currentPassword: '',
          newPassword: '',
          confirmPassword: ''
        })
      }
    } catch (err) {
      setError(err.response?.data?.message || 'Failed to change password')
    } finally {
      setLoading(false)
    }
  }

  const handleProfilePictureUpload = async (e) => {
    const file = e.target.files[0]
    if (!file) return

    const formData = new FormData()
    formData.append('profilePicture', file)

    setLoading(true)
    setError('')

    try {
      const response = await userAPI.uploadProfilePicture(formData)
      if (response.success) {
        setSuccess('Profile picture uploaded successfully!')
        fetchProfile()
      }
    } catch (err) {
      setError(err.response?.data?.message || 'Failed to upload picture')
    } finally {
      setLoading(false)
    }
  }

  const handleEmailVerification = async () => {
    if (!verificationOtp) {
      setError('Please enter OTP')
      return
    }

    setLoading(true)
    setError('')

    try {
      const response = await authAPI.verifyEmail(verificationOtp)
      if (response.success) {
        setSuccess('Email verified successfully!')
        fetchProfile()
        setVerificationOtp('')
      }
    } catch (err) {
      setError(err.response?.data?.message || 'Failed to verify email')
    } finally {
      setLoading(false)
    }
  }

  const resendVerificationEmail = async () => {
    setLoading(true)
    setError('')

    try {
      const response = await authAPI.resendEmailVerification()
      if (response.success) {
        setSuccess('Verification email sent!')
      }
    } catch (err) {
      setError(err.response?.data?.message || 'Failed to send verification email')
    } finally {
      setLoading(false)
    }
  }

  return (
    <div className="profile-container">
      <h2>My Profile</h2>

      {error && <div className="error-message">{error}</div>}
      {success && <div className="success-message">{success}</div>}

      {/* Profile Picture Section */}
      <div className="profile-section">
        <h3>Profile Picture</h3>
        <div className="profile-picture-wrapper">
          {user?.profile_picture ? (
            <img 
              src={fileAPI.getFileUrl(user.profile_picture)} 
              alt="Profile" 
              className="profile-picture"
            />
          ) : (
            <div className="profile-picture-placeholder">No Picture</div>
          )}
          <input
            type="file"
            accept="image/*"
            onChange={handleProfilePictureUpload}
            disabled={loading}
            className="file-input"
          />
        </div>
      </div>

      {/* Email Verification Section */}
      {!user?.isEmailVerified && (
        <div className="profile-section verification-section">
          <h3>Email Verification Required</h3>
          <p>Please verify your email address to access all features.</p>
          <div className="verification-form">
            <input
              type="text"
              placeholder="Enter OTP"
              value={verificationOtp}
              onChange={(e) => setVerificationOtp(e.target.value)}
              maxLength="6"
              disabled={loading}
            />
            <button onClick={handleEmailVerification} disabled={loading}>
              Verify
            </button>
            <button onClick={resendVerificationEmail} disabled={loading}>
              Resend OTP
            </button>
          </div>
        </div>
      )}

      {/* Profile Information */}
      <div className="profile-section">
        <h3>Profile Information</h3>
        <form onSubmit={handleProfileUpdate}>
          <div className="form-group">
            <label>Name</label>
            <input
              type="text"
              value={profileData.name}
              onChange={(e) => setProfileData({...profileData, name: e.target.value})}
              disabled={loading}
            />
          </div>
          <div className="form-group">
            <label>Email</label>
            <input
              type="email"
              value={profileData.email}
              onChange={(e) => setProfileData({...profileData, email: e.target.value})}
              disabled={loading}
            />
          </div>
          <button type="submit" disabled={loading}>
            {loading ? 'Updating...' : 'Update Profile'}
          </button>
        </form>
      </div>

      {/* Change Password */}
      <div className="profile-section">
        <h3>Change Password</h3>
        <form onSubmit={handlePasswordChange}>
          <div className="form-group">
            <label>Current Password</label>
            <input
              type="password"
              value={passwordData.currentPassword}
              onChange={(e) => setPasswordData({...passwordData, currentPassword: e.target.value})}
              disabled={loading}
            />
          </div>
          <div className="form-group">
            <label>New Password</label>
            <input
              type="password"
              value={passwordData.newPassword}
              onChange={(e) => setPasswordData({...passwordData, newPassword: e.target.value})}
              disabled={loading}
            />
          </div>
          <div className="form-group">
            <label>Confirm New Password</label>
            <input
              type="password"
              value={passwordData.confirmPassword}
              onChange={(e) => setPasswordData({...passwordData, confirmPassword: e.target.value})}
              disabled={loading}
            />
          </div>
          <button type="submit" disabled={loading}>
            {loading ? 'Changing...' : 'Change Password'}
          </button>
        </form>
      </div>
    </div>
  )
}

export default Profile
import { useState, useEffect } from 'react';
import { useNavigate, Link } from 'react-router-dom';
import { updateUserPassword, sendForgotPasswordOTP } from '../utils/api';

function ForgotPassword() {
  const [email, setEmail] = useState('');
  const [otp, setOtp] = useState('');
  const [newPassword, setNewPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [step, setStep] = useState(1);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const [loading, setLoading] = useState(false);
  const [otpExpiry, setOtpExpiry] = useState(null);
  const [timeRemaining, setTimeRemaining] = useState(0);
  const navigate = useNavigate();

  useEffect(() => {
    let interval = null;
    if (otpExpiry && step === 2) {
      interval = setInterval(() => {
        const now = new Date();
        const remaining = Math.max(0, Math.floor((otpExpiry - now) / 1000));
        setTimeRemaining(remaining);
        if (remaining === 0) clearInterval(interval);
      }, 1000);
    }
    return () => clearInterval(interval);
  }, [otpExpiry, step]);

  const formatTime = (seconds) =>
    seconds === 0 ? 'Expired' : `${Math.floor(seconds / 60)}:${(seconds % 60).toString().padStart(2, '0')}`;

  const handleEmailSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setSuccess('');
    setLoading(true);

    if (!email || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
      setError('Please enter a valid email');
      setLoading(false);
      return;
    }

    try {
      console.log('Sending OTP to:', email);
      await sendForgotPasswordOTP(email);
      setOtpExpiry(new Date(Date.now() + 10 * 60 * 1000));
      setStep(2);
      setSuccess('OTP sent to your email!');
      console.log('OTP sent successfully');
    } catch (err) {
      console.error('Error sending OTP:', err);
      setError(err.response?.data?.message || 'Failed to send OTP');
    } finally {
      setLoading(false);
    }
  };

  const handleOtpSubmit = async (e) => {
    e.preventDefault();
    setError('');
    setSuccess('');
    setLoading(true);

    if (!otp || otp.length !== 6) {
      setError('Please enter a valid 6-digit OTP');
      setLoading(false);
      return;
    }

    if (timeRemaining === 0) {
      setError('OTP has expired. Please request a new one.');
      setLoading(false);
      return;
    }

    try {
      const response = await updateUserPassword(email, '', otp, true);

      if (response) {
        setStep(3);
        setSuccess('OTP verified! Set your new password.');
      } else {
        setError('Invalid OTP. Please try again.');
      }
    } catch (err) {
      console.error('Error verifying OTP:', err);
      setError(err.response?.data?.message || 'Invalid or expired OTP');
    } finally {
      setLoading(false);
    }
  };

  const handlePasswordReset = async (e) => {
    e.preventDefault();
    setError('');
    setLoading(true);

    if (!newPassword || !confirmPassword) {
      setError('Please fill in both password fields');
      setLoading(false);
      return;
    }

    if (newPassword !== confirmPassword) {
      setError('Passwords do not match');
      setLoading(false);
      return;
    }

    if (newPassword.length < 6) {
      setError('Password must be at least 6 characters long');
      setLoading(false);
      return;
    }

    try {
      console.log('Resetting password for:', email);
      await updateUserPassword(email, newPassword, otp, false);
      setSuccess('Password reset successfully! Redirecting to login...');
      setTimeout(() => navigate('/login'), 2000);
    } catch (err) {
      console.error('Error resetting password:', err);
      setError(err.response?.data?.message || 'Failed to reset password');
    } finally {
      setLoading(false);
    }
  };

  const resendOtp = async () => {
    setError('');
    setLoading(true);

    try {
      console.log('Resending OTP to:', email);
      await sendForgotPasswordOTP(email);
      setOtpExpiry(new Date(Date.now() + 10 * 60 * 1000));
      setOtp('');
      setSuccess('New OTP sent to your email!');
      console.log('OTP resent successfully');
    } catch (err) {
      console.error('Error resending OTP:', err);
      setError(err.response?.data?.message || 'Failed to resend OTP');
    } finally {
      setLoading(false);
    }
  };

  const handleOtpChange = (index, value, e) => {
    if (e.key === 'Backspace' && !value && index > 0) {
      const prevInput = document.querySelector(`.otp-box:nth-child(${index})`);
      if (prevInput) {
        prevInput.focus();
        const newOtp = otp.split('');
        newOtp[index - 1] = '';
        setOtp(newOtp.join(''));
      }
      return;
    }

    const val = value.replace(/\D/g, '');
    if (val.length <= 1) {
      const newOtp = otp.split('');
      newOtp[index] = val;
      setOtp(newOtp.join(''));

      if (val && index < 5) {
        const nextInput = document.querySelector(`.otp-box:nth-child(${index + 2})`);
        if (nextInput) nextInput.focus();
      }
    }
  };

  const handleOtpPaste = (e) => {
    e.preventDefault();
    const pastedData = e.clipboardData.getData('text').replace(/\D/g, '').slice(0, 6);
    setOtp(pastedData);

    const lastIndex = Math.min(pastedData.length, 6) - 1;
    const lastInput = document.querySelector(`.otp-box:nth-child(${lastIndex + 1})`);
    if (lastInput) lastInput.focus();
  };

  return (
    <div className="min-h-screen flex items-center justify-center bg-gradient-to-br from-purple-500 via-purple-600 to-purple-800  relative">
      <div className="bg-white/95 backdrop-blur-xl p-8 rounded-3xl shadow-2xl border border-white/20 w-11/12 max-w-md animate-[fadeSlideUp_1s_ease-out_0.1s_forwards] opacity-0">
        <h1 className="text-center text-purple-600 text-4xl mb-2 font-extrabold">
          Lab Expert
        </h1>
        <h2 className="text-center text-gray-600 text-xl mb-6 font-normal">
          {step === 1 ? 'Reset Password' : step === 2 ? 'Verify OTP' : 'Set New Password'}
        </h2>

        {step === 1 && (
          <form onSubmit={handleEmailSubmit} className="flex flex-col gap-6">
            {error && (
              <div className="bg-red-50 text-red-600 px-4 py-3 rounded-lg border border-red-200 text-sm text-center animate-[shake_0.3s_ease-in-out]">
                {error}
              </div>
            )}
            {success && (
              <div className="bg-green-50 text-green-600 px-4 py-3 rounded-lg border border-green-200 text-sm text-center">
                {success}
              </div>
            )}
            <div className="flex flex-col gap-2">
              <label htmlFor="email" className="text-gray-700 font-semibold text-sm">
                Email
              </label>
              <input
                type="email"
                id="email"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                placeholder="Enter your email"
                className="w-full px-4 py-3.5 border-2 border-gray-200 rounded-lg text-base transition-all duration-300 bg-white focus:outline-none focus:border-purple-600 focus:ring-4 focus:ring-purple-100 placeholder:text-gray-400 disabled:opacity-60 disabled:cursor-not-allowed"
                disabled={loading}
              />
            </div>
            <button
              type="submit"
              className="bg-gradient-to-br from-purple-600 to-purple-800 text-white px-4 py-4 rounded-xl text-base font-bold cursor-pointer transition-all duration-300 hover:-translate-y-0.5 hover:shadow-2xl hover:shadow-purple-500/30 disabled:opacity-60 disabled:cursor-not-allowed disabled:transform-none disabled:bg-gray-500"
              disabled={loading}
            >
              {loading ? 'Sending...' : 'Send OTP'}
            </button>
          </form>
        )}

        {step === 2 && (
          <form onSubmit={handleOtpSubmit} className="flex flex-col gap-6">
            {error && (
              <div className="bg-red-50 text-red-600 px-4 py-3 rounded-lg border border-red-200 text-sm text-center animate-[shake_0.3s_ease-in-out]">
                {error}
              </div>
            )}
            {success && (
              <div className="bg-green-50 text-green-600 px-4 py-3 rounded-lg border border-green-200 text-sm text-center">
                {success}
              </div>
            )}
            <p className="text-center text-gray-500 text-sm leading-relaxed">
              OTP sent to <strong>{email}</strong>
            </p>
            {otpExpiry && (
              <p className="text-center text-gray-500 text-sm px-3 py-3 bg-purple-50 rounded-lg border border-purple-100 transition-all duration-300 hover:bg-purple-100 cursor-pointer">
                Time remaining: <strong
                  className="text-base font-bold transition-colors duration-300"
                  style={{ color: timeRemaining <= 60 ? '#dc2626' : '#667eea' }}
                >
                  {formatTime(timeRemaining)}
                </strong>
              </p>
            )}
            <div className="flex flex-col gap-2">
              <label htmlFor="otp" className="text-gray-700 font-semibold text-sm">
                Enter 6-Digit OTP
              </label>
              <div className="flex justify-center items-center gap-3 my-4">
                {[0, 1, 2, 3, 4, 5].map((i) => (
                  <input
                    key={i}
                    type="text"
                    className={`otp-box w-12 h-12 text-center text-xl font-bold border-2 rounded-lg bg-white transition-all duration-300 outline-none focus:border-purple-600 focus:ring-4 focus:ring-purple-100 ${
                      otp[i] ? 'border-green-600 bg-green-50' : 'border-gray-200'
                    }`}
                    value={otp[i] || ''}
                    onChange={(e) => handleOtpChange(i, e.target.value, e)}
                    onKeyDown={(e) => {
                      if (e.key === 'Backspace' && !otp[i] && i > 0) {
                        e.preventDefault();
                        handleOtpChange(i, '', e);
                      }
                    }}
                    onPaste={i === 0 ? handleOtpPaste : undefined}
                    maxLength="1"
                  />
                ))}
              </div>
            </div>
            <button
              type="submit"
              className="bg-gradient-to-br from-purple-600 to-purple-800 text-white px-4 py-4 rounded-xl text-base font-bold cursor-pointer transition-all duration-300 hover:-translate-y-0.5 hover:shadow-2xl hover:shadow-purple-500/30 disabled:opacity-60 disabled:cursor-not-allowed disabled:transform-none disabled:bg-gray-500"
              disabled={loading || timeRemaining === 0}
            >
              {loading ? 'Verifying...' : 'Verify OTP'}
            </button>
            <div className="flex justify-between gap-4 mt-4">
              <button
                type="button"
                onClick={resendOtp}
                className="bg-transparent border-0 text-purple-600 cursor-pointer text-sm underline px-4 py-2 rounded transition-all duration-300 font-semibold hover:bg-purple-50 hover:no-underline hover:text-purple-700"
                disabled={loading}
              >
                {loading ? 'Sending...' : 'Resend OTP'}
              </button>
              <button
                type="button"
                onClick={() => {setStep(1); setOtp(''); setError(''); setSuccess('');}}
                className="bg-transparent border-0 text-purple-600 cursor-pointer text-sm underline px-4 py-2 rounded transition-all duration-300 font-semibold hover:bg-purple-50 hover:no-underline hover:text-purple-700"
              >
                Change Email
              </button>
            </div>
          </form>
        )}

        {step === 3 && (
          <form onSubmit={handlePasswordReset} className="flex flex-col gap-6">
            {error && (
              <div className="bg-red-50 text-red-600 px-4 py-3 rounded-lg border border-red-200 text-sm text-center animate-[shake_0.3s_ease-in-out]">
                {error}
              </div>
            )}
            {success && (
              <div className="bg-green-50 text-green-600 px-4 py-3 rounded-lg border border-green-200 text-sm text-center">
                {success}
              </div>
            )}
            <div className="flex flex-col gap-2">
              <label htmlFor="newPassword" className="text-gray-700 font-semibold text-sm">
                New Password
              </label>
              <input
                type="password"
                id="newPassword"
                value={newPassword}
                onChange={(e) => setNewPassword(e.target.value)}
                placeholder="New password"
                className="w-full px-4 py-3.5 border-2 border-gray-200 rounded-lg text-base transition-all duration-300 bg-white focus:outline-none focus:border-purple-600 focus:ring-4 focus:ring-purple-100 placeholder:text-gray-400 disabled:opacity-60 disabled:cursor-not-allowed"
                disabled={loading}
              />
            </div>
            <div className="flex flex-col gap-2">
              <label htmlFor="confirmPassword" className="text-gray-700 font-semibold text-sm">
                Confirm Password
              </label>
              <input
                type="password"
                id="confirmPassword"
                value={confirmPassword}
                onChange={(e) => setConfirmPassword(e.target.value)}
                placeholder="Confirm password"
                className="w-full px-4 py-3.5 border-2 border-gray-200 rounded-lg text-base transition-all duration-300 bg-white focus:outline-none focus:border-purple-600 focus:ring-4 focus:ring-purple-100 placeholder:text-gray-400 disabled:opacity-60 disabled:cursor-not-allowed"
                disabled={loading}
              />
            </div>
            <button
              type="submit"
              className="bg-gradient-to-br from-purple-600 to-purple-800 text-white px-4 py-4 rounded-xl text-base font-bold cursor-pointer transition-all duration-300 hover:-translate-y-0.5 hover:shadow-2xl hover:shadow-purple-500/30 disabled:opacity-60 disabled:cursor-not-allowed disabled:transform-none"
              disabled={loading}
            >
              {loading ? 'Resetting...' : 'Reset Password'}
            </button>
          </form>
        )}

        <div className="mt-6 text-center pt-4 border-t border-gray-200">
          <Link
            to="/login"
            className="text-purple-600 no-underline font-semibold transition-all duration-300 hover:text-purple-700 hover:underline"
          >
            Back to Login
          </Link>
        </div>
      </div>

      <style jsx>{`
        @keyframes fadeSlideUp {
          from {
            opacity: 0;
            transform: translateY(20px);
          }
          to {
            opacity: 1;
            transform: translateY(0);
          }
        }
        @keyframes shake {
          0%, 100% { transform: translateX(0); }
          25% { transform: translateX(-5px); }
          75% { transform: translateX(5px); }
        }
      `}</style>
    </div>
  );
}

export default ForgotPassword;
import React, { useState } from 'react';
import { userAPI, fileAPI } from '../utils/api';

function ProfilePictureUpload({ currentPictureId, onUploadSuccess }) {
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState('');
  const [preview, setPreview] = useState(null);

  const handleFileSelect = (e) => {
    const file = e.target.files[0];
    if (file) {
      const allowedTypes = ['image/jpeg', 'image/jpg', 'image/png', 'image/gif'];
      if (!allowedTypes.includes(file.type)) {
        setError('Please select a valid image file (JPEG, PNG, GIF)');
        return;
      }
      
      if (file.size > 5 * 1024 * 1024) {
        setError('File size must be less than 5MB');
        return;
      }
      
      const reader = new FileReader();
      reader.onloadend = () => {
        setPreview(reader.result);
      };
      reader.readAsDataURL(file);
      
      setError('');
    }
  };

  const handleUpload = async (e) => {
    e.preventDefault();
    const fileInput = e.target.querySelector('input[type="file"]');
    const file = fileInput?.files[0];
    
    if (!file) {
      setError('Please select a file');
      return;
    }
    
    setUploading(true);
    setError('');
    
    try {
      const formData = new FormData();
      formData.append('profilePicture', file);
      
      console.log('Uploading profile picture...');
      const response = await userAPI.uploadProfilePicture(formData);
      
      if (response.data.success) {
        console.log('Profile picture uploaded successfully');
        onUploadSuccess(response.data.file);
        setPreview(null);
        fileInput.value = '';
      }
    } catch (err) {
      console.error('Upload error:', err);
      setError(err.response?.data?.message || 'Failed to upload image');
    } finally {
      setUploading(false);
    }
  };

  return (
    <div className="profile-picture-upload">
      <form onSubmit={handleUpload}>
        <div className="current-picture">
          {currentPictureId ? (
            <img 
              src={fileAPI.getFile(currentPictureId)} 
              alt="Current profile" 
              className="profile-img"
              style={{ width: '100px', height: '100px', borderRadius: '50%', objectFit: 'cover' }}
            />
          ) : (
            <div className="no-picture" style={{ 
              width: '100px', 
              height: '100px', 
              borderRadius: '50%', 
              backgroundColor: '#f0f0f0', 
              display: 'flex', 
              alignItems: 'center', 
              justifyContent: 'center',
              color: '#666'
            }}>
              No picture
            </div>
          )}
        </div>
        
        {preview && (
          <div className="preview" style={{ marginTop: '15px' }}>
            <h4>Preview:</h4>
            <img 
              src={preview} 
              alt="Preview" 
              className="preview-img" 
              style={{ width: '100px', height: '100px', borderRadius: '50%', objectFit: 'cover' }}
            />
          </div>
        )}
        
        {error && <div className="error-message" style={{ color: 'red', margin: '10px 0' }}>{error}</div>}
        
        <input
          type="file"
          accept="image/*"
          onChange={handleFileSelect}
          disabled={uploading}
          style={{ margin: '15px 0' }}
        />
        
        <button 
          type="submit" 
          disabled={uploading || !preview}
          style={{
            backgroundColor: uploading || !preview ? '#ccc' : '#007bff',
            color: 'white',
            padding: '10px 20px',
            border: 'none',
            borderRadius: '5px',
            cursor: uploading || !preview ? 'not-allowed' : 'pointer'
          }}
        >
          {uploading ? 'Uploading...' : 'Upload Picture'}
        </button>
      </form>
    </div>
  );
}

export default ProfilePictureUpload;
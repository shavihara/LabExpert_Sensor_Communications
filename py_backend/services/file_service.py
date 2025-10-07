import uuid
import os
import shutil
from config.database import prepare  # From your database config

class FileService:
    @staticmethod
    def ensure_user_directories(user_id):
        base_dir = os.path.join(os.path.dirname(__file__), '../../uploads/users', user_id)
        profile_dir = os.path.join(base_dir, 'profile')
        documents_dir = os.path.join(base_dir, 'documents')
        for dir_path in [base_dir, profile_dir, documents_dir]:
            os.makedirs(dir_path, exist_ok=True)
        return {'base_dir': base_dir, 'profile_dir': profile_dir, 'documents_dir': documents_dir}

    @staticmethod
    def save_profile_picture(file, user_id):
        dirs = FileService.ensure_user_directories(user_id)
        profile_dir = dirs['profile_dir']
        # Delete existing profile pic if any
        stmt = prepare("SELECT * FROM files WHERE user_id = :user_id AND file_type = 'profile_picture' AND is_active = 1")
        existing = stmt({"user_id": user_id}).fetchone()
        if existing:
            FileService.delete_file(existing['id'])
        file_id = str(uuid.uuid4())
        ext = os.path.splitext(file.filename)[1]
        filename = f"profile-{int(datetime.now().timestamp())}{ext}"
        file_path = os.path.join(profile_dir, filename)
        with open(file_path, "wb") as f:
            shutil.copyfileobj(file.file, f)
        insert_stmt = prepare("""
            INSERT INTO files (id, user_id, file_type, original_name, filename, file_path, mimetype, size)
            VALUES (:id, :user_id, :file_type, :original_name, :filename, :file_path, :mimetype, :size)
        """)
        insert_stmt({
            "id": file_id, "user_id": user_id, "file_type": 'profile_picture',
            "original_name": file.filename, "filename": filename, "file_path": file_path,
            "mimetype": file.content_type, "size": file.size
        }).execute()
        return {"id": file_id, "filename": filename, "file_path": file_path}

    @staticmethod
    def delete_file(file_id):
        stmt = prepare("SELECT * FROM files WHERE id = :id AND is_active = 1")
        file = stmt({"id": file_id}).fetchone()
        if file:
            try:
                os.remove(file['file_path'])
            except Exception as e:
                print(f"Error deleting file: {e}")
            update_stmt = prepare("UPDATE files SET is_active = 0 WHERE id = :id")
            update_stmt({"id": file_id}).execute()

    # Add save_document, get_file, etc., if needed from original
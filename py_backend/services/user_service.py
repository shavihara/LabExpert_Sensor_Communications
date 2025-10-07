import uuid
import bcrypt
from datetime import datetime, timedelta
from sqlalchemy import text
from config.database import engine

class UserService:
    @staticmethod
    def find_by_id(user_id):
        stmt = text("SELECT * FROM users WHERE id = :id AND is_active = 1")
        with engine.connect() as conn:
            result = conn.execute(stmt, {"id": user_id})
            row = result.fetchone()
            if row:
                return {
                    'id': row[0],
                    'name': row[1],
                    'email': row[2],
                    'password': row[3],
                    'role': row[4] if len(row) > 4 else 'user',
                    'is_email_verified': row[5] if len(row) > 5 else False,
                    'is_active': row[6] if len(row) > 6 else True,
                    'created_at': row[7] if len(row) > 7 else None,
                    'last_login': row[8] if len(row) > 8 else None
                }
            return None

    @staticmethod
    def find_by_email(email):
        stmt = text("SELECT * FROM users WHERE email = :email AND is_active = 1")
        with engine.connect() as conn:
            result = conn.execute(stmt, {"email": email})
            row = result.fetchone()
            if row:
                return {
                    'id': row[0],
                    'name': row[1],
                    'email': row[2],
                    'password': row[3],
                    'role': row[4] if len(row) > 4 else 'user',
                    'is_email_verified': row[5] if len(row) > 5 else False,
                    'is_active': row[6] if len(row) > 6 else True,
                    'created_at': row[7] if len(row) > 7 else None,
                    'last_login': row[8] if len(row) > 8 else None
                }
            return None

    @staticmethod
    def validate_password(plain_password, hashed_password):
        return bcrypt.checkpw(plain_password.encode(), hashed_password.encode())

    @staticmethod
    def update_last_login(user_id):
        stmt = text("UPDATE users SET last_login = :now WHERE id = :id")
        with engine.begin() as conn:
            conn.execute(stmt, {"id": user_id, "now": datetime.now().isoformat()})

    @staticmethod
    def create(user_data):
        user_id = str(uuid.uuid4())
        hashed_pw = bcrypt.hashpw(user_data['password'].encode(), bcrypt.gensalt()).decode()

        stmt = text("""
            INSERT INTO users (id, name, email, password, role, is_email_verified, is_active)
            VALUES (:id, :name, :email, :hashed_pw, :role, 0, 1)
        """)

        with engine.begin() as conn:
            conn.execute(stmt, {
                "id": user_id,
                "name": user_data['name'],
                "email": user_data['email'],
                "hashed_pw": hashed_pw,
                "role": "user"
            })

        return {
            "id": user_id,
            "name": user_data['name'],
            "email": user_data['email'],
            "role": "user"
        }

    @staticmethod
    def count_all():
        stmt = text("SELECT COUNT(*) FROM users WHERE is_active = 1")
        with engine.connect() as conn:
            result = conn.execute(stmt)
            return result.scalar() or 0

    @staticmethod
    def count_verified():
        stmt = text("SELECT COUNT(*) FROM users WHERE is_email_verified = 1 AND is_active = 1")
        with engine.connect() as conn:
            result = conn.execute(stmt)
            return result.scalar() or 0

    @staticmethod
    def count_recent():
        thirty_days_ago = (datetime.now() - timedelta(days=30)).isoformat()
        stmt = text("SELECT COUNT(*) FROM users WHERE created_at >= :thirty_days_ago AND is_active = 1")
        with engine.connect() as conn:
            result = conn.execute(stmt, {"thirty_days_ago": thirty_days_ago})
            return result.scalar() or 0

    @staticmethod
    def get_recent(limit=5):
        stmt = text("""
            SELECT id, name, email, created_at 
            FROM users 
            WHERE is_active = 1 
            ORDER BY created_at DESC 
            LIMIT :limit
        """)
        with engine.connect() as conn:
            result = conn.execute(stmt, {"limit": limit})
            return [
                {
                    "id": row[0],
                    "name": row[1],
                    "email": row[2],
                    "created_at": row[3]
                } for row in result.fetchall()
            ]

    @staticmethod
    def get_all():
        stmt = text("""
            SELECT id, name, email, role, is_email_verified, is_active, last_login, created_at 
            FROM users 
            WHERE is_active = 1 
            ORDER BY created_at DESC
        """)
        with engine.connect() as conn:
            result = conn.execute(stmt)
            return [
                {
                    "id": row[0],
                    "name": row[1],
                    "email": row[2],
                    "role": row[3],
                    "is_email_verified": row[4],
                    "is_active": row[5],
                    "last_login": row[6],
                    "created_at": row[7]
                } for row in result.fetchall()
            ]
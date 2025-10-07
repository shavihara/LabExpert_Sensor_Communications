import secrets
from datetime import datetime, timedelta
from sqlalchemy import text
from config.database import engine

class SessionService:
    @staticmethod
    def find_by_token(token):
        stmt = text("""
                    SELECT s.*, u.name, u.email, u.role
                    FROM sessions s
                             JOIN users u ON s.user_id = u.id
                    WHERE s.token = :token
                      AND s.is_active = 1
                      AND s.expires_at > :now
                    """)
        now = datetime.now().isoformat()

        with engine.connect() as conn:
            result = conn.execute(stmt, {"token": token, "now": now})
            row = result.fetchone()
            if row:
                return {
                    'user_id': row[1],  # s.user_id
                    'token': row[2],  # s.token
                    'ip_address': row[3],  # s.ip_address
                    'expires_at': row[4],  # s.expires_at
                    'is_active': row[5],  # s.is_active
                    'last_activity': row[6],  # s.last_activity
                    'created_at': row[7],  # s.created_at
                    'name': row[8],  # u.name
                    'email': row[9],  # u.email
                    'role': row[10]  # u.role
                }
            return None

    @staticmethod
    def create(user_id, ip_address):
        token = secrets.token_hex(32)  # generates a secure 64-char token
        expires_at = (datetime.now() + timedelta(days=7)).isoformat()
        now = datetime.now().isoformat()

        stmt = text("""
                    INSERT INTO sessions (user_id, token, ip_address, expires_at, is_active, last_activity)
                    VALUES (:user_id, :token, :ip_address, :expires_at, 1, :now)
                    """)

        with engine.begin() as conn:
            conn.execute(stmt, {
                "user_id": user_id,
                "token": token,
                "ip_address": ip_address,
                "expires_at": expires_at,
                "now": now
            })

        return token

    @staticmethod
    def update_activity(token):
        stmt = text("""
                    UPDATE sessions
                    SET last_activity = :now
                    WHERE token = :token
                      AND is_active = 1
                    """)
        now = datetime.now().isoformat()

        with engine.begin() as conn:
            conn.execute(stmt, {"token": token, "now": now})

    @staticmethod
    def count_active():
        now = datetime.now().isoformat()
        stmt = text("""
            SELECT COUNT(*) 
            FROM sessions 
            WHERE is_active = 1 AND expires_at > :now
        """)
        with engine.connect() as conn:
            result = conn.execute(stmt, {"now": now})
            return result.scalar() or 0

    @staticmethod
    def get_active(limit=5):
        now = datetime.now().isoformat()
        stmt = text("""
            SELECT s.user_id, u.name, u.email, s.last_activity 
            FROM sessions s
            JOIN users u ON s.user_id = u.id
            WHERE s.is_active = 1 AND s.expires_at > :now
            ORDER BY s.last_activity DESC 
            LIMIT :limit
        """)
        with engine.connect() as conn:
            result = conn.execute(stmt, {"now": now, "limit": limit})
            return [
                {
                    "user_id": row[0],
                    "name": row[1],
                    "email": row[2],
                    "last_activity": row[3]
                } for row in result.fetchall()
            ]
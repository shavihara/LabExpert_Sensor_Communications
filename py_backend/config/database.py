import sqlite3
from sqlalchemy import create_engine, text
from sqlalchemy.orm import sessionmaker

DB_PATH = './data/lab_expert.db'
engine = create_engine(f'sqlite:///{DB_PATH}', connect_args={'timeout': 5, 'check_same_thread': False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

# Function for raw queries (like your prepare)
def prepare(sql):
    def execute(params=None):
        with engine.connect() as conn:
            return conn.execute(text(sql), params or {})
    return execute

# Run migrations (create tables if not exist)
def init_db():
    with engine.connect() as conn:
        # Users table
        conn.execute(text("""
        CREATE TABLE IF NOT EXISTS users(
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            role TEXT DEFAULT 'user',
            is_email_verified INTEGER DEFAULT 0,
            is_active INTEGER DEFAULT 1,
            profile_picture TEXT,
            last_login DATETIME,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME
        );
        """))

        # Sessions table
        conn.execute(text("""
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id TEXT NOT NULL,
            token TEXT NOT NULL,
            ip_address TEXT,
            expires_at DATETIME NOT NULL,
            is_active INTEGER DEFAULT 1,
            last_activity DATETIME,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
        """))

        # OTPs table
        conn.execute(text("""
        CREATE TABLE IF NOT EXISTS otps (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id TEXT NOT NULL,
            otp_code TEXT NOT NULL,
            purpose TEXT NOT NULL,
            expires_at DATETIME NOT NULL,
            is_used INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
        """))

        # Files table
        conn.execute(text("""
        CREATE TABLE IF NOT EXISTS files (
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            file_type TEXT NOT NULL,
            original_name TEXT NOT NULL,
            filename TEXT NOT NULL,
            file_path TEXT NOT NULL,
            mimetype TEXT,
            size INTEGER,
            is_active INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
        """))

        # WAL mode for concurrency
        conn.execute(text("PRAGMA journal_mode = WAL;"))
        conn.execute(text("PRAGMA synchronous = NORMAL;"))

init_db()
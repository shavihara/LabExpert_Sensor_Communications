from config.database import engine
with engine.connect() as conn:
    conn.execute(text("ALTER TABLE otps ADD COLUMN is_verified INTEGER DEFAULT 0"))
print("Migration completed")
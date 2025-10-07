import random
import string
import pyotp
import qrcode
from datetime import datetime, timedelta
from sqlalchemy import text
from config.database import engine


class OTPService:
    @staticmethod
    def create_otp(user_id, purpose="password_reset"):
        # Generate 6-digit OTP
        otp_code = ''.join(random.choices(string.digits, k=6))
        expires_at = (datetime.now() + timedelta(minutes=10)).isoformat()  # 10 min expiry

        stmt = text("""
                    INSERT INTO otps (user_id, otp_code, purpose, expires_at, is_used)
                    VALUES (:user_id, :otp_code, :purpose, :expires_at, 0)
                    """)

        with engine.begin() as conn:
            conn.execute(stmt, {
                "user_id": user_id,
                "otp_code": otp_code,
                "purpose": purpose,
                "expires_at": expires_at
            })

        return otp_code

    @staticmethod
    @staticmethod
    def verify_otp(user_id, otp_code, purpose="password_reset"):
        stmt = text("""
                    SELECT id, expires_at
                    FROM otps
                    WHERE user_id = :user_id
                      AND otp_code = :otp_code
                      AND purpose = :purpose
                      AND is_used = 0
                      AND expires_at > :now
                    ORDER BY created_at DESC LIMIT 1
                    """)

        now = datetime.now().isoformat()

        with engine.connect() as conn:
            result = conn.execute(stmt, {
                "user_id": user_id,
                "otp_code": otp_code,
                "purpose": purpose,
                "now": now
            })
            row = result.fetchone()

            if row:
                # Mark as used
                update_stmt = text("UPDATE otps SET is_used = 1 WHERE id = :id")
                with engine.begin() as conn2:
                    conn2.execute(update_stmt, {"id": row[0]})
                return True

            return False

    @staticmethod
    def generate_secret(user_id):
        # For TOTP (Google Authenticator style)
        return pyotp.random_base32()

    @staticmethod
    def get_qr_code(email, secret):
        # Generate QR code for TOTP setup
        totp = pyotp.TOTP(secret)
        qr_url = totp.provisioning_uri(
            name=email,
            issuer_name="Lab Expert"
        )

        # Generate QR code image (optional - you might want to return just the URL)
        qr = qrcode.QRCode(version=1, box_size=10, border=5)
        qr.add_data(qr_url)
        qr.make(fit=True)

        return qr_url
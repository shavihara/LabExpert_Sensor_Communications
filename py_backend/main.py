# main.py
# Lab Expert Backend API
from datetime import datetime
from fastapi import FastAPI, Depends, HTTPException, UploadFile, File, Query, Request, Header
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from fastapi.responses import JSONResponse
from pydantic import BaseModel, EmailStr
import yagmail
from dotenv import load_dotenv
import os
from services.user_service import UserService
from services.session_service import SessionService
from services.otp_service import OTPService
from services.file_service import FileService
import platform
import psutil
import time
from sse_starlette.sse import EventSourceResponse
import logging
from pathlib import Path
from sensor_service import (
    get_device_id,
    upload_firmware,
    live_distance_generator,
    collect_displacement,
    collect_oscillations,
    check_esp32_connection,
    configure_experiment,
    start_experiment,
    stop_experiment
)
from enum import Enum
import socket

from services.oscillation_service import (
    check_osi_connection,
    configure_osi_experiment,
    start_osi_experiment,
    stop_osi_experiment,
    reset_osi_count,
    live_oscillation_generator,
    get_osi_data
)

# Configure logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

# Load .env config
load_dotenv()

app = FastAPI()


# Get local IP address
def get_local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception:
        return "127.0.0.1"


LOCAL_IP = get_local_ip()
logger.info(f"🌐 Local IP Address: {LOCAL_IP}")

# ------------------ CORS ------------------
# UPDATED: Allow requests from network IP
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://127.0.0.1:5173",
        "http://localhost:3000",
        "http://127.0.0.1:3000",
        f"http://{LOCAL_IP}:5173",
        f"http://{LOCAL_IP}:3000",
        "http://192.168.137.1:3000",
        "http://192.168.1.198:3000",  # Your specific phone IP
        "http://192.168.1.*:3000",  # Allow any device on network
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


# Global exception handler to ensure CORS headers on errors
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    logger.error(f"Uncaught exception for request {request.url}: {str(exc)}", exc_info=True)
    origin = request.headers.get("origin", "http://localhost:5173")
    return JSONResponse(
        status_code=500,
        content={"detail": f"Internal Server Error: {str(exc)}"},
        headers={
            "Access-Control-Allow-Origin": origin,
            "Access-Control-Allow-Credentials": "true",
            "Access-Control-Allow-Methods": "*",
            "Access-Control-Allow-Headers": "*",
        }
    )


# ------------------ Models ------------------
class LoginRequest(BaseModel):
    email: EmailStr
    password: str


class SignupRequest(BaseModel):
    name: str
    email: EmailStr
    password: str


class ForgotPasswordRequest(BaseModel):
    email: EmailStr


class OTPVerifyRequest(BaseModel):
    otp: str
    email: EmailStr


class ExperimentConfig(BaseModel):
    frequency: int  # Sampling frequency in Hz
    duration: int  # Duration in seconds
    mode: str = "distance"  # measurement mode


class ExperimentData(BaseModel):
    data: list
    metadata: dict = {}


# ------------------ Auth ------------------
security = HTTPBearer()


async def get_current_user(credentials: HTTPAuthorizationCredentials = Depends(security)):
    token = credentials.credentials
    if not token:
        raise HTTPException(status_code=401, detail="Invalid authentication credentials")

    session = SessionService.find_by_token(token)
    if not session:
        raise HTTPException(status_code=401, detail="Invalid or expired token")

    user = UserService.find_by_id(session['user_id'])
    if not user:
        raise HTTPException(status_code=401, detail="User not found")

    SessionService.update_activity(token)
    return user


async def get_current_user_sensor(authorization: str = Header(None), token: str = Query(None)):
    token_to_use = authorization or token
    if not token_to_use:
        raise HTTPException(status_code=401, detail="No token provided")

    if token_to_use.startswith("Bearer "):
        token_to_use = token_to_use.split(" ")[1]

    logger.debug(f"Token used for sensor: {token_to_use}")
    session = SessionService.find_by_token(token_to_use)
    if not session:
        raise HTTPException(status_code=401, detail="Invalid or expired token")

    user = UserService.find_by_id(session['user_id'])
    if not user:
        raise HTTPException(status_code=401, detail="User not found")

    SessionService.update_activity(token_to_use)
    return user


# ------------------ Routes ------------------

@app.get("/")
async def root():
    """Root endpoint to verify server is running"""
    return {
        "success": True,
        "message": "Lab Expert API is running",
        "local_ip": LOCAL_IP,
        "access_urls": [
            f"http://localhost:5000",
            f"http://{LOCAL_IP}:5000"
        ]
    }


@app.get("/api/user/me")
async def get_me(current_user=Depends(get_current_user)):
    return {"success": True, "user": current_user}


@app.post("/api/auth/login")
async def login(req: LoginRequest):
    user = UserService.find_by_email(req.email)
    if not user or not UserService.validate_password(req.password, user['password']):
        raise HTTPException(401, "Invalid credentials")

    UserService.update_last_login(user['id'])
    token = SessionService.create(user['id'], "127.0.0.1")

    return {
        "success": True,
        "token": token,
        "user": {
            "id": user['id'],
            "name": user['name'],
            "email": user['email'],
            "role": user['role'],
        },
    }


@app.post("/api/auth/signup")
async def signup(req: SignupRequest):
    existing = UserService.find_by_email(req.email)
    if existing:
        raise HTTPException(400, "Email already exists")

    user = UserService.create(
        {"name": req.name, "email": req.email, "password": req.password}
    )
    token = SessionService.create(user['id'], "127.0.0.1")

    return {
        "success": True,
        "token": token,
        "user": {"id": user['id'], "name": req.name, "email": req.email},
    }


@app.get("/api/auth/check-email")
async def check_email(email: EmailStr = Query(...)):
    user = UserService.find_by_email(email)
    return {"exists": bool(user)}


@app.get("/api/auth/setup-otp")
async def setup_otp(current_user=Depends(get_current_user)):
    secret = OTPService.generate_secret(current_user['id'])
    qr_path = OTPService.get_qr_code(current_user['email'], secret)
    return {"success": True, "qr_url": qr_path}


@app.post("/api/auth/verify-otp")
async def verify_otp(req: OTPVerifyRequest):
    logger.debug(f"Received verify-otp request: {req.dict()}")
    try:
        user = UserService.find_by_email(req.email)
        if not user:
            logger.error(f"User not found for email: {req.email}")
            raise HTTPException(status_code=404, detail="User not found")

        if OTPService.verify_otp(user['id'], req.otp, purpose="password_reset"):
            logger.info(f"OTP verified successfully for user: {req.email}")
            return {"success": True}
        logger.warning(f"Invalid OTP for user: {req.email}")
        raise HTTPException(status_code=400, detail="Invalid OTP")
    except Exception as e:
        logger.error(f"Error in verify-otp: {str(e)}", exc_info=True)
        raise


@app.post("/api/auth/forgot-password")
async def forgot_password(req: ForgotPasswordRequest):
    user = UserService.find_by_email(req.email)
    if not user:
        raise HTTPException(404, "User not found")

    otp = OTPService.create_otp(user['id'], "password_reset")
    try:
        yag.send(to=req.email, subject="Reset Password", contents=f"Your OTP: {otp}")
    except Exception as e:
        raise HTTPException(500, f"Email send failed: {str(e)}")

    return {"success": True, "message": "OTP sent"}


@app.get("/api/health")
def health():
    return {
        "success": True,
        "message": "Lab Expert API is running",
        "local_ip": LOCAL_IP
    }


@app.post("/api/files/profile-picture")
async def upload_profile(file: UploadFile = File(...), current_user=Depends(get_current_user)):
    saved = FileService.save_profile_picture(file, current_user['id'])
    return {"success": True, "file": saved}


# ------------------ Admin Routes ------------------

def require_admin(user):
    """Allow only admin email to access admin routes"""
    if user.get("email") != "labexpert.us@gmail.com":
        raise HTTPException(403, "Forbidden: Admins only")


@app.get("/api/admin/dashboard")
async def admin_dashboard(current_user=Depends(get_current_user)):
    require_admin(current_user)

    data = {
        "stats": {
            "totalUsers": UserService.count_all(),
            "verifiedUsers": UserService.count_verified(),
            "recentSignups": UserService.count_recent(),
            "activeSessions": SessionService.count_active(),
        },
        "recentUsers": UserService.get_recent(5),
        "activeSessions_list": SessionService.get_active(5),
    }
    return {"success": True, "data": data}


@app.get("/api/admin/users")
async def admin_users(current_user=Depends(get_current_user)):
    require_admin(current_user)
    users = UserService.get_all()
    return {"success": True, "users": users}


@app.get("/api/admin/system")
async def admin_system(current_user=Depends(get_current_user)):
    require_admin(current_user)

    uptime = time.time() - psutil.boot_time()
    memory = psutil.virtual_memory()

    system_info = {
        "database": "SQLite",
        "version": "1.0.0",
        "nodeVersion": "N/A",
        "platform": platform.system(),
        "uptime": uptime,
        "memoryUsage": {
            "rss": memory.used,
            "heapUsed": memory.active if hasattr(memory, "active") else memory.used,
            "heapTotal": memory.total,
        },
    }
    return {"success": True, "system": system_info}


# ------------------ Sensor Routes ------------------

@app.get("/api/sensor/status")
async def sensor_status(current_user=Depends(get_current_user)):
    """Check ESP32 connection status"""
    status = await check_esp32_connection()
    return {"success": True, "status": status}


@app.post("/api/sensor/configure")
async def configure_sensor(config: ExperimentConfig, current_user=Depends(get_current_user)):
    """Configure experiment parameters"""
    result = await configure_experiment(config.frequency, config.duration, config.mode)
    if result["success"]:
        return {"success": True, "config": result["config"]}
    raise HTTPException(500, result.get("error", "Configuration failed"))


@app.post("/api/sensor/start")
async def start_sensor(current_user=Depends(get_current_user)):
    """Start experiment"""
    result = await start_experiment()
    if result["success"]:
        return {"success": True}
    raise HTTPException(500, result.get("error", "Failed to start experiment"))


@app.post("/api/sensor/stop")
async def stop_sensor(current_user=Depends(get_current_user)):
    """Stop experiment"""
    result = await stop_experiment()
    if result["success"]:
        return {"success": True}
    raise HTTPException(500, result.get("error", "Failed to stop experiment"))


@app.get("/api/sensor/stream")
async def stream_sensor_data(token: str = Query(None)):
    """SSE endpoint for live sensor data - uses query param for auth"""
    if not token:
        raise HTTPException(status_code=401, detail="No token provided")

    # Validate token
    session = SessionService.find_by_token(token)
    if not session:
        raise HTTPException(status_code=401, detail="Invalid or expired token")

    user = UserService.find_by_id(session['user_id'])
    if not user:
        raise HTTPException(status_code=401, detail="User not found")

    SessionService.update_activity(token)

    logger.debug("Starting SSE stream for authenticated user")
    return EventSourceResponse(live_distance_generator())


@app.get("/api/sensor/displacement")
async def collect_displacement_endpoint(current_user=Depends(get_current_user)):
    """Collect displacement data"""
    result = await collect_displacement()
    if result["success"]:
        return {"success": True, "data": result["data"]}
    raise HTTPException(500, result.get("error", "Failed to collect displacement"))


@app.get("/api/sensor/oscillations")
async def collect_oscillations_endpoint(
        n: int = Query(3, ge=1, le=10),
        current_user=Depends(get_current_user)
):
    """Collect oscillation data"""
    result = await collect_oscillations(n)
    if result["success"]:
        return {"success": True, "results": result["results"]}
    raise HTTPException(500, result.get("error", "Failed to collect oscillations"))


@app.post("/api/sensor/upload_firmware")
async def handle_upload_firmware(current_user=Depends(get_current_user)):
    """Upload firmware to ESP32"""
    device_id = await get_device_id()
    if not device_id:
        raise HTTPException(500, "Failed to get device ID")

    bin_path = Path("bin") / f"{device_id}.bin"
    if not bin_path.exists():
        raise HTTPException(404, f"Firmware file '{bin_path}' not found")

    success = await upload_firmware(bin_path)
    if success:
        return {"success": True, "message": "Firmware uploaded successfully"}
    else:
        raise HTTPException(500, "Firmware upload failed")


@app.post("/api/sensor/save_data")
async def save_experiment_data(data: ExperimentData, current_user=Depends(get_current_user)):
    """Save experiment data to user profile"""
    try:
        # TODO: Implement actual database storage
        # For now, just acknowledge receipt
        logger.info(f"Saving experiment data for user {current_user['id']}")
        return {"success": True, "message": "Data saved to profile"}
    except Exception as e:
        logger.error(f"Failed to save experiment data: {e}")
        raise HTTPException(500, f"Failed to save data: {str(e)}")
    
# ------------------ OSI Sensor Routes ------------------

@app.get("/api/osi/status")
async def osi_status(current_user=Depends(get_current_user)):
    """Check OSI sensor connection status"""
    status = await check_osi_connection()
    return {"success": True, "status": status}


@app.post("/api/osi/configure")
async def configure_osi(config: ExperimentConfig, current_user=Depends(get_current_user)):
    """Configure OSI experiment parameters"""
    result = await configure_osi_experiment(config.frequency, config.duration)
    if result["success"]:
        return {"success": True, "config": result["config"]}
    raise HTTPException(500, result.get("error", "Configuration failed"))


@app.post("/api/osi/start")
async def start_osi(current_user=Depends(get_current_user)):
    """Start OSI counting"""
    result = await start_osi_experiment()
    if result["success"]:
        return {"success": True}
    raise HTTPException(500, result.get("error", "Failed to start"))


@app.post("/api/osi/stop")
async def stop_osi(current_user=Depends(get_current_user)):
    """Stop OSI counting"""
    result = await stop_osi_experiment()
    if result["success"]:
        return {"success": True}
    raise HTTPException(500, result.get("error", "Failed to stop"))


@app.post("/api/osi/reset")
async def reset_osi(current_user=Depends(get_current_user)):
    """Reset OSI counter"""
    result = await reset_osi_count()
    if result["success"]:
        return {"success": True}
    raise HTTPException(500, result.get("error", "Failed to reset"))


@app.get("/api/osi/stream")
async def stream_osi_data(token: str = Query(None)):
    """SSE endpoint for live OSI count data"""
    if not token:
        raise HTTPException(status_code=401, detail="No token provided")

    session = SessionService.find_by_token(token)
    if not session:
        raise HTTPException(status_code=401, detail="Invalid or expired token")

    user = UserService.find_by_id(session['user_id'])
    if not user:
        raise HTTPException(status_code=401, detail="User not found")

    SessionService.update_activity(token)

    logger.debug("Starting OSI SSE stream")
    return EventSourceResponse(live_oscillation_generator())


@app.get("/api/osi/data")
async def get_osi_data_endpoint(current_user=Depends(get_current_user)):
    """Get all collected OSI data"""
    result = await get_osi_data()
    if result["success"]:
        return {"success": True, "data": result["data"]}
    raise HTTPException(500, result.get("error", "Failed to get data"))


@app.post("/api/osi/save_data")
async def save_osi_data(data: ExperimentData, current_user=Depends(get_current_user)):
    """Save OSI experiment data to user profile"""
    try:
        logger.info(f"Saving OSI data for user {current_user['id']}")
        return {"success": True, "message": "Data saved to profile"}
    except Exception as e:
        logger.error(f"Failed to save OSI data: {e}")
        raise HTTPException(500, f"Failed to save data: {str(e)}")


# -----------------------------------------------------EXPERIMENT SELECTION---------------------------------------------------------

class ExperimentType(Enum):
    DISTANCE = "distance"
    OSCILLATION = "oscillation"
    DISPLACEMENT = "displacement"


class ExperimentSelectRequest(BaseModel):
    experiment_type: ExperimentType


@app.post("/api/sensor/select_experiment")
async def select_experiment(
        request: ExperimentSelectRequest,
        current_user=Depends(get_current_user)
):
    """Select experiment and upload appropriate firmware"""
    try:
        # Get device ID
        device_id = await get_device_id()
        if not device_id:
            raise HTTPException(500, "Failed to get device ID from ESP32")

        # Determine firmware file based on experiment type
        firmware_map = {
            ExperimentType.DISTANCE: f"{device_id}.bin",
            ExperimentType.OSCILLATION: f"{device_id}_OSC.bin",
            ExperimentType.DISPLACEMENT: f"{device_id}.bin"
        }

        firmware_file = firmware_map.get(request.experiment_type)
        if not firmware_file:
            raise HTTPException(400, f"Unknown experiment type: {request.experiment_type}")

        bin_path = Path("bin") / firmware_file
        if not bin_path.exists():
            raise HTTPException(
                404,
                f"Firmware file '{firmware_file}' not found. Please compile and place in bin/ folder"
            )

        # Upload firmware via OTA
        logger.info(f"Uploading firmware: {firmware_file}")
        success = await upload_firmware(bin_path)

        if success:
            logger.info("Firmware upload successful")
            # Check if ESP32 is back online
            status = await check_esp32_connection()
            if status["connected"]:
                return {
                    "success": True,
                    "message": f"Firmware uploaded successfully. ESP32 ready for {request.experiment_type.value} experiment",
                    "firmware": firmware_file
                }
            else:
                return {
                    "success": True,
                    "message": "Firmware uploaded but ESP32 not responding yet. Please wait...",
                    "firmware": firmware_file
                }
        else:
            raise HTTPException(500, "Firmware upload failed")

    except Exception as e:
        logger.error(f"Experiment selection failed: {e}")
        raise HTTPException(500, f"Failed to select experiment: {str(e)}")


@app.get("/api/sensor/available_experiments")
async def get_available_experiments(current_user=Depends(get_current_user)):
    """Get list of available experiments based on compiled firmwares"""
    device_id = await get_device_id()
    if not device_id:
        return {"success": False, "experiments": []}

    bin_folder = Path("bin")
    available = []

    # Check which firmware files exist
    experiments = [
        {"type": "distance", "file": f"{device_id}.bin", "name": "Distance Measurement"},
        {"type": "oscillation", "file": f"{device_id}_OSC.bin", "name": "Oscillation Timing"},
        {"type": "displacement", "file": f"{device_id}_ESP8266.bin", "name": "Displacement Analysis"}
    ]

    for exp in experiments:
        if (bin_folder / exp["file"]).exists():
            available.append({
                "type": exp["type"],
                "name": exp["name"],
                "firmware": exp["file"]
            })

    return {"success": True, "experiments": available}


# ------------------ Email Setup ------------------
yag = None
try:
    yag = yagmail.SMTP(os.getenv("EMAIL_USER"), os.getenv("EMAIL_PASS"))
    yag.send(to=os.getenv("EMAIL_USER"), subject="LAB EXPERT ONLINE!", contents="Email config OK")
    print("✅ Email server ready")
except Exception as e:
    print(f"❌ Email config error: {e}")


# ------------------ Startup Event ------------------
@app.on_event("startup")
async def startup_event():
    logger.info("=" * 60)
    logger.info("🚀 Lab Expert API Starting...")
    logger.info(f"🌐 Local Network IP: {LOCAL_IP}")
    logger.info(f"📱 Access from phone: http://{LOCAL_IP}:5000")
    logger.info(f"💻 Access from PC: http://localhost:5000")
    logger.info("=" * 60)


# ------------------ Run ------------------
if __name__ == "__main__":
    import uvicorn

    # CRITICAL: Bind to 0.0.0.0 to accept connections from network
    uvicorn.run(
        app,
        host="0.0.0.0",  # Listen on all network interfaces
        port=int(os.getenv("PORT", 5000)),
        log_level="info"
    )
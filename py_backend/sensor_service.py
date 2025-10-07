import aiohttp
import asyncio
import json
from datetime import datetime
import logging
from collections import deque
import numpy as np

logger = logging.getLogger(__name__)

ESP32_IP = "192.168.137.15"
ESP32_BASE_URL = f"http://{ESP32_IP}"
REQUEST_TIMEOUT = 10

# ESP32 buffer capacity
MAX_ESP32_SAMPLES = 2000


class ESP32ConnectionError(Exception):
    pass


class PhysicsDataProcessor:
    """Process raw sensor data and compute physics quantities"""

    def __init__(self, smoothing_window=5):
        self.smoothing_window = smoothing_window
        self.raw_buffer = deque(maxlen=smoothing_window)
        self.last_displacement = None
        self.last_velocity = None
        self.last_timestamp = None

    def reset(self):
        """Reset processor state"""
        self.raw_buffer.clear()
        self.last_displacement = None
        self.last_velocity = None
        self.last_timestamp = None

    def smooth_displacement(self, displacement):
        """Apply moving average smoothing to reduce sensor noise"""
        self.raw_buffer.append(displacement)
        if len(self.raw_buffer) >= 3:
            return sum(self.raw_buffer) / len(self.raw_buffer)
        return displacement

    def process_reading(self, distance_mm, timestamp_ms):
        """
        Process raw sensor reading and compute physics quantities
        """
        # Filter out error readings
        if distance_mm == 65535:
            logger.warning(f"Sensor error reading at {timestamp_ms}ms - using last valid value")
            if self.last_displacement is not None:
                distance_mm = self.last_displacement * 1000
            else:
                return None

        # Convert to SI units
        timestamp_s = timestamp_ms / 1000.0
        displacement_m = distance_mm / 1000.0

        # Apply smoothing
        smoothed_displacement = self.smooth_displacement(displacement_m)

        velocity_ms = 0.0
        acceleration_ms2 = 0.0

        if self.last_timestamp is not None and self.last_displacement is not None:
            delta_t = timestamp_s - self.last_timestamp

            if delta_t > 0:
                velocity_ms = (smoothed_displacement - self.last_displacement) / delta_t

                if self.last_velocity is not None:
                    acceleration_ms2 = (velocity_ms - self.last_velocity) / delta_t

        self.last_displacement = smoothed_displacement
        self.last_velocity = velocity_ms
        self.last_timestamp = timestamp_s

        return {
            "time": round(timestamp_s, 3),
            "displacement": round(smoothed_displacement, 4),
            "velocity": round(velocity_ms, 3),
            "acceleration": round(acceleration_ms2, 3),
            "raw_distance_mm": distance_mm,
            "sample_quality": "good" if distance_mm != 65535 else "interpolated"
        }


physics_processor = PhysicsDataProcessor(smoothing_window=5)


async def check_esp32_connection():
    """Check if ESP32 is reachable and get device info"""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(f"{ESP32_BASE_URL}/status",
                                   timeout=aiohttp.ClientTimeout(total=5)) as response:
                if response.status == 200:
                    data = await response.json()
                    return {
                        "connected": True,
                        "device_info": data,
                        "ready": data.get("ready", True),
                        "max_samples": data.get("max_samples", MAX_ESP32_SAMPLES)
                    }
                return {"connected": False, "ready": False}
    except Exception as e:
        logger.error(f"ESP32 connection check failed: {e}")
        return {"connected": False, "ready": False, "error": str(e)}


async def get_device_id():
    """Get device ID from ESP32"""
    try:
        logger.info(f"Attempting to get device ID from {ESP32_BASE_URL}/id")
        async with aiohttp.ClientSession() as session:
            async with session.get(f"{ESP32_BASE_URL}/id",
                                   timeout=aiohttp.ClientTimeout(total=5)) as response:
                logger.info(f"Response status: {response.status}")
                if response.status == 200:
                    data = await response.json()
                    device_id = data.get("id")
                    logger.info(f"Device ID received: {device_id}")
                    return device_id
                else:
                    logger.error(f"Bad status code: {response.status}")
                    return None
    except Exception as e:
        logger.error(f"Failed to get device ID: {e}", exc_info=True)
        return None


async def configure_experiment(frequency: int, duration: int, mode: str = "distance"):
    """Send experiment configuration to ESP32 with validation"""
    try:
        # Validate configuration before sending
        required_samples = frequency * duration
        if required_samples > MAX_ESP32_SAMPLES:
            error_msg = (f"Configuration exceeds ESP32 buffer capacity. "
                         f"Max samples: {MAX_ESP32_SAMPLES}, "
                         f"Required: {required_samples}. "
                         f"Reduce frequency or duration.")
            logger.error(error_msg)
            return {"success": False, "error": error_msg}

        config = {
            "frequency": frequency,
            "duration": duration,
            "mode": mode
        }

        physics_processor.reset()

        async with aiohttp.ClientSession() as session:
            async with session.post(
                    f"{ESP32_BASE_URL}/configure",
                    json=config,
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    logger.info(f"Experiment configured: {config} (requires {required_samples} samples)")
                    return {
                        "success": True,
                        "config": config,
                        "required_samples": required_samples,
                        "max_samples": MAX_ESP32_SAMPLES
                    }
                elif response.status == 400:
                    error_data = await response.json()
                    logger.error(f"Configuration rejected by ESP32: {error_data}")
                    return {"success": False, "error": error_data.get("error", "Configuration rejected")}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to configure experiment: {e}")
        return {"success": False, "error": str(e)}


async def start_experiment():
    """Start data collection on ESP32"""
    try:
        physics_processor.reset()

        async with aiohttp.ClientSession() as session:
            async with session.get(
                    f"{ESP32_BASE_URL}/start",
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    logger.info("Experiment started successfully")
                    return {"success": True}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to start experiment: {e}")
        return {"success": False, "error": str(e)}


async def stop_experiment():
    """Stop data collection on ESP32"""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(
                    f"{ESP32_BASE_URL}/stop",
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    logger.info("Experiment stopped successfully")
                    return {"success": True}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to stop experiment: {e}")
        return {"success": False, "error": str(e)}


async def live_distance_generator():
    """SSE generator for live sensor data with physics processing"""
    try:
        connector = aiohttp.TCPConnector(force_close=False, limit=1)
        async with aiohttp.ClientSession(connector=connector) as session:
            logger.info(f"Connecting to ESP32 SSE stream at {ESP32_BASE_URL}/stream")

            async with session.get(
                    f"{ESP32_BASE_URL}/stream",
                    timeout=aiohttp.ClientTimeout(total=0, sock_read=300)
            ) as response:
                if response.status != 200:
                    logger.error(f"ESP32 stream returned status {response.status}")
                    yield {
                        "event": "error",
                        "data": json.dumps({"error": f"ESP32 returned status {response.status}"})
                    }
                    return

                logger.info("Connected to ESP32 SSE stream - Physics processor active")

                buffer = ""

                async for chunk in response.content.iter_any():
                    try:
                        text = chunk.decode('utf-8')
                        buffer += text

                        while '\n\n' in buffer:
                            message, buffer = buffer.split('\n\n', 1)

                            for line in message.split('\n'):
                                line = line.strip()

                                if not line or line.startswith(':'):
                                    continue

                                if line.startswith('data: '):
                                    data_str = line[6:]

                                    try:
                                        raw_data = json.loads(data_str)

                                        if "distance" not in raw_data:
                                            continue

                                        processed_data = physics_processor.process_reading(
                                            distance_mm=raw_data["distance"],
                                            timestamp_ms=raw_data["timestamp"]
                                        )

                                        if processed_data is None:
                                            continue

                                        processed_data["sample"] = raw_data.get("sample", 0)

                                        logger.info(
                                            f"Sample {processed_data['sample']}: "
                                            f"t={processed_data['time']}s, "
                                            f"s={processed_data['displacement']}m, "
                                            f"v={processed_data['velocity']}m/s, "
                                            f"a={processed_data['acceleration']}m/s² "
                                            f"[{processed_data['sample_quality']}]"
                                        )

                                        yield {
                                            "event": "message",
                                            "data": json.dumps(processed_data)
                                        }

                                    except json.JSONDecodeError as e:
                                        logger.error(f"Invalid JSON from ESP32: {data_str} - {e}")

                        if len(buffer) > 10000:
                            logger.warning("Buffer overflow, clearing")
                            buffer = ""

                    except UnicodeDecodeError as e:
                        logger.error(f"Unicode decode error: {e}")
                        continue
                    except Exception as e:
                        logger.error(f"Error processing chunk: {e}", exc_info=True)
                        continue

    except asyncio.CancelledError:
        logger.info("SSE stream cancelled by client")
    except asyncio.TimeoutError:
        logger.error("ESP32 stream timeout")
        yield {
            "event": "error",
            "data": json.dumps({"error": "Stream timeout"})
        }
    except Exception as e:
        logger.error(f"Stream error: {e}", exc_info=True)
        yield {
            "event": "error",
            "data": json.dumps({"error": str(e)})
        }


async def collect_displacement():
    """Collect displacement data from ESP32"""
    try:
        start_result = await start_experiment()
        if not start_result["success"]:
            return {"success": False, "error": "Failed to start collection"}

        max_polls = 120
        poll_count = 0
        async with aiohttp.ClientSession() as session:
            while poll_count < max_polls:
                async with session.get(
                        f"{ESP32_BASE_URL}/status",
                        timeout=aiohttp.ClientTimeout(total=5)
                ) as response:
                    if response.status == 200:
                        status = await response.json()
                        if status.get("ready"):
                            break
                await asyncio.sleep(0.5)
                poll_count += 1

            async with session.get(
                    f"{ESP32_BASE_URL}/data",
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    data = await response.json()
                    return {"success": True, "data": data}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to collect displacement: {e}")
        return {"success": False, "error": str(e)}


async def collect_oscillations(n: int = 3):
    """Collect oscillation timing data"""
    results = []
    try:
        async with aiohttp.ClientSession() as session:
            for i in range(n):
                async with session.get(
                        f"{ESP32_BASE_URL}/start_oscillation",
                        timeout=aiohttp.ClientTimeout(total=5)
                ) as response:
                    if response.status != 200:
                        results.append({"error": f"Failed to start oscillation {i + 1}"})
                        continue

                await asyncio.sleep(3)

                async with session.get(
                        f"{ESP32_BASE_URL}/oscillation_data",
                        timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
                ) as response:
                    if response.status == 200:
                        data = await response.json()
                        results.append({"set": i + 1, "data": data})
                    else:
                        results.append({"error": f"Failed to get oscillation {i + 1} data"})

        return {"success": True, "results": results}
    except Exception as e:
        logger.error(f"Failed to collect oscillations: {e}")
        return {"success": False, "error": str(e)}


async def upload_firmware(bin_path):
    """Upload firmware to ESP32 via OTA"""
    try:
        if not bin_path.exists():
            logger.error(f"Firmware file not found: {bin_path}")
            return False

        file_size = bin_path.stat().st_size
        logger.info(f"Uploading firmware: {bin_path.name} ({file_size:,} bytes)")

        if file_size > 1500000:
            logger.warning(f"Firmware size is large: {file_size:,} bytes - may not fit in partition")

        async with aiohttp.ClientSession() as session:
            with open(bin_path, 'rb') as f:
                data = aiohttp.FormData()
                data.add_field('update', f,
                               filename=bin_path.name,
                               content_type='application/octet-stream')

                logger.info(f"Sending POST to {ESP32_BASE_URL}/update")
                async with session.post(
                        f"{ESP32_BASE_URL}/update",
                        data=data,
                        timeout=aiohttp.ClientTimeout(total=90)
                ) as response:
                    logger.info(f"Upload HTTP status: {response.status}")

                    if response.status == 200:
                        text = await response.text()
                        logger.info(f"ESP32 response: '{text}'")

                        if text.strip() == "OK":
                            logger.info("Upload successful! Waiting for ESP32 to reboot...")
                            await asyncio.sleep(15)
                            return True
                        else:
                            logger.error(f"Upload reported failure: {text}")
                            return False
                    else:
                        logger.error(f"Bad HTTP status: {response.status}")
                        return False

    except asyncio.TimeoutError:
        logger.error("Upload timed out - ESP32 may have crashed during write")
        return False
    except Exception as e:
        logger.error(f"Firmware upload exception: {e}", exc_info=True)
        return False
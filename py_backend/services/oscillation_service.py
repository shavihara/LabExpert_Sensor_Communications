import aiohttp
import asyncio
import json
from datetime import datetime
import logging

logger = logging.getLogger(__name__)

ESP32_IP = "192.168.137.15"
ESP32_BASE_URL = f"http://{ESP32_IP}"
REQUEST_TIMEOUT = 10


async def check_osi_connection():
    """Check if ESP32 OSI sensor is reachable"""
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
                        "current_count": data.get("total_count", 0)
                    }
                return {"connected": False, "ready": False}
    except Exception as e:
        logger.error(f"OSI connection check failed: {e}")
        return {"connected": False, "ready": False, "error": str(e)}


async def configure_osi_experiment(frequency: int, duration: int):
    """Send experiment configuration to OSI sensor"""
    try:
        config = {
            "frequency": frequency,
            "duration": duration,
            "mode": "count"
        }

        async with aiohttp.ClientSession() as session:
            async with session.post(
                    f"{ESP32_BASE_URL}/configure",
                    json=config,
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    logger.info(f"OSI experiment configured: {config}")
                    return {"success": True, "config": config}
                elif response.status == 400:
                    error_data = await response.json()
                    logger.error(f"Configuration rejected: {error_data}")
                    return {"success": False, "error": error_data.get("error", "Configuration rejected")}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to configure OSI experiment: {e}")
        return {"success": False, "error": str(e)}


async def start_osi_experiment():
    """Start OSI data collection"""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(
                    f"{ESP32_BASE_URL}/start",
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    logger.info("OSI experiment started")
                    return {"success": True}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to start OSI experiment: {e}")
        return {"success": False, "error": str(e)}


async def stop_osi_experiment():
    """Stop OSI data collection"""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(
                    f"{ESP32_BASE_URL}/stop",
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    logger.info("OSI experiment stopped")
                    return {"success": True}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to stop OSI experiment: {e}")
        return {"success": False, "error": str(e)}


async def reset_osi_count():
    """Reset OSI counter to zero"""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(
                    f"{ESP32_BASE_URL}/reset",
                    timeout=aiohttp.ClientTimeout(total=5)
            ) as response:
                if response.status == 200:
                    logger.info("OSI counter reset")
                    return {"success": True}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to reset OSI counter: {e}")
        return {"success": False, "error": str(e)}


async def live_oscillation_generator():
    """SSE generator for live oscillation count data"""
    try:
        connector = aiohttp.TCPConnector(force_close=False, limit=1)
        async with aiohttp.ClientSession(connector=connector) as session:
            logger.info(f"Connecting to OSI SSE stream at {ESP32_BASE_URL}/stream")

            async with session.get(
                    f"{ESP32_BASE_URL}/stream",
                    timeout=aiohttp.ClientTimeout(total=0, sock_read=300)
            ) as response:
                if response.status != 200:
                    logger.error(f"OSI stream returned status {response.status}")
                    yield {
                        "event": "error",
                        "data": json.dumps({"error": f"ESP32 returned status {response.status}"})
                    }
                    return

                logger.info("Connected to OSI SSE stream")

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

                                        if "count" not in raw_data:
                                            continue

                                        processed_data = {
                                            "time": round(raw_data["timestamp"] / 1000.0, 3),
                                            "count": raw_data["count"],
                                            "sample": raw_data.get("sample", 0)
                                        }

                                        logger.info(
                                            f"Sample {processed_data['sample']}: "
                                            f"t={processed_data['time']}s, "
                                            f"count={processed_data['count']}"
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
        logger.info("OSI SSE stream cancelled by client")
    except asyncio.TimeoutError:
        logger.error("OSI stream timeout")
        yield {
            "event": "error",
            "data": json.dumps({"error": "Stream timeout"})
        }
    except Exception as e:
        logger.error(f"OSI stream error: {e}", exc_info=True)
        yield {
            "event": "error",
            "data": json.dumps({"error": str(e)})
        }


async def get_osi_data():
    """Get all collected OSI data"""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(
                    f"{ESP32_BASE_URL}/data",
                    timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT)
            ) as response:
                if response.status == 200:
                    data = await response.json()
                    return {"success": True, "data": data}
                return {"success": False, "error": f"Status {response.status}"}
    except Exception as e:
        logger.error(f"Failed to get OSI data: {e}")
        return {"success": False, "error": str(e)}
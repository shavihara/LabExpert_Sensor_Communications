import asyncio
import subprocess
import sys
from bleak import BleakScanner, BleakClient

NUS_SVC = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

async def ensure_bt_on():
    try:
        from winsdk.windows.devices.radios import Radio, RadioState
    except Exception:
        try:
            from winrt.windows.devices.radios import Radio, RadioState
        except Exception:
            try:
                subprocess.run(["powershell", "Start-Process", "ms-settings:bluetooth"], check=False)
            except Exception:
                pass
            input("Enable Bluetooth then press Enter: ")
            return
    radios = await Radio.get_radios_async()
    for r in radios:
        if getattr(r.kind, "name", "") == "Bluetooth":
            try:
                await r.set_state_async(RadioState.ON)
            except Exception:
                try:
                    subprocess.run(["powershell", "Start-Process", "ms-settings:bluetooth"], check=False)
                except Exception:
                    pass
                input("Enable Bluetooth then press Enter: ")

def get_current_ssid():
    try:
        out = subprocess.check_output(["netsh", "wlan", "show", "interfaces"], text=True, encoding="utf-8", errors="ignore")
        for line in out.splitlines():
            if "SSID" in line and "BSSID" not in line:
                parts = line.split(":", 1)
                if len(parts) == 2:
                    return parts[1].strip()
    except Exception:
        return None
    return None

def get_password_for_ssid(ssid):
    try:
        out = subprocess.check_output(["netsh", "wlan", "show", "profile", f"name={ssid}", "key=clear"], text=True, encoding="utf-8", errors="ignore")
        for line in out.splitlines():
            if "Key Content" in line:
                parts = line.split(":", 1)
                if len(parts) == 2:
                    return parts[1].strip()
    except Exception:
        return None
    return None

async def scan_all():
    for _ in range(3):
        try:
            devices = await BleakScanner.discover(timeout=6)
            for i, d in enumerate(devices):
                print(i, d.name or "<unknown>", d.address)
            idx = int(input("Select device index: "))
            return devices[idx]
        except OSError as e:
            s = str(e).lower()
            if "device is not ready" in s or "winerror" in s:
                try:
                    subprocess.run(["powershell", "Start-Process", "ms-settings:bluetooth"], check=False)
                except Exception:
                    pass
                input("Enable Bluetooth then press Enter: ")
            else:
                raise
    print("Scan failed")
    sys.exit(1)

async def provision(address, mode, ssid, password):
    echo_ssid = None
    echo_pass = None
    status = None
    done = asyncio.Event()
    def on_notify(_, data):
        s = data.decode(errors="ignore")
        print(s)
        nonlocal echo_ssid, echo_pass, status
        if s.startswith("ECHO:"):
            body = s.split(":",1)[1]
            parts = body.split(";")
            kv = {}
            for p in parts:
                if "=" in p:
                    k,v = p.split("=",1)
                    kv[k]=v
            echo_ssid = kv.get("SSID")
            echo_pass = kv.get("PASS")
        if s.startswith("STATUS:"):
            status = s
            if s.startswith("STATUS:SUCCESS"):
                done.set()
    async with BleakClient(address) as client:
        await client.start_notify(NUS_TX, on_notify)
        await client.write_gatt_char(NUS_RX, b"HELLO")
        await client.write_gatt_char(NUS_RX, f"MODE:{mode}".encode())
        await client.write_gatt_char(NUS_RX, f"SSID:{ssid}".encode())
        await client.write_gatt_char(NUS_RX, f"PASS:{password}".encode())
        for _ in range(20):
            await asyncio.sleep(0.2)
            if echo_ssid == ssid and echo_pass == password:
                break
        if echo_ssid != ssid or echo_pass != password:
            print("Echo mismatch")
            return
        await client.write_gatt_char(NUS_RX, b"CONFIRM")
        await asyncio.wait_for(done.wait(), timeout=30)

async def main():
    await ensure_bt_on()
    dev = await scan_all()
    name = dev.name or ""
    print("Selected:", name, dev.address)
    mode = input("auto/manual: ").strip().lower()
    if mode == "auto":
        ssid = get_current_ssid()
        if not ssid:
            print("No SSID detected")
            sys.exit(1)
        pwd = get_password_for_ssid(ssid)
        if not pwd:
            print("Password unavailable; enter manually")
            pwd = input("Password: ")
    else:
        ssid = input("SSID: ")
        pwd = input("Password: ")
    await provision(dev.address, mode, ssid, pwd)

if __name__ == "__main__":
    asyncio.run(main())
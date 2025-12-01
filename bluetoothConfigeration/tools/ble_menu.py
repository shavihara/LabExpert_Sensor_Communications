#!/usr/bin/env python3
"""
Compact Bluetooth BLE Menu System
Supports scanning, connecting, service discovery, and automation.
"""
import asyncio
import logging
from bleak import BleakScanner, BleakClient
from datetime import datetime
from typing import List, Callable, Optional

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class BluetoothMenu:
    def __init__(self):
        self.devices = []
        self.client: Optional[BleakClient] = None
        self.running = True
        
    async def scan_devices(self):
        """Scan for BLE devices"""
        logger.info("Scanning for BLE devices...")
        self.devices = await BleakScanner.discover(timeout=5)
        for i, d in enumerate(self.devices):
            print(f"{i}: {d.name or 'Unknown'} - {d.address}")
    
    async def connect_device(self, index: int):
        """Connect to selected device"""
        if not self.devices or index >= len(self.devices):
            print("Invalid device selection")
            return
        
        device = self.devices[index]
        try:
            self.client = BleakClient(device.address)
            await self.client.connect()
            logger.info(f"Connected to {device.name or device.address}")
            
            # Discover services (Bleak v21+ API)
            services = self.client.services
            print("Services discovered:")
            for service in services:
                print(f"  {service.uuid}: {service.description}")
                
        except Exception as e:
            logger.error(f"Connection failed: {e}")
    
    async def disconnect(self):
        """Disconnect current device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            logger.info("Disconnected")
    
    async def auto_scan_schedule(self, interval: int = 30):
        """Automated scheduled scanning"""
        logger.info(f"Starting automated scanning every {interval}s")
        while self.running:
            await self.scan_devices()
            await asyncio.sleep(interval)
    
    def show_menu(self):
        """Display main menu"""
        print("\n=== BLE Menu ===")
        print("1. Scan Devices")
        print("2. Connect to Device")
        print("3. Disconnect")
        print("4. Automated Scanning")
        print("5. Exit")
    
    async def run(self):
        """Main menu loop"""
        while self.running:
            self.show_menu()
            choice = input("Select option: ").strip()
            
            try:
                if choice == "1":
                    await self.scan_devices()
                elif choice == "2":
                    idx = int(input("Device index: "))
                    await self.connect_device(idx)
                elif choice == "3":
                    await self.disconnect()
                elif choice == "4":
                    interval = int(input("Scan interval (seconds): ") or 30)
                    await self.auto_scan_schedule(interval)
                elif choice == "5":
                    self.running = False
                    await self.disconnect()
                else:
                    print("Invalid option")
            except Exception as e:
                logger.error(f"Operation failed: {e}")

async def main():
    """Main entry point"""
    menu = BluetoothMenu()
    await menu.run()

if __name__ == "__main__":
    # Example usage:
    # python ble_menu.py
    # python -c "import asyncio; from ble_menu import BluetoothMenu; asyncio.run(BluetoothMenu().auto_scan_schedule(10))"
    asyncio.run(main())
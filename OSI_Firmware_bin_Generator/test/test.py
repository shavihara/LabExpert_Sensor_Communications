import paho.mqtt.client as mqtt
import json
import csv
import time
from datetime import datetime
import threading

class OscillationCounterTester:
    def __init__(self, broker_ip="192.168.137.1", broker_port=1883):
        self.broker_ip = broker_ip
        self.broker_port = broker_port
        self.client = mqtt.Client()
        
        # Data storage
        self.oscillation_data = []
        self.experiment_summary = None
        self.is_running = False
        self.start_time = None
        
        # Setup MQTT callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
        # CSV file
        self.csv_filename = f"oscillation_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"Connected to MQTT broker at {self.broker_ip}:{self.broker_port}")
            # Subscribe to all relevant topics
            client.subscribe("sensor/oscillation/data")
            client.subscribe("sensor/oscillation/summary")
            client.subscribe("sensor/status")
            client.subscribe("sensor/type")
            print("Subscribed to topics: sensor/oscillation/data, sensor/oscillation/summary, sensor/status, sensor/type")
        else:
            print(f"Failed to connect, return code {rc}")

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        
        try:
            data = json.loads(payload)
            
            if topic == "sensor/oscillation/data":
                self.handle_oscillation_data(data)
            elif topic == "sensor/oscillation/summary":
                self.handle_summary_data(data)
            elif topic == "sensor/status":
                self.handle_status_data(data)
            elif topic == "sensor/type":
                print(f"Sensor Type: {data}")
                
        except json.JSONDecodeError:
            print(f"Non-JSON message on {topic}: {payload}")

    def handle_oscillation_data(self, data):
        count = data.get('count', 0)
        disconnect_time = data.get('disconnect_time', '00:00:00')
        reconnect_time = data.get('reconnect_time', '00:00:00')
        sensor_id = data.get('sensor_id', 'unknown')
        
        # Store data
        timestamp = datetime.now().strftime('%H:%M:%S')
        self.oscillation_data.append({
            'count': count,
            'disconnect_time': disconnect_time,
            'reconnect_time': reconnect_time,
            'sensor_id': sensor_id,
            'received_timestamp': timestamp
        })
        
        print(f"Oscillation {count}: {disconnect_time} - {reconnect_time} (Sensor: {sensor_id})")
        
        # Save to CSV immediately
        self.save_to_csv()

    def handle_summary_data(self, data):
        self.experiment_summary = data
        total_count = data.get('total_count', 0)
        first_time = data.get('first_oscillation_time', '00:00:00')
        last_time = data.get('last_oscillation_time', '00:00:00')
        status = data.get('status', 'unknown')
        
        print(f"\n=== EXPERIMENT SUMMARY ===")
        print(f"Total Oscillations: {total_count}")
        print(f"First: {first_time}, Last: {last_time}")
        print(f"Status: {status}")
        print(f"==========================\n")
        
        self.is_running = False
        
        # Save summary to CSV
        self.save_summary_to_csv()

    def handle_status_data(self, data):
        status = data.get('status', '')
        sensor_id = data.get('sensor_id', 'unknown')
        
        if status == "experiment_started":
            print(f"Experiment STARTED on sensor {sensor_id}")
            self.is_running = True
            self.start_time = datetime.now()
            # Clear previous data
            self.oscillation_data = []
            
        elif status == "experiment_stopped":
            print(f"Experiment STOPPED on sensor {sensor_id}")
            self.is_running = False
            
        elif status == "experiment_completed":
            print(f"Experiment COMPLETED on sensor {sensor_id}")
            self.is_running = False

    def connect(self):
        try:
            self.client.connect(self.broker_ip, self.broker_port, 60)
            # Start the MQTT loop in a separate thread
            self.client.loop_start()
            time.sleep(2)  # Wait for connection
            return True
        except Exception as e:
            print(f"Connection failed: {e}")
            return False

    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()
        print("Disconnected from MQTT broker")

    def start_experiment(self, count=20):
        if not self.is_running:
            command = {
                "command": "start_oscillation",
                "count": count
            }
            self.client.publish("sensor/commands", json.dumps(command))
            print(f"Sent start command for {count} oscillations")
            return True
        else:
            print("Experiment is already running")
            return False

    def stop_experiment(self):
        command = {
            "command": "stop_oscillation"
        }
        self.client.publish("sensor/commands", json.dumps(command))
        print("Sent stop command")

    def get_status(self):
        command = {
            "command": "status"
        }
        self.client.publish("sensor/commands", json.dumps(command))
        print("Sent status request")

    def save_to_csv(self):
        try:
            with open(self.csv_filename, 'w', newline='') as csvfile:
                fieldnames = ['count', 'disconnect_time', 'reconnect_time', 'sensor_id', 'received_timestamp']
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                
                writer.writeheader()
                for data in self.oscillation_data:
                    writer.writerow(data)
                    
            print(f"Data saved to {self.csv_filename}")
        except Exception as e:
            print(f"Error saving to CSV: {e}")

    def save_summary_to_csv(self):
        if self.experiment_summary:
            summary_filename = f"experiment_summary_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
            try:
                with open(summary_filename, 'w', newline='') as csvfile:
                    fieldnames = ['total_count', 'first_oscillation_time', 'last_oscillation_time', 
                                 'status', 'sensor_id', 'experiment_duration']
                    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                    
                    writer.writeheader()
                    writer.writerow(self.experiment_summary)
                    
                print(f"Summary saved to {summary_filename}")
            except Exception as e:
                print(f"Error saving summary to CSV: {e}")

    def wait_for_completion(self, timeout=300):
        """Wait for experiment to complete with timeout"""
        start_wait = time.time()
        while self.is_running and (time.time() - start_wait) < timeout:
            time.sleep(1)
            
        if self.is_running:
            print("Timeout reached, experiment still running")
            return False
        else:
            print("Experiment completed")
            return True

def main():
    tester = OscillationCounterTester()
    
    if not tester.connect():
        return
    
    try:
        while True:
            print("\n=== Oscillation Counter Tester ===")
            print("1. Start Experiment")
            print("2. Stop Experiment")
            print("3. Get Status")
            print("4. View Current Data")
            print("5. Export Data to CSV")
            print("6. Exit")
            
            choice = input("Select option (1-6): ").strip()
            
            if choice == '1':
                try:
                    count = int(input("Enter number of oscillations (default 20): ") or "20")
                    if tester.start_experiment(count):
                        print(f"Waiting for {count} oscillations...")
                        # Wait for completion
                        tester.wait_for_completion()
                except ValueError:
                    print("Invalid number")
                    
            elif choice == '2':
                tester.stop_experiment()
                
            elif choice == '3':
                tester.get_status()
                time.sleep(1)  # Wait for status response
                
            elif choice == '4':
                print(f"\nCurrent Data ({len(tester.oscillation_data)} records):")
                for data in tester.oscillation_data:
                    print(f"  {data['count']}: {data['disconnect_time']} - {data['reconnect_time']}")
                    
            elif choice == '5':
                tester.save_to_csv()
                if tester.experiment_summary:
                    tester.save_summary_to_csv()
                    
            elif choice == '6':
                break
                
            else:
                print("Invalid option")
                
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        tester.disconnect()

# Alternative: Automated test function
def automated_test(oscillation_count=10):
    """Run an automated test with the specified oscillation count"""
    tester = OscillationCounterTester()
    
    if not tester.connect():
        return
    
    print(f"Starting automated test with {oscillation_count} oscillations...")
    
    if tester.start_experiment(oscillation_count):
        # Wait for completion
        if tester.wait_for_completion():
            print("Test completed successfully!")
            
            # Save all data
            tester.save_to_csv()
            if tester.experiment_summary:
                tester.save_summary_to_csv()
                
            # Print results
            print(f"\nTest Results:")
            print(f"Total oscillations recorded: {len(tester.oscillation_data)}")
            if tester.experiment_summary:
                print(f"Summary: {tester.experiment_summary}")
        else:
            print("Test timed out or failed")
    
    tester.disconnect()

if __name__ == "__main__":
    # You can run either the interactive mode or automated test
    
    # Option 1: Interactive mode
    main()
    
    # Option 2: Automated test (uncomment below)
    # automated_test(oscillation_count=10)
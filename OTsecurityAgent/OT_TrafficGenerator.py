import socket
import time
import sys
from abc import ABC, abstractmethod
from scapy.all import IP, TCP, send

# --- Configuration ---
INJECTION_IP = "127.0.0.1"
MODBUS_INJECTION_PORT = 5020
HMI_INJECTION_PORT = 8080
SLEEP_SECONDS = 1
ATTACK_TRIGGER_INTERVAL = 10

# --- Logging Configuration ---
EVENT_DELIMITER = "------------------------------------------------"
APP_ID = "OT-AGENT-DEMO"
MODBUS_LOG_PORT = 5020
MODBUS_LOG_DST = f"{INJECTION_IP}:{MODBUS_LOG_PORT}"
HMI_LOG_PORT = 8080
HMI_LOG_DST = f"{INJECTION_IP}:{HMI_LOG_PORT}"

# --- Utility Functions ---
def bytes_to_hex_string(data: bytes) -> str:
    return ' '.join(f'{b:02x}' for b in data)

def log_to_stdout(protocol, type_name, src, dst, payload_hex):
    """
    Standardized logging to stdout for the Orchestrator to consume.
    
    """
    timestamp = int(time.time())
    print(EVENT_DELIMITER)
    print(f'"module": "INGESTION_AGENT",')
    print(f'"app_id": "{APP_ID}",')
    print(f'"timestamp": "{timestamp}",')
    print(f'"protocol": "{protocol}",')
    print(f'"type": "{type_name}",')
    print(f'"src": "{src}",')
    print(f'"dst": "{dst}",')
    print(f'"payload_snippet": "{payload_hex}"')
    print(EVENT_DELIMITER)
    sys.stdout.flush()

# --- Base Class ---

class BaseTrafficGenerator(ABC):
    def __init__(self, protocol, type_name, dst_port, dst_ip=INJECTION_IP):
        self.protocol = protocol
        self.type_name = type_name
        self.dst_port = dst_port
        self.dst_ip = dst_ip

    @abstractmethod
    def generate(self, loop_counter: int):
        pass

    def execute_request(self, loop_counter: int, payload: bytes):
        """Uses a real TCP socket to ensure the server receives the data."""
        src_port = 0 # OS will assign a random high port
        try:
            # 1. Create a real TCP connection (performs the handshake)
            with socket.create_connection((self.dst_ip, self.dst_port), timeout=1) as sock:
                src_port = sock.getsockname()[1]
                
                # 2. Send the Modbus/HTTP payload
                sock.sendall(payload)
                
                # 3. Optional: Receive response (good for verifying logic)
                # response = sock.recv(1024) 
                
            # 4. Log the event after successful transmission
            log_to_stdout(
                self.protocol, 
                self.type_name, 
                f"{self.dst_ip}:{src_port}", 
                f"{self.dst_ip}:{self.dst_port}", 
                bytes_to_hex_string(payload)
            )
        except Exception as e:
            print(f"[ERROR] Connection to {self.dst_port} failed: {e}")

# --- Revised Subclasses (Removing Scapy logic) ---

class ModbusReadGenerator(BaseTrafficGenerator):
    def __init__(self):
        super().__init__("Modbus/TCP", "Read Holding Registers (0x03)", MODBUS_INJECTION_PORT)

    def generate(self, loop_counter: int):
        payload = bytes.fromhex("00010000000601030000000a")
        self.execute_request(loop_counter, payload)

class ModbusControlGenerator(BaseTrafficGenerator):
    def __init__(self):
        super().__init__("Modbus/TCP", "CONTROL ACTION: Force Single Coil (0x05)", MODBUS_INJECTION_PORT)

    def generate(self, loop_counter: int):
        payload = bytes.fromhex("00020000000601050005ff00")
        self.execute_request(loop_counter, payload)

class HttpAttackGenerator(BaseTrafficGenerator):
    def __init__(self):
        super().__init__("HTTP-HMI", "ATTACK: Potential SQL Injection", HMI_INJECTION_PORT)

    def generate(self, loop_counter: int):
        malicious_query = (
            "GET /api/tags?id=105 OR 1=1 -- HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n\r\n"
        )
        self.execute_request(loop_counter, malicious_query.encode('ascii'))

# --- Revised Main Execution ---

def main():
    print("[*] Socket-Based Traffic Generator started.")
    gen_normal = ModbusReadGenerator()
    gen_control = ModbusControlGenerator()
    gen_attack = HttpAttackGenerator()

    loop_counter = 0
    try:
        while True:
            loop_counter += 1
            
            # 1. Normal Traffic
            gen_normal.generate(loop_counter)

            # 2. Control Action every 5 cycles
            if loop_counter % 5 == 0:
                gen_control.generate(loop_counter)

            # 3. Web Attack every 10 cycles
            if loop_counter % ATTACK_TRIGGER_INTERVAL == 0:
                gen_attack.generate(loop_counter)

            time.sleep(SLEEP_SECONDS)
    except KeyboardInterrupt:
        sys.exit(0)

if __name__ == "__main__":
    main()
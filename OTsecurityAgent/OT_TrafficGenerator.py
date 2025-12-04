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
    """
    Abstract Base Class that handles the common logic for packet creation
    and data formatting.
    """
    def __init__(self, protocol, type_name, dst_port, dst_ip=INJECTION_IP):
        self.protocol = protocol
        self.type_name = type_name
        self.dst_port = dst_port
        self.dst_ip = dst_ip

    def _build_scapy_packet(self, payload: bytes, src_port: int, seq: int):
        """Constructs the IP/TCP layers wrapper."""
        ip_layer = IP(src=self.dst_ip, dst=self.dst_ip)
        tcp_layer = TCP(sport=src_port, dport=self.dst_port, flags="PA", seq=seq, ack=seq)
        return ip_layer / tcp_layer / payload

    @abstractmethod
    def generate(self, loop_counter: int):
        """
        Subclasses must implement this to return the specific payload and 
        calculated source port.
        """
        pass

    def create_event(self, loop_counter: int, payload: bytes, src_port: int):
        """
        Helper method to finalize the packet and log data.
        Returns: (scapy_packet, log_metadata_dict)
        """
        # 1. Create Scapy Packet
        packet = self._build_scapy_packet(payload, src_port, loop_counter)

        # 2. Prepare Log Data
        payload_hex = bytes_to_hex_string(payload)
        src_str = f"{self.dst_ip}:{src_port}"
        dst_str = f"{self.dst_ip}:{self.dst_port}"

        log_data = {
            "protocol": self.protocol,
            "type_name": self.type_name,
            "src": src_str,
            "dst": dst_str,
            "payload_hex": payload_hex
        }

        return packet, log_data

# --- Subclasses ---

class ModbusReadGenerator(BaseTrafficGenerator):
    def __init__(self):
        super().__init__(protocol="Modbus/TCP", 
                         type_name="Read Holding Registers (0x03)", 
                         dst_port=MODBUS_INJECTION_PORT)

    def generate(self, loop_counter: int):
        # Fixed source port for normal traffic
        src_port = 49152 
        # Modbus PDU: Transaction ID 1, Protocol 0, Len 6, Unit 1, Func 3, Start 0, Count 10
        payload = bytes.fromhex("00010000000601030000000a")
        
        return self.create_event(loop_counter, payload, src_port)


class ModbusControlGenerator(BaseTrafficGenerator):
    def __init__(self):
        super().__init__(protocol="Modbus/TCP", 
                         type_name="CONTROL ACTION: Force Single Coil (0x05)", 
                         dst_port=MODBUS_INJECTION_PORT)

    def generate(self, loop_counter: int):
        # Different source port for control actions
        src_port = 49153
        # Modbus PDU: Func 0x05 (Write Coil)
        payload = bytes.fromhex("00020000000601050005ff00")
        
        return self.create_event(loop_counter, payload, src_port)


class HttpAttackGenerator(BaseTrafficGenerator):
    def __init__(self):
        super().__init__(protocol="HTTP-HMI", 
                         type_name="ATTACK: Potential SQL Injection", 
                         dst_port=HMI_INJECTION_PORT)

    def generate(self, loop_counter: int):
        # Dynamic source port calculation
        src_port = 60210 + (loop_counter % 100)
        
        malicious_query = (
            "GET /api/tags?id=105 OR 1=1 -- HTTP/1.1\r\n"
            "Host: 192.168.1.10\r\n"
            "User-Agent: Attacker-Tool\r\n"
            "\r\n"
        )
        payload = malicious_query.encode('ascii')
        
        return self.create_event(loop_counter, payload, src_port)

# --- Main Execution ---

def main():
    print("[*] OOP Traffic Generator started.")
    print(f"[*] Target IP: {INJECTION_IP}")
    print(EVENT_DELIMITER)

    # Instantiate Generators
    gen_normal = ModbusReadGenerator()
    gen_control = ModbusControlGenerator()
    gen_attack = HttpAttackGenerator()

    loop_counter = 0

    try:
        while True:
            loop_counter += 1
            events_to_process = []

            # --- Scheduling Logic ---
            
            # 1. Always run normal traffic
            events_to_process.append(gen_normal.generate(loop_counter))

            # 2. Run Control Action every 5 cycles
            if loop_counter % 5 == 0:
                events_to_process.append(gen_control.generate(loop_counter))

            # 3. Run Web Attack every ATTACK_TRIGGER_INTERVAL cycles
            if loop_counter % ATTACK_TRIGGER_INTERVAL == 0:
                events_to_process.append(gen_attack.generate(loop_counter))

            # --- Execution Loop ---
            for packet, log_data in events_to_process:
                try:
                    # Send via Scapy
                    send(packet, verbose=0)
                    
                    # Log via Helper
                    log_to_stdout(
                        log_data['protocol'],
                        log_data['type_name'],
                        log_data['src'],
                        log_data['dst'],
                        log_data['payload_hex']
                    )
                    time.sleep(0.05)
                except Exception as e:
                    print(f"[ERROR] Failed to send/log packet: {e}")

            time.sleep(SLEEP_SECONDS)

    except KeyboardInterrupt:
        print("\n[*] Generator stopped.")
        sys.exit(0)

if __name__ == "__main__":
    main()
import time
import random
import sys
from scapy.all import IP, TCP, send 

# --- Configuration for Network Injection ---
# Target IP for actual packet injection (must be 127.0.0.1 for sniffer visibility)
INJECTION_IP = "127.0.0.1"
MODBUS_INJECTION_PORT = 5020
# --- FIX: Changing the HMI port from 80 to 8080 (a high port) to ensure visibility on loopback ---
HMI_INJECTION_PORT = 8080 # Simulated HMI Web Server on a non-privileged port

SLEEP_SECONDS = 1
ATTACK_TRIGGER_INTERVAL = 10 # Trigger a special attack every 10 cycles

# --- Configuration for Logging Output ---
EVENT_DELIMITER = "------------------------------------------------"
APP_ID = "OT-AGENT-DEMO"
MODBUS_LOG_PORT = 5020
MODBUS_LOG_DST = f"{INJECTION_IP}:{MODBUS_LOG_PORT}"
# --- FIX: Updating the HMI log destination to reflect the 8080 port ---
HMI_LOG_PORT = 8080
HMI_LOG_DST = f"{INJECTION_IP}:{HMI_LOG_PORT}"

# --- Utility Functions ---

def bytes_to_hex_string(data: bytes) -> str:
    """
    Converts a bytes object to a hexadecimal string representation,
    with spaces between bytes.
    Example: b'\x01\x03' -> '01 03'
    """
    return ' '.join(f'{b:02x}' for b in data)

def log_event(protocol: str, type_name: str, src: str, dst: str, payload_hex: str):
    """
    Generates a structured event block for logging to stdout, matching the
    required format.
    """
    timestamp = int(time.time())

    # Print to stdout, which will be piped to the Orchestrator
    print(EVENT_DELIMITER)
    print(f'"module": "INGESTION_AGENT",')
    print(f'"app_id": "{APP_ID}",')
    print(f'"timestamp": "{timestamp}",')
    print(f'"protocol": "{protocol}",')
    print(f'"type": "{type_name}",')
    print(f'"src": "{src}",')
    print(f'"dst": "{dst}",')
    print(f'"payload_snippet": "{payload_hex}"')  # This is the hex string representation of the payload
    print(EVENT_DELIMITER)
    
    # Crucial for immediate processing by the parent orchestrator process
    sys.stdout.flush()

# --- Traffic Creation Scenarios ---

def generate_normal_traffic(loop_counter: int):
    """
    Simulates normal Modbus read traffic (FC 0x03).
    
    Returns: (scapy_packet, log_protocol, log_type, log_src, log_dst, log_payload_hex)
    """
    
    # --- Logging Parameters ---
    type_name = "Read Holding Registers (0x03)"
    log_src = "127.0.0.1:49152"
    log_dst = MODBUS_LOG_DST
    log_protocol = "Modbus/TCP"

    # Modbus payload (01 03 00 00 00 0a)
    modbus_pdu = bytes.fromhex("00010000000601030000000a")
    log_payload_hex = bytes_to_hex_string(modbus_pdu)
    
    # --- Scapy Packet (Actual Injection) ---
    ip_layer = IP(src=INJECTION_IP, dst=INJECTION_IP)
    tcp_layer = TCP(sport=49152, dport=MODBUS_INJECTION_PORT, flags="PA", seq=loop_counter, ack=loop_counter)
    scapy_packet = ip_layer / tcp_layer / modbus_pdu
    
    return scapy_packet, log_protocol, type_name, log_src, log_dst, log_payload_hex

def generate_control_action_traffic(loop_counter: int):
    """
    Simulates a critical control action (Modbus write command).
    """
    
    # --- Logging Parameters ---
    type_name = "CONTROL ACTION: Force Single Coil (0x05)"
    log_src = "127.0.0.1:49153"
    log_dst = MODBUS_LOG_DST
    log_protocol = "Modbus/TCP"

    # Modbus Write Single Coil (0x05) payload
    modbus_pdu = bytes.fromhex("00020000000601050005ff00")
    log_payload_hex = bytes_to_hex_string(modbus_pdu)
    
    # --- Scapy Packet (Actual Injection) ---
    ip_layer = IP(src=INJECTION_IP, dst=INJECTION_IP)
    tcp_layer = TCP(sport=49153, dport=MODBUS_INJECTION_PORT, flags="PA", seq=loop_counter, ack=loop_counter)
    scapy_packet = ip_layer / tcp_layer / modbus_pdu
    
    return scapy_packet, log_protocol, type_name, log_src, log_dst, log_payload_hex

def generate_web_attack_traffic(loop_counter: int):
    """
    Simulates a HTTP request containing a potential SQL injection payload.
    """
    
    # --- Logging Parameters ---
    type_name = "ATTACK: Potential SQL Injection"
    log_protocol = "HTTP-HMI"
    # Log the traffic targeting the new high port
    log_src = f"{INJECTION_IP}:60210" # Use a dynamic source port based on injection
    log_dst = HMI_LOG_DST # Use the high port 8080 in the log

    # HTTP GET payload (simulated SQL Injection)
    malicious_query = (
        "GET /api/tags?id=105 OR 1=1 -- HTTP/1.1\r\n"
        "Host: 192.168.1.10\r\n" # This is fine as it's part of the payload content
        "User-Agent: Attacker-Tool\r\n"
        "\r\n"
    )
    http_payload = malicious_query.encode('ascii')
    log_payload_hex = bytes_to_hex_string(http_payload)

    # --- Scapy Packet (Actual Injection - FIX: Use Port 8080) ---
    src_port = 60210 + (loop_counter % 100) # Ephemeral port for injection
    ip_layer = IP(src=INJECTION_IP, dst=INJECTION_IP)
    # Target HMI_INJECTION_PORT (8080)
    tcp_layer = TCP(sport=src_port, dport=HMI_INJECTION_PORT, flags="PA", seq=loop_counter, ack=loop_counter)
    scapy_packet = ip_layer / tcp_layer / http_payload
    
    return scapy_packet, log_protocol, type_name, f"{INJECTION_IP}:{src_port}", HMI_LOG_DST, log_payload_hex

# --- Main Loop ---

def main():
    print("[*] Python Traffic Generator started.")
    print("[*] Packets sent via Layer 3 (send) to 127.0.0.1 for C++ sniffer.")
    print(f"[*] HMI Traffic now targets Port {HMI_INJECTION_PORT} (high port) for visibility.")
    print(f"[*] Structured ground truth logs printed to stdout every {SLEEP_SECONDS}s.")
    print("-" * 50)
    
    loop_counter = 0
    
    try:
        while True:
            loop_counter += 1
            
            traffic_events = []

            # 1. Always generate normal traffic
            traffic_events.append(generate_normal_traffic(loop_counter))

            # 2. Introduce anomalies on a schedule
            if loop_counter % 5 == 0:
                 traffic_events.append(generate_control_action_traffic(loop_counter))
            
            if loop_counter % ATTACK_TRIGGER_INTERVAL == 0:
                traffic_events.append(generate_web_attack_traffic(loop_counter))
                
            # Process all prepared events for this cycle
            for scapy_packet, log_protocol, log_type, log_src, log_dst, log_payload_hex in traffic_events:
                try:
                    # 1. Send the packet over the network
                    send(scapy_packet, verbose=0) 
                    
                    # 2. Log the event in the structured format
                    log_event(log_protocol, log_type, log_src, log_dst, log_payload_hex)
                    
                    time.sleep(0.05) # Small buffer between packets

                except Exception as e:
                    print(f"\n[CRITICAL ERROR] Failed to send {log_type}. Generator stopping. Details: {e}")
                    sys.exit(1)
            
            time.sleep(SLEEP_SECONDS)

    except KeyboardInterrupt:
        print("\n[*] Traffic generator gracefully stopped by user.")
        sys.exit(0)

if __name__ == "__main__":
    main()
import logging
import sys
import argparse

# --- PyModbus v3.x Specific Imports ---
# This version forces the most explicit, direct import path for v3.x contexts.

try:
    # 1. Server Startup Function
    from pymodbus.server import StartTcpServer
    
    # 2. Data Block Definition (e.g., holding registers, coils)
    from pymodbus.datastore import ModbusSequentialDataBlock
    
    # 3. Context Definitions (The specific classes that were failing)
    # They are commonly located in a 'context' submodule within datastore in v3.x
    from pymodbus.datastore.context import ModbusSlaveContext, ModbusServerContext

    # 4. Modbus device identification
    from pymodbus.device import ModbusDeviceIdentification
    
except ImportError as e:
    print(f"FATAL ERROR: Import failed. The pymodbus library structure is incompatible.")
    print(f"Details: {e}")
    print("\n--- FIXING SUGGESTION ---")
    print("Since you are on a new Python version (e.g., 3.13), please try installing a known stable PyModbus release.")
    print("1. Uninstall the current version:")
    print("   C:\\...\\python.exe -m pip uninstall pymodbus")
    print("2. Install a stable 3.x version (e.g., 3.6.4):")
    print("   C:\\...\\python.exe -m pip install pymodbus==3.6.4")
    
    sys.exit(1)


# Configure logging
logging.basicConfig(
    format='%(asctime)s %(levelname)-8s %(name)-15s %(message)s',
    level=logging.DEBUG,
)

def get_command_line_args():
    """Parse command line arguments for flexibility."""
    parser = argparse.ArgumentParser(description="Modbus TCP Mock Server")
    parser.add_argument("--ip", default="127.0.0.1", help="IP to bind to (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=5020, help="Port to listen on (default: 5020)")
    return parser.parse_args()

def setup_payload():
    """Create the memory map with some interesting test data."""
    # Initialize with non-zero data to make testing easier
    # HR will have values 0, 1, 2, 3, 4...
    hr_data = [i for i in range(100)] 
    
    store = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0]*100),
        co=ModbusSequentialDataBlock(0, [0]*100),
        hr=ModbusSequentialDataBlock(0, hr_data), # Holding Registers have data!
        ir=ModbusSequentialDataBlock(0, [0]*100))
    
    return ModbusServerContext(slaves=store, single=True)

def setup_identity():
    """Define the server identity info (Vendor, Product Name, etc)."""
    identity = ModbusDeviceIdentification()
    identity.VendorName = 'MyCompany'
    identity.ProductCode = 'MockPLC'
    identity.VendorUrl = 'http://github.com/pymodbus'
    identity.ProductName = 'Modbus Server'
    identity.ModelName = 'PyModbus v3'
    identity.MajorMinorRevision = '1.0'
    return identity

def run_server():
    args = get_command_line_args()
    context = setup_payload()
    identity = setup_identity()

    print("--- Starting Mock PLC (Modbus Server) ---")
    print(f"Python Version: {sys.version}")
    print(f"Listening on {args.ip}:{args.port}")
    print("Holding Registers 0-99 pre-loaded with sequential values.")
    print("Press Ctrl+C to stop")

    try:
        StartTcpServer(
            context=context, 
            identity=identity, 
            address=(args.ip, args.port)
        )
    except OSError as e:
        print(f"\nERROR: Could not bind to port {args.port}. Is it already in use?")
    except KeyboardInterrupt:
        print("\nStopping server...")

if __name__ == "__main__":
    run_server()
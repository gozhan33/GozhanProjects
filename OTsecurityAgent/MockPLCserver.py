import logging
import sys

# --- PyModbus v3.x Specific Imports ---
# The previous multi-try-catch blocks failed because pymodbus's internal 
# structure is inconsistent across minor versions. This version forces 
# the most explicit, direct import path for v3.x contexts.

try:
    # 1. Server Startup Function
    from pymodbus.server import StartTcpServer
    
    # 2. Data Block Definition (e.g., holding registers, coils)
    from pymodbus.datastore import ModbusSequentialDataBlock
    
    # 3. Context Definitions (The specific classes that were failing)
    # They are commonly located in a 'context' submodule within datastore in v3.x
    from pymodbus.datastore.context import ModbusSlaveContext, ModbusServerContext
    
except ImportError as e:
    print(f"FATAL ERROR: Import failed. The pymodbus library structure is incompatible.")
    print(f"Details: {e}")
    print("\n--- FIXING SUGGESTION ---")
    print("Since you are on a new Python version (3.13), please try installing a known stable PyModbus release.")
    print("1. Uninstall the current version:")
    print("   C:\\...\\python.exe -m pip uninstall pymodbus")
    print("2. Install a stable 3.x version (e.g., 3.6.4):")
    print("   C:\\...\\python.exe -m pip install pymodbus==3.6.4")
    
    sys.exit(1)


# Configure logging
logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

def run_server():
    print("--- Starting Mock PLC (Modbus Server) ---")
    print(f"Python Version: {sys.version}")
    print("Listening on 0.0.0.0:5020")
    print("Press Ctrl+C to stop")

    # Define the memory map
    # Note: 'single=True' means we only use one Slave ID (Unit ID 1)
    store = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0]*100),
        co=ModbusSequentialDataBlock(0, [0]*100),
        hr=ModbusSequentialDataBlock(0, [0]*100),
        ir=ModbusSequentialDataBlock(0, [0]*100))

    context = ModbusServerContext(slaves=store, single=True)

    # Run the server
    StartTcpServer(context=context, address=("0.0.0.0", 5020))

if __name__ == "__main__":
    run_server()
# OT Cybersecurity Agentic AI Demo Report

This document outlines the architecture and execution results of the OT Cybersecurity Demo. The solution simulates a converged IT/OT environment, captures live network traffic, and uses a multi-agent AI architecture to detect, triage, and analyze threats in real-time using the MITRE ATT&CK for ICS and OWASP frameworks.

## System Overview

The demo consists of four key components working in a pipeline:

Mock PLC Server (Python): Simulates a Modbus PLC listening on port 5020.

OT Traffic Generator (Python/Scapy): Generates synthetic traffic, including normal operations, Modbus control actions (Force Single Coil), and HMI web attacks (SQL Injection).

Ingest Agent (C++): Uses Npcap to sniff loopback traffic (127.0.0.1) in real-time and outputs raw event data to stdout.

Agent Orchestrator (Python):

Pipes the ingestion output.

Triage Agent: Filters for suspicious activity.

Expert Agents: Passes anomalies to MITRE and OWASP agents powered by Gemini for analysis.

Alerting: Displays context-aware alerts on the command prompt.

### 1. Mock PLC Server Output

The server initializes and listens for Modbus TCP connections on all interfaces. For this mock PLC server, pymodbus v3.6.4 was used.

```
C:\Users\gokha\GitHub\GozhanProjects\OTsecurityAgent>python MockPLCserver.py
--- Starting Mock PLC (Modbus Server) ---
Python Version: 3.13.3 (tags/v3.13.3:6280bb5, Apr  8 2025, 14:47:33) [MSC v.1943 64 bit (AMD64)]
Listening on 0.0.0.0:5020
Press Ctrl+C to stop
INFO:pymodbus.logging:Server listening.
```


### 2. OT Traffic Generator Output

The generator injects packets into the loopback interface. Below are logs showing a generated Control Action (Modbus) and a Web Attack (HTTP). For this traffic generator, scapy v2.6.1 was used.

```
C:\Users\gokha\GitHub\GozhanProjects\OTsecurityAgent>python OT_TrafficGenerator.py
[*] Python Traffic Generator started.
[*] Packets sent via Layer 3 (send) to 127.0.0.1 for C++ sniffer.
[*] HMI Traffic now targets Port 8080 (high port) for visibility.
[*] Structured ground truth logs printed to stdout every 1s.
```

```json
{
  "module": "INGESTION_AGENT",
  "app_id": "OT-AGENT-DEMO",
  "timestamp": "1763773592",
  "protocol": "Modbus/TCP",
  "type": "CONTROL ACTION: Force Single Coil (0x05)",
  "src": "127.0.0.1:49153",
  "dst": "127.0.0.1:5020",
  "payload_snippet": "00 02 00 00 00 06 01 05 00 05 ff 00"
}
```

```json
{
  "module": "INGESTION_AGENT",
  "app_id": "OT-AGENT-DEMO",
  "timestamp": "1763773592",
  "protocol": "HTTP-HMI",
  "type": "ATTACK: Potential SQL Injection",
  "src": "127.0.0.1:60270",
  "dst": "127.0.0.1:8080",
  "payload_snippet": "47 45 54 20 2f 61 70 69 2f 74 61 67 73 3f 69 64 3d 31 30 35 20 4f 52 20 31 3d 31 20 2d 2d 20 48 54 54 50 2f 31 2e 31 0d 0a 48 6f 73 74 3a 20 31 39 32 2e 31 36 38 2e 31 2e 31 30 0d 0a 55 73 65 72 2d 41 67 65 6e 74 3a 20 41 74 74 61 63 6b 65 72 2d 54 6f 6f 6c 0d 0a 0d 0a"
}
```


### 3. Agent Orchestrator Output (Analysis & Response)

The C++ sniffer captures the traffic above and pipes it to the Orchestrator. The C++ snipper is built using Npcap SDK (https://npcap.com/#download). 

The logs below demonstrate the Dual-Analysis capability, where the AI correctly identifies the nature of each attack.

**Execution Command:**
```bash
WindowsOTsecIngestAgent.exe loopback | python OT_Agent_Orchestrator.py
```

#### Detection 1: Modbus Control Action

The system identifies a direct manipulation of the controller.

```
[*] Attempting to load configuration from: C:\Users\gokha\KeyVault\config.json
[*] Gemini API Key loaded successfully from config file.
[*] TriageAgent initialized. Starting baseline checks.
[*] MITREAgent initialized. Ready to perform LLM-based reasoning.
[*] OWASPAgent initialized. Ready to perform LLM-based web security reasoning.
[*] Orchestrator started. Listening for events on stdin...
[*] Anomaly detected: Direct Control Command/Write Action. Forwarding for dual analysis.
[*] Calling Gemini for MITRE analysis...
[*] Calling Gemini for OWASP analysis...

================================================================================
                !!! CRITICAL ANOMALY DETECTED & DUAL-ANALYZED !!!
================================================================================
| REASON: Direct Control Command/Write Action
| PROTOCOL: MODBUS TCP (Write Single Coil (0x05) [CONTROL ACTION])
| TRAFFIC: 127.0.0.1 -> 127.0.0.1
--------------------------------------------------------------------------------
| MITRE ATT&CK for ICS Mapping:
|   TECHNIQUE: T0832.001 - Direct Parameter/State Change
|   PHASE: Impact
|   SUMMARY: The anomaly involves a direct, high-priority control command
|            (MODBUS Write Single Coil, 0x05) used to modify the discrete
|            state of an operational component, which is a clear mechanism
|            for directly changing device parameters or output states to
|            affect the controlled process.
--------------------------------------------------------------------------------
| OWASP Web Security Mapping (LOW RELEVANCE):
|   CATEGORY: A00: None
|   SUMMARY: This event is a native MODBUS TCP 'Write Single Coil' control
|            command (Function Code 0x05), which is a standard operational
|            technology (OT) protocol action. The payload is purely binary
|            MODBUS structure and does not contain characteristics of web-
|            based attacks like SQL injection, XSS, or manipulation of HTTP
|            context. Therefore, it falls outside the scope of the standard
|            OWASP Top 10.
================================================================================
```


#### Detection 2: HMI Web Attack (SQL Injection)

The system identifies a web-based attack targeting the HMI, correctly mapping it to OWASP Injection while providing MITRE context for "Programmatic Interface" discovery.

```
[*] Anomaly detected: Non-Baseline/Uncategorized Traffic. Forwarding for dual analysis.
[*] Calling Gemini for MITRE analysis...
[*] Calling Gemini for OWASP analysis...

================================================================================
                !!! CRITICAL ANOMALY DETECTED & DUAL-ANALYZED !!!
================================================================================
| REASON: Non-Baseline/Uncategorized Traffic
| PROTOCOL: HTTP/HMI Simulation (Potential HTTP Attack (GET Request))
| TRAFFIC: 127.0.0.1:60220 -> 127.0.0.1:8080
--------------------------------------------------------------------------------
| MITRE ATT&CK for ICS Mapping:
|   TECHNIQUE: T0843 - Programmatic Interface
|   PHASE: Discovery
|   SUMMARY: The use of an HTTP GET request targeting the HMI's specific API
|            endpoint (`/api/tags`) indicates an attempt to leverage a
|            Programmatic Interface to enumerate process variables. This
|            action is a form of Discovery aimed at gathering detailed
|            configuration and tag information about the operational
|            environment.
--------------------------------------------------------------------------------
| OWASP Web Security Mapping (HIGH RELEVANCE):
|   CATEGORY: A03:2021 - Injection
|   SUMMARY: The event uses the HTTP protocol to target an API endpoint
|            (/api/tags) with a parameter (id). This structure is
|            characteristic of probing for input validation vulnerabilities,
|            such as SQL Injection or command injection, which maps directly
|            to the Injection category of web application attacks against the
|            HMI's web interface.
================================================================================
```

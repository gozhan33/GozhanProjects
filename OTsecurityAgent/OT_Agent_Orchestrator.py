import sys
import json
import time
import random
import requests
import os
import argparse # NEW: For command-line arguments
from typing import Dict, Any, Optional, List, Tuple
from abc import ABC, abstractmethod

# --- Configuration & Constants ---

EVENT_DELIMITER = "------------------------------------------------"
MAX_RETRIES = 5
BASE_DELAY = 1.0 # seconds

# LLM Configurations
GEMINI_MODEL = "gemini-2.5-flash-preview-09-2025"
OPENAI_MODEL = "gpt-4o"
OPENAI_URL = "https://api.openai.com/v1/chat/completions"


# --- Key Loading ---

def load_api_keys() -> Dict[str, str]:
    """
    Loads API keys for both Gemini and OpenAI from the config file.
    Returns a dictionary: {'gemini': 'key_value', 'openai': 'key_value'}
    """
    home_dir = os.path.expanduser('~')
    config_path = os.path.join(home_dir, 'KeyVault', 'config.json')
    keys: Dict[str, str] = {}

    print(f"[*] Attempting to load configurations from: {config_path}", file=sys.stderr)

    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
            
            # 1. Load Gemini Key
            keys['gemini'] = config.get('gemini', {}).get('api_key', "")
            if keys['gemini']:
                print("[*] Gemini API Key loaded.", file=sys.stderr)
            
            # 2. Load OpenAI Key
            keys['openai'] = config.get('openai', {}).get('api_key', "")
            if keys['openai']:
                print("[*] OpenAI API Key loaded.", file=sys.stderr)
            
            if not keys['gemini'] and not keys['openai']:
                 print("[CRITICAL] Neither 'gemini.api_key' nor 'openai.api_key' found in config file.", file=sys.stderr)
            
            return keys
    
    except FileNotFoundError:
        print(f"[CRITICAL] Configuration file not found at: {config_path}", file=sys.stderr)
        return keys
    except json.JSONDecodeError:
        print(f"[CRITICAL] Failed to parse config.json. Check file format.", file=sys.stderr)
        return keys
    except Exception as e:
        print(f"[CRITICAL] Unexpected error during key loading: {e}", file=sys.stderr)
        return keys

# Load all keys once
ALL_API_KEYS = load_api_keys()


# --- LLM Service Abstraction ---

class LLMService(ABC):
    """Abstract base class for all LLM API handlers (Gemini, OpenAI, etc.)."""
    
    @abstractmethod
    def call_api(self, payload: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Handles the actual HTTP call, headers, and retry logic."""
        pass
    
    @abstractmethod
    def build_payload(self, system_prompt: str, user_query: str, response_schema: Dict[str, Any]) -> Dict[str, Any]:
        """Constructs the provider-specific API request body."""
        pass

    @abstractmethod
    def parse_response_content(self, response_json: Dict[str, Any]) -> str:
        """Extracts the raw JSON string content from the provider's response format."""
        pass


# --- Concrete Service Implementations ---

class GeminiLLMService(LLMService):
    """Concrete implementation for the Gemini API."""

    def __init__(self, api_key: str):
        if not api_key:
            raise ValueError("Gemini API key is required.")
        self.api_key = api_key
        self.api_url = f"https://generativelanguage.googleapis.com/v1beta/models/{GEMINI_MODEL}:generateContent?key={self.api_key}"

    def build_payload(self, system_prompt: str, user_query: str, response_schema: Dict[str, Any]) -> Dict[str, Any]:
        """Builds the Gemini-specific JSON payload."""
        return {
            "contents": [{"parts": [{"text": user_query}]}],
            "systemInstruction": {"parts": [{"text": system_prompt}]},
            "generationConfig": {
                "responseMimeType": "application/json",
                "responseSchema": response_schema
            }
        }
    
    def parse_response_content(self, response_json: Dict[str, Any]) -> str:
        """Extracts content from Gemini response."""
        return response_json['candidates'][0]['content']['parts'][0]['text']

    def call_api(self, payload: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Handles API retries for Gemini."""
        headers = {'Content-Type': 'application/json'}
        for attempt in range(MAX_RETRIES):
            try:
                response = requests.post(self.api_url, headers=headers, json=payload, timeout=30)
                response.raise_for_status() 
                return response.json()
            except requests.exceptions.HTTPError as e:
                if response.status_code == 429 and attempt < MAX_RETRIES - 1:
                    delay = BASE_DELAY * (2 ** attempt) + random.uniform(0, 1)
                    print(f"[ERROR] Gemini Rate limited (429) on attempt {attempt + 1}. Retrying in {delay:.2f}s.", file=sys.stderr)
                    time.sleep(delay)
                    continue
                else:
                    print(f"[CRITICAL] Gemini HTTP Error after {attempt + 1} attempts or non-recoverable error ({response.status_code}): {e}", file=sys.stderr)
                    return None
            except requests.exceptions.RequestException as e:
                if attempt < MAX_RETRIES - 1:
                    delay = BASE_DELAY * (2 ** attempt) + random.uniform(0, 1)
                    print(f"[ERROR] Gemini call failed on attempt {attempt + 1}. Retrying in {delay:.2f}s. Error: {e}", file=sys.stderr)
                    time.sleep(delay)
                else:
                    print(f"[CRITICAL] Gemini call failed after {MAX_RETRIES} attempts. Giving up. Error: {e}", file=sys.stderr)
                    return None
        return None

# NEW CLASS IMPLEMENTATION
class OpenAILLMService(LLMService):
    """Concrete implementation for the OpenAI API."""

    def __init__(self, api_key: str):
        if not api_key:
            raise ValueError("OpenAI API key is required.")
        self.api_key = api_key
        self.api_url = OPENAI_URL
        self.model = OPENAI_MODEL

    def build_payload(self, system_prompt: str, user_query: str, response_schema: Dict[str, Any]) -> Dict[str, Any]:
        """Builds the OpenAI-specific JSON payload, using response_format for structured output."""
        return {
            "model": self.model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_query}
            ],
            # OpenAI structured output
            "response_format": {"type": "json_object"}
        }

    def parse_response_content(self, response_json: Dict[str, Any]) -> str:
        """Extracts content from OpenAI response."""
        return response_json['choices'][0]['message']['content']

    def call_api(self, payload: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Handles API retries for OpenAI."""
        headers = {
            'Content-Type': 'application/json',
            'Authorization': f'Bearer {self.api_key}'
        }
        
        for attempt in range(MAX_RETRIES):
            try:
                response = requests.post(self.api_url, headers=headers, json=payload, timeout=30)
                response.raise_for_status()
                return response.json()
            except requests.exceptions.HTTPError as e:
                if response.status_code == 429 and attempt < MAX_RETRIES - 1:
                    delay = BASE_DELAY * (2 ** attempt) + random.uniform(0, 1)
                    print(f"[ERROR] OpenAI Rate limited (429) on attempt {attempt + 1}. Retrying in {delay:.2f}s.", file=sys.stderr)
                    time.sleep(delay)
                    continue
                else:
                    print(f"[CRITICAL] OpenAI HTTP Error after {attempt + 1} attempts or non-recoverable error ({response.status_code}): {e}", file=sys.stderr)
                    return None
            except requests.exceptions.RequestException as e:
                if attempt < MAX_RETRIES - 1:
                    delay = BASE_DELAY * (2 ** attempt) + random.uniform(0, 1)
                    print(f"[ERROR] OpenAI call failed on attempt {attempt + 1}. Retrying in {delay:.2f}s. Error: {e}", file=sys.stderr)
                    time.sleep(delay)
                else:
                    print(f"[CRITICAL] OpenAI call failed after {MAX_RETRIES} attempts. Giving up. Error: {e}", file=sys.stderr)
                    return None
        return None

# --- LLMSecurityAgent Base Class (Updated to use parse_response_content) ---

class LLMSecurityAgent(ABC):
    """
    Base class for all security expert agents (MITRE, OWASP, etc.).
    It delegates API communication to a pluggable LLMService instance.
    """
    
    SYSTEM_PROMPT: str = "" 
    RESPONSE_SCHEMA: Dict[str, Any] = {}
    
    def __init__(self, llm_service: LLMService):
        self.llm_service = llm_service
        self.agent_name = self.__class__.__name__
        print(f"[*] {self.agent_name} initialized and linked to {self.llm_service.__class__.__name__}.")

    def perform_analysis(self, event: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        
        print(f"[*] Calling LLM Service for {self.agent_name} analysis...")

        user_query = self._build_user_query(event)
        
        # 1. Build the provider-specific payload
        payload = self.llm_service.build_payload(
            self.SYSTEM_PROMPT, 
            user_query, 
            self.RESPONSE_SCHEMA
        )

        # 2. Call the provider's API
        response_json = self.llm_service.call_api(payload)
        
        if response_json:
            try:
                # Use the service's specific method to extract the JSON string content
                json_string = self.llm_service.parse_response_content(response_json)
                # Parse the JSON string into a Python dictionary
                return json.loads(json_string)
            except (json.JSONDecodeError, KeyError) as e:
                print(f"[LLM PARSE ERROR] {self.agent_name} failed to parse JSON response: {e}", file=sys.stderr)
                # print(f"[DEBUG RAW RESPONSE] {response_json}", file=sys.stderr)
                return None
        return None

    def _build_user_query(self, event: Dict[str, Any]) -> str:
        # ... (Implementation remains the same)
        return (
            f"Analyze the following high-priority anomaly event:\n"
            f"**Anomaly Reason:** {event['anomaly_reason']}\n"
            f"**Protocol:** {event.get('protocol', 'Unknown')}\n"
            f"**Action/Type:** {event.get('type', 'Unknown')}\n"
            f"**Source IP:** {event.get('src', 'N/A')} -> **Destination IP:** {event.get('dst', 'N/A')}\n"
            f"**Payload Snippet (Hex):** {event.get('payload_snippet', 'No Data')}\n\n"
            f"Based on this data, provide your specific security analysis."
        )


# --- MITREAgent and OWASPAgent (Expertise remains the same) ---
# ... (MITREAgent and OWASPAgent definitions remain the same, as they inherit all execution logic)

# [Code for TriageAgent, parse_log_block, display_analysis, consume_ingestion_stream remains the same]
# --- TriageAgent and Utility Functions (Unchanged) ---
class TriageAgent:
    """
    Agent 1: Establishes a basic baseline and filters events before passing 
    them to the specialized expert agents (MITRE/OWASP).
    """
    NORMAL_READ_ACTIONS = [
        "Read Coils (0x01)", 
        "Read Holding Registers (0x03)", 
        "Ack_Data (Response)",
        "DNP3 Frame Detected",
        "PN-IO RT Cyclic Data"
    ]

    def __init__(self):
        print("[*] TriageAgent initialized. Starting baseline checks.")

    def check_for_anomaly(self, event: Dict[str, Any]) -> bool:
        """
        Analyzes an event and returns True if it's a significant anomaly 
        that requires expert analysis (Agents 2/3).
        """
        command_type = event.get("type", "")
        
        # Rule 1: Flag all control commands immediately
        if "CONTROL ACTION" in command_type or command_type in ["Userdata", "Job Request"]:
            event["anomaly_reason"] = "Direct Control Command/Write Action"
            return True

        # Rule 2: Flag unknown or suspicious reconnaissance traffic
        if "Unknown" in command_type or "RECONNAISSANCE" in command_type:
            event["anomaly_reason"] = "Suspicious Reconnaissance or Unknown Protocol Action"
            return True

        # Rule 3: Allow normal read traffic to pass through (Noise Reduction)
        if command_type in self.NORMAL_READ_ACTIONS:
            return False
            
        # Default: If it's not a known normal read action, treat it as an anomaly
        event["anomaly_reason"] = "Non-Baseline/Uncategorized Traffic"
        return True

def parse_log_block(block_lines: list) -> dict:
    """
    Parses a block of log lines generated by the Traffic Generator
    into a structured Python dictionary, with added debug output.
    """
    event = {}
    
    for line in block_lines:
        line = line.strip()
        if not line:
            continue
            
        line_cleaned_comma = line.rstrip(',')

        try:
            key_part, value_part = line_cleaned_comma.split(':', 1)
        except ValueError:
            continue
            
        key = key_part.strip().strip('"')
        value = value_part.strip().strip('"')
        
        if key == 'timestamp':
            try:
                event[key] = int(value)
            except ValueError:
                event[key] = value
        else:
            event[key] = value
            
    if 'module' in event and event['module'] == 'INGESTION_AGENT':
        return event

    print(f"[DEBUG PARSE] FAILURE. Required field 'module' missing or wrong value. Final dict keys: {list(event.keys())}", file=sys.stderr)
    return {}

class MITREAgent(LLMSecurityAgent):
    """Agent 2: Maps anomalies to MITRE ATT&CK for ICS."""
    
    SYSTEM_PROMPT = (
        "You are an expert industrial control systems (ICS) threat intelligence analyst. "
        "Your task is to analyze a raw network anomaly detected by an OT agent and map it "
        "to the most relevant MITRE ATT&CK for ICS Technique (T-number). "
        "The analysis must be based on the provided event data and your domain knowledge."
        "Provide a concise, single-paragraph analysis focusing only on the most plausible technique. "
        "Your final output MUST be in the following JSON format ONLY, without any other preceding or concluding text:\n"
        '{\n'
        '   "mitre_technique": "TXXXX.XX",\n'
        '   "technique_name": "Technique Name",\n'
        '   "plausible_phase": "Phase of Attack (e.g., Impact, Command and Control)",\n'
        '   "analysis_summary": "Concise justification for the mapping (1-2 sentences)."\n'
        '}'
    )

    RESPONSE_SCHEMA = {
        "type": "OBJECT",
        "properties": {
            "mitre_technique": {"type": "STRING", "description": "The specific T-number, e.g., T0807."},
            "technique_name": {"type": "STRING", "description": "The formal name of the technique."},
            "plausible_phase": {"type": "STRING", "description": "The stage of the attack lifecycle."},
            "analysis_summary": {"type": "STRING", "description": "A brief explanation of why this technique is plausible."}
        },
        "required": ["mitre_technique", "technique_name", "plausible_phase", "analysis_summary"]
    }

class OWASPAgent(LLMSecurityAgent):
    """Agent 3: Maps web-based anomalies to OWASP Top 10."""
    
    SYSTEM_PROMPT = (
        "You are an expert web application security analyst focused on IT/OT convergence points (like HMIs and IIoT gateways). "
        "Your task is to analyze a raw network anomaly event. Determine if the payload or context suggests an attack "
        "that maps to the OWASP Top 10 list (e.g., Injection, Broken Access Control)."
        "Provide a concise, single-paragraph analysis, focusing on whether this is likely a web-based attack and why."
        "If it is clearly an industrial protocol (e.g., Modbus), state that it is not applicable to web security and provide the default technique mapping."
        "Your final output MUST be in the following JSON format ONLY:\n"
        '{\n'
        '   "owasp_category": "AXX:YYYY - Category Name",\n'
        '   "is_web_attack": false,\n'
        '   "analysis_summary": "Concise justification for the mapping (1-2 sentences)."\n'
        '}'
    )

    RESPONSE_SCHEMA = {
        "type": "OBJECT",
        "properties": {
            "owasp_category": {"type": "STRING", "description": "The relevant OWASP Top 10 category, e.g., A03:2021 - Injection. If non-applicable, use 'A00: None'."},
            "is_web_attack": {"type": "BOOLEAN", "description": "True if this event is highly likely a web/IT-based attack, False otherwise."},
            "analysis_summary": {"type": "STRING", "description": "A brief explanation of the web security relevance or lack thereof."}
        },
        "required": ["owasp_category", "is_web_attack", "analysis_summary"]
    }

def display_analysis(event: Dict[str, Any], mitre_report: Dict[str, str], owasp_report: Dict[str, Any]):
    """Formats and prints the final report to stdout for the user/next step."""
    
    print("\n" + "="*80)
    print(f"!!! CRITICAL ANOMALY DETECTED & DUAL-ANALYZED !!!".center(80))
    print("="*80)
    print(f"| REASON: {event.get('anomaly_reason', 'N/A')}")
    print(f"| PROTOCOL: {event.get('protocol', 'N/A')} ({event.get('type', 'N/A')})")
    print(f"| TRAFFIC: {event.get('src', 'N/A')} -> {event.get('dst', 'N/A')}")
    print("-" * 80)
    
    # MITRE Analysis Results (ICS Expert)
    print(f"| MITRE ATT&CK for ICS Mapping:")
    print(f"|   TECHNIQUE: {mitre_report.get('mitre_technique', 'N/A')} - {mitre_report.get('technique_name', 'N/A')}")
    print(f"|   PHASE: {mitre_report.get('plausible_phase', 'N/A')}")
    print(f"|   SUMMARY: {mitre_report.get('analysis_summary', 'N/A')}")
    print("-" * 80)

    # OWASP Analysis Results (IT/Web Expert)
    is_web = owasp_report.get('is_web_attack', False)
    print(f"| OWASP Web Security Mapping ({'HIGH RELEVANCE' if is_web else 'LOW RELEVANCE'}):")
    print(f"|   CATEGORY: {owasp_report.get('owasp_category', 'N/A')}")
    print(f"|   SUMMARY: {owasp_report.get('analysis_summary', 'N/A')}")
    print("="*80 + "\n")

def consume_ingestion_stream(triage_agent: TriageAgent, mitre_agent: LLMSecurityAgent, owasp_agent: LLMSecurityAgent):
    """
    The main loop that reads event blocks from stdin (piped from the Traffic Generator).
    """
    event_lines: List[str] = []
    print("[*] Orchestrator started. Listening for events on stdin...")
    
    # Read line-by-line indefinitely
    for line in sys.stdin:
        line = line.strip()

        if line == EVENT_DELIMITER:
            parsed_event = parse_log_block(event_lines)
            
            if parsed_event:
                if triage_agent.check_for_anomaly(parsed_event):
                    # --- Anomaly Detected: Hand-off to Expert Agents ---
                    print(f"[*] Anomaly detected: {parsed_event['anomaly_reason']}. Forwarding for dual analysis.", file=sys.stderr)
                    
                    # Call Agent 2 (MITRE Reasoner)
                    mitre_analysis = mitre_agent.perform_analysis(parsed_event)
                    
                    # Call Agent 3 (OWASP Reasoner)
                    owasp_analysis = owasp_agent.perform_analysis(parsed_event)
                    
                    if mitre_analysis and owasp_analysis:
                        display_analysis(parsed_event, mitre_analysis, owasp_analysis)
                    else:
                        print("[!!!] One or both Expert Agents failed to generate a report.", file=sys.stderr)
                        
            else:
                print("[!!!] Block skipped due to parsing failure or missing required fields.", file=sys.stderr)
                
            # Reset buffer for the next event
            event_lines = []
        else:
            # Accumulate lines into the current event list
            event_lines.append(line)

# --- Argument Parsing ---

def get_command_line_args():
    """Parses command line arguments to select the LLM provider."""
    parser = argparse.ArgumentParser(description="LLM Security Agent Orchestrator.")
    parser.add_argument(
        "--llm", 
        type=str, 
        default="gemini", 
        choices=["gemini", "openai"],
        help="The LLM provider to use for analysis (default: gemini)."
    )
    return parser.parse_args()


# --- Main Execution ---

def main():
    # 1. Get arguments
    args = get_command_line_args()
    selected_llm = args.llm
    
    # 2. Check key and select service implementation
    llm_service: Optional[LLMService] = None
    api_key = ALL_API_KEYS.get(selected_llm)

    if not api_key:
        print(f"[FATAL] API Key for '{selected_llm}' not found or loaded. Aborting.", file=sys.stderr)
        sys.exit(1)

    if selected_llm == "gemini":
        llm_service = GeminiLLMService(api_key=api_key)
    elif selected_llm == "openai":
        llm_service = OpenAILLMService(api_key=api_key)

    if llm_service is None:
        print(f"[FATAL] Failed to initialize LLM service for '{selected_llm}'. Aborting.", file=sys.stderr)
        sys.exit(1)
        
    # 3. Initialize Agents by injecting the selected LLM service
    triage_agent = TriageAgent()
    mitre_agent = MITREAgent(llm_service=llm_service)
    owasp_agent = OWASPAgent(llm_service=llm_service) 
    
    print(f"[*] Orchestrator running with {selected_llm.upper()} ({llm_service.__class__.__name__}) as the backend.", file=sys.stderr)

    consume_ingestion_stream(triage_agent, mitre_agent, owasp_agent)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nOrchestrator stopped by user.", file=sys.stderr)
        sys.exit(0)
    except Exception as e:
        print(f"\n[FATAL ERROR] Orchestrator crashed: {e}", file=sys.stderr)
        sys.exit(1)
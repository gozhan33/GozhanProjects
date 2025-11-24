/**
 * OT Cybersecurity Agent - Windows Ingestion Layer PoC
 * * Purpose: Sniff network traffic and identify/parse ICS protocols on Windows.
 * * Dependencies: Npcap SDK (https://npcap.com/#download)
 * * Compiler: MSVC (Visual Studio) or MinGW
 * * Linker Flags: wpcap.lib ws2_32.lib
 * * !!! CRITICAL NOTE ON LOOPBACK CAPTURE !!!
 * For the Python traffic generator (127.0.0.1) to work, you MUST
 * select the dedicated 'Npcap Loopback Adapter' from the list of interfaces.
 * Selecting a physical Ethernet/WiFi adapter will NOT capture loopback traffic.
 */

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define HAVE_REMOTE

#include <winsock2.h>
#include <ws2tcpip.h> 
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

// Npcap includes
#include "pcap.h"

// Link against Ws2_32.lib and wpcap.lib
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "wpcap.lib")

// --- Protocol Constants ---
#define PORT_MODBUS 5020 // Modbus/TCP
#define PORT_HMI_HTTP_SIM 8080 // FIX: Simulated HMI Web Server port (used for attack traffic)
#define PORT_DNP3 20000
#define PORT_S7 102
#define ETHERTYPE_PROFINET 0x8892
#define ETHERTYPE_IP 0x0800

// --- Manual Struct Definitions (Simplified for Windows compatibility) ---

// Ethernet Header (Needed for standard interfaces)
struct ether_header {
    u_char ether_dhost[6];     // destination host address
    u_char ether_shost[6];     // source host address
    u_short ether_type;        // IP, ARP, etc.
};

// IP Header (Standard definitions)
struct ip_header {
    u_char ip_vhl;             // version << 4 | header length >> 2
    u_char ip_tos;             // type of service 
    u_short ip_len;            // total length 
    u_short ip_id;             // identification
    u_short ip_off;            // fragment offset field
    u_char ip_ttl;             // time to live
    u_char ip_p;               // protocol
    u_short ip_sum;            // checksum
    struct in_addr ip_src,ip_dst; // source and dest address
};
#define IP_HL(ip) (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip) (((ip)->ip_vhl) >> 4)

// TCP Header (Standard definitions)
struct tcp_header {
    u_short th_sport;      // source port
    u_short th_dport;      // destination port
    u_long th_seq;         // sequence number
    u_long th_ack;         // acknowledgement number
    u_char th_offx2;       // data offset, rsvd
    #define TH_OFF(th) (((th)->th_offx2 & 0xf0) >> 4)
    u_char th_flags;       // flags
    u_short th_win;        // window
    u_short th_sum;        // checksum
    u_short th_urp;        // urgent pointer
};


// --- Data Structures for Simulation ---
struct OTEvent {
    std::string timestamp;
    std::string source_ip;
    std::string dest_ip;
    std::string protocol;
    std::string command_type; 
    std::string raw_hex;       
};


class OTAgentIngestor {
public:
    OTAgentIngestor(pcap_t* handle) : pcap_handle(handle) {
        // Determine the datalink type on startup
        datalink_type = pcap_datalink(pcap_handle);
    }

    void start() {
        // Compile filter to capture only relevant traffic (optimization)
        struct bpf_program fp;
        
        // Add Modbus, S7, HMI HTTP Sim, and DNP3 to the BPF filter
        std::string filter_exp = 
            "tcp port " + std::to_string(PORT_MODBUS) + 
            " or tcp port " + std::to_string(PORT_S7) + 
            " or tcp port " + std::to_string(PORT_HMI_HTTP_SIM) + 
            " or udp port " + std::to_string(PORT_DNP3) + 
            " or ether proto 0x8892";
        
        if (pcap_compile(pcap_handle, &fp, filter_exp.c_str(), 0, PCAP_NETMASK_UNKNOWN) == -1) {
             std::cerr << "Error compiling filter: " << pcap_geterr(pcap_handle) << std::endl;
             return;
        }
        if (pcap_setfilter(pcap_handle, &fp) == -1) {
            std::cerr << "Error setting filter" << std::endl;
            return;
        }

        printf("[*] BPF Filter set to: %s\n", filter_exp.c_str());

        // Start loop
        pcap_loop(pcap_handle, 0, packetHandler, reinterpret_cast<u_char*>(this));
    }

private:
    pcap_t* pcap_handle;
    int datalink_type;

    // Static callback wrapper to call member function
    static void packetHandler(u_char* userData, const struct pcap_pkthdr* pkthdr, const u_char* packet) {
        OTAgentIngestor* agent = reinterpret_cast<OTAgentIngestor*>(userData);
        agent->processPacket(pkthdr, packet);
    }

    void processPacket(const struct pcap_pkthdr* pkthdr, const u_char* packet) {
        
        int offset = 0;
        uint16_t ether_type = ETHERTYPE_IP; 

        // --- Datalink Type Check (CRITICAL FOR LOOPBACK) ---
        // DLT_EN10MB (Ethernet) is the standard 14-byte header
        if (datalink_type == DLT_EN10MB) {
            offset = 14; 
            struct ether_header* eth_header = (struct ether_header*)packet;
            ether_type = ntohs(eth_header->ether_type);
        } 
        // DLT_NULL/DLT_LOOP is usually 4 bytes for Npcap Loopback
        else if (datalink_type == DLT_NULL || datalink_type == DLT_LOOP || datalink_type == 24) { 
            offset = 4;
            ether_type = ETHERTYPE_IP;
        } 
        else {
             return; 
        }

        // 1. Handle Layer 2 Protocols (PROFINET) - Only works on DLT_EN10MB
        if (ether_type == ETHERTYPE_PROFINET) {
             return;
        }

        // 2. Handle Layer 3 (IP) -> TCP/UDP -> Layer 7 Protocols
        if (ether_type == ETHERTYPE_IP) {
            const u_char* ip_header_start = packet + offset; 
            
            // Check if packet length is long enough for IP header
            if (pkthdr->len < offset + sizeof(struct ip_header)) return;

            struct ip_header* ip_header = (struct ip_header*)ip_header_start;
            int ip_header_len = IP_HL(ip_header) * 4;

            // IP Address Formatting
            char src_ip[INET_ADDRSTRLEN];
            char dst_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);

            if (ip_header->ip_p == IPPROTO_TCP) {
                const u_char* tcp_header_start = ip_header_start + ip_header_len;
                
                // Check if packet length is long enough for TCP header
                if (pkthdr->len < offset + ip_header_len + sizeof(struct tcp_header)) return;

                struct tcp_header* tcp_header = (struct tcp_header*)tcp_header_start;
                int tcp_header_len = TH_OFF(tcp_header) * 4;
                
                const u_char* payload = tcp_header_start + tcp_header_len;
                int payload_len = pkthdr->len - (offset + ip_header_len + tcp_header_len);

                uint16_t src_port = ntohs(tcp_header->th_sport);
                uint16_t dst_port = ntohs(tcp_header->th_dport);

                // Check Ports for Protocols
                if (dst_port == PORT_MODBUS || src_port == PORT_MODBUS) {
                    handleModbusTCP(payload, payload_len, src_ip, dst_ip);
                } 
                // Add check for the simulated HTTP port (8080)
                else if (dst_port == PORT_HMI_HTTP_SIM || src_port == PORT_HMI_HTTP_SIM) {
                    handleTCPPayload(payload, payload_len, src_ip, dst_ip, 
                                     src_port, dst_port, "HTTP/HMI Simulation"); // <-- FIX APPLIED
                }
                // (Other protocols checks would go here for S7Comm, etc.)
            }
        }
    }

    // --- Protocol Handlers ---

    void handleModbusTCP(const u_char* payload, int len, const char* src, const char* dst) {
        if (len < 7) return; 

        // Modbus MBAP Header: TransID(2), ProtoID(2), Len(2), UnitID(1), FuncCode(1)
        uint16_t proto_id = (payload[2] << 8) | payload[3];
        uint8_t func_code = payload[7];

        if (proto_id != 0) return; 

        std::string action;
        // Basic Function Code Mapping
        switch(func_code) {
            case 1: action = "Read Coils (0x01)"; break;
            case 3: action = "Read Holding Registers (0x03)"; break;
            case 5: action = "Write Single Coil (0x05) [CONTROL ACTION]"; break;
            case 6: action = "Write Single Register (0x06) [CONTROL ACTION]"; break;
            case 15: action = "Write Multiple Coils (0x0F) [CONTROL ACTION]"; break;
            case 16: action = "Write Multiple Registers (0x10) [CONTROL ACTION]"; break;
            default: action = "Unknown Function Code: 0x" + bytesToHex(&func_code, 1);
        }

        OTEvent evt;
        evt.protocol = "MODBUS TCP";
        evt.source_ip = src;
        evt.dest_ip = dst;
        evt.command_type = action;
        evt.raw_hex = bytesToHex(payload, len > 16 ? 16 : len); 

        sendToAgentBrain(evt);
    }

    // Generic TCP Payload handler for the attack traffic
    void handleTCPPayload(const u_char* payload, int len, const char* src_ip, const char* dst_ip, 
                          uint16_t src_port, uint16_t dst_port, const std::string& description) {

        OTEvent evt;
        evt.protocol = description;
        evt.source_ip = std::string(src_ip) + ":" + std::to_string(src_port);
        evt.dest_ip = std::string(dst_ip) + ":" + std::to_string(dst_port);

        // Check for common attack signatures (simplified check for "GET")
        if (len > 3 && payload[0] == 'G' && payload[1] == 'E' && payload[2] == 'T') {
            evt.command_type = "Potential HTTP Attack (GET Request)";
        } else {
            evt.command_type = "Generic TCP Payload";
        }
        
        evt.raw_hex = bytesToHex(payload, len > 16 ? 16 : len);

        sendToAgentBrain(evt);
    }
    
    // Placeholder for other handlers to satisfy compilation (though not called in this test)
    void handleS7Comm(const u_char* payload, int len, const char* src, const char* dst) {}


    // --- Helper Functions ---

    void sendToAgentBrain(const OTEvent& evt) {
        // Output JSON-like structure to stdout
        std::cout << "{" << std::endl;
        std::cout << " \"module\": \"INGESTION_AGENT\"," << std::endl;
        std::cout << " \"protocol\": \"" << evt.protocol << "\"," << std::endl;
        // NOTE: Appending ports to match the Python log structure for easier comparison
        std::cout << " \"src\": \"" << evt.source_ip << "\"," << std::endl; 
        std::cout << " \"dst\": \"" << evt.dest_ip << "\"," << std::endl; 
        std::cout << " \"type\": \"" << evt.command_type << "\"," << std::endl;
        std::cout << " \"payload_snippet\": \"" << evt.raw_hex << "\"" << std::endl;
        std::cout << "}" << std::endl;
        std::cout << "------------------------------------------------" << std::endl;
        std::cout.flush(); // Crucial to ensure immediate pipe output
    }

    std::string bytesToHex(const u_char* bytes, int len) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for(int i = 0; i < len; ++i) {
            ss << std::setw(2) << (int)bytes[i] << " ";
        }
        return ss.str();
    }
};

int main(int argc, char* argv[]) {
    pcap_if_t* alldevs;
    pcap_if_t* d;
    int i = 0;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t* selected_dev = nullptr;
    std::string target_interface_name;

    // Retrieve the list of available network interfaces
    if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1) {
        fprintf(stderr, "Error in pcap_findalldevs_ex: %s\n", errbuf);
        return 1;
    }

    if (argc > 1) {
        // --- Option 1: Command Line Argument provided (e.g., "Loopback" or the full name) ---
        target_interface_name = argv[1];
        printf("[*] Looking for interface matching '%s'...\n", target_interface_name.c_str());

        for (d = alldevs; d; d = d->next) {
            // Check if the name matches exactly, or if the description contains the argument 
            if (strcmp(d->name, target_interface_name.c_str()) == 0) {
                selected_dev = d;
                break;
            }
            if (d->description && strstr(d->description, target_interface_name.c_str()) != NULL) {
                selected_dev = d;
                break;
            }
        }
        
        if (selected_dev == nullptr) {
            printf("\n[ERROR] Interface '%s' not found. Falling back to interactive selection.\n", target_interface_name.c_str());
        }
    } 
    
    // --- Option 2: Interactive Selection (Fallback or Default if no argument given) ---
    if (selected_dev == nullptr) {
        printf("Available Interfaces:\n");
        // Re-enumerate interfaces for the user (i is currently 0)
        for (d = alldevs; d; d = d->next) {
            printf("%d. %s", ++i, d->name);
            if (d->description) {
                printf(" (%s)\n", d->description);
            } else {
                printf(" (No description available)\n");
            }
        }

        if (i == 0) {
            printf("\nNo interfaces found! Make sure Npcap is installed.\n");
            pcap_freealldevs(alldevs);
            return 1;
        }

        // Prompt user for interface number
        int inum;
        printf("Enter the interface number (1-%d): ", i);
        if (scanf_s("%d", &inum) != 1) { 
            printf("\nInvalid input.\n");
            pcap_freealldevs(alldevs);
            return 1;
        }

        if (inum < 1 || inum > i) {
            printf("\nInterface number out of range.\n");
            pcap_freealldevs(alldevs);
            return 1;
        }

        // Jump to the selected interface
        for (d = alldevs, i = 0; i < inum - 1; d = d->next, i++);
        selected_dev = d;
    }

    // Check if we have a device to open
    if (selected_dev == nullptr) {
        printf("\nFailed to select a valid interface.\n");
        pcap_freealldevs(alldevs);
        return 1;
    }
    
    // Open the selected interface for sniffing
    pcap_t* pcap_handle = pcap_open_live(selected_dev->name, 65536, 1, 20, errbuf); 

    if (pcap_handle == NULL) {
        fprintf(stderr, "\nError opening adapter %s: %s\n", selected_dev->name, errbuf);
        pcap_freealldevs(alldevs);
        return 1;
    }
    
    printf("\n[*] OTAgent: Listening on Network adapter '%s'...\n", selected_dev->description ? selected_dev->description : selected_dev->name);
    
    // Start the ingestion process
    OTAgentIngestor agent(pcap_handle);
    agent.start();

    // Cleanup
    pcap_close(pcap_handle);
    pcap_freealldevs(alldevs);

    return 0;
}
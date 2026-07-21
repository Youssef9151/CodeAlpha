from scapy.all import AsyncSniffer, IP, TCP, UDP, ICMP, Raw, wrpcap

captured_packets = []

def packet_analyzer(packet):
    try:
        if IP in packet:
            source_ip = packet[IP].src
            destination_ip = packet[IP].dst
            
            protocol = "Unknown"
            if TCP in packet:
                protocol = "TCP"
            elif UDP in packet:
                protocol = "UDP"
            elif ICMP in packet:
                protocol = "ICMP"
            
            print(f"\n[+] Packet Captured: {source_ip} --> {destination_ip} | Protocol: {protocol}")
            
            # Process Payload
            if packet.haslayer(Raw):
                payload = packet[Raw].load
                try:
                    decoded_payload = payload.decode('utf-8', 'ignore')
                    if decoded_payload.strip():
                        print(f"    Payload (Text): {decoded_payload.strip()}")
                except:
                    print(f"    Payload (Bytes): {payload}")
            
            # Add to list
            captured_packets.append(packet)
            
    except Exception as e:
        print(f"[-] Error processing packet: {e}")

def main():
    sniffer = AsyncSniffer(iface="Wi-Fi", filter="tcp port 80", prn=packet_analyzer)
    sniffer.start()
    
    input("\n[*] Sniffer is running... Press [ENTER] stop and save.\n")
    
    sniffer.stop()
    print("\n[!] Sniffer stopped by user.")

    if captured_packets:
        print(f"[+] Saving {len(captured_packets)} packets to 'capture.pcap'...")
        wrpcap('capture.pcap', captured_packets)
        print("[+] Packets saved successfully. You can open this file in Wireshark.")
    else:
        print("[-] No packets were captured.")

if __name__ == "__main__":
    main()
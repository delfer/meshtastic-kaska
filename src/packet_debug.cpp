#include "packet_debug.h"

void printPacketInsight(uint8_t* buffer, size_t len, SX1276& radio) {
    Serial.println(F("\n--- [Meshtastic Packet] ---"));

    if (len >= 16) {
        // Offset 0x00: Destination (4 bytes, Little-endian) - Unique NodeID of the recipient
        uint32_t dest   = (uint32_t)buffer[0] | (uint32_t)buffer[1] << 8 | (uint32_t)buffer[2] << 16 | (uint32_t)buffer[3] << 24;
        // Offset 0x04: Sender (4 bytes, Little-endian) - Unique NodeID of the sender
        uint32_t sender = (uint32_t)buffer[4] | (uint32_t)buffer[5] << 8 | (uint32_t)buffer[6] << 16 | (uint32_t)buffer[7] << 24;
        // Offset 0x08: Packet ID (4 bytes, Little-endian) - Unique ID for this packet from this sender
        uint32_t packetId = (uint32_t)buffer[8] | (uint32_t)buffer[9] << 8 | (uint32_t)buffer[10] << 16 | (uint32_t)buffer[11] << 24;
        
        // Offset 0x0C: Flags (1 byte)
        uint8_t flags   = buffer[12];
        uint8_t hopLimit = flags & 0x07;           // Bits 0-2: Remaining number of hops
        bool wantAck     = (flags >> 3) & 0x01;    // Bit 3: Whether the sender wants an acknowledgement
        bool viaMqtt     = (flags >> 4) & 0x01;    // Bit 4: Set if packet was sent via MQTT
        uint8_t hopStart = (flags >> 5) & 0x07;    // Bits 5-7: Original hop limit when packet was sent

        // Offset 0x0D: Channel hash (1 byte) - Used by receiver to identify the correct decryption key
        uint8_t chanHash = buffer[13];

        // Offset 0x0E: Next-hop (1 byte) - Used for source routing
        uint8_t nextHop = buffer[14];

        // Offset 0x0F: Relay node (1 byte) - The NodeID of the node that actually transmitted this packet
        uint8_t relayNode = buffer[15];

        Serial.print(F("Sender ID:    0x")); Serial.println(sender, HEX);
        Serial.print(F("Dest ID:      0x")); Serial.print(dest, HEX);
        if (dest == 0xFFFFFFFF) Serial.println(F(" (Broadcast)")); else Serial.println();
        
        Serial.print(F("Packet ID:    0x")); Serial.println(packetId, HEX);

        Serial.print(F("Hop Limit:    ")); Serial.println(hopLimit);
        Serial.print(F("Hop Start:    ")); Serial.println(hopStart);
        Serial.print(F("Want ACK:     ")); Serial.println(wantAck ? F("Yes") : F("No"));
        Serial.print(F("Via MQTT:     ")); Serial.println(viaMqtt ? F("Yes") : F("No"));
        Serial.print(F("Chan Hash:    0x")); Serial.println(chanHash, HEX);
        Serial.print(F("Next Hop:     0x")); Serial.println(nextHop, HEX);
        Serial.print(F("Relay Node:   0x")); Serial.println(relayNode, HEX);

        // Отладочные данные железа
        Serial.print(F("Freq Error:   ")); Serial.print(radio.getFrequencyError()); Serial.println(F(" Hz"));
        Serial.print(F("Payload Size: ")); Serial.print(len - 16); Serial.println(F(" bytes"));
        
        // Дамп первых 8 байт полезной нагрузки
        Serial.print(F("Payload Hex:  "));
        for(int i = 16; i < min((int)len, 24); i++) {
            if(buffer[i] < 0x10) Serial.print('0');
            Serial.print(buffer[i], HEX);
            Serial.print(' ');
        }
        if (len > 24) Serial.println(F("...")); else Serial.println();
    }

    Serial.print(F("RSSI/SNR:    ")); Serial.print(radio.getRSSI()); 
    Serial.print(F(" / ")); Serial.println(radio.getSNR());
}

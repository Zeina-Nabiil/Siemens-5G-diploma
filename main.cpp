#include <iostream>
#include <cstdint>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <random>  // For generating random payload sizes

using namespace std;

class EthernetPacket {
public:
    static const uint64_t PreambleAndSFD = 0xFB555555555555D5; // 8 bytes (64 bits)
    uint64_t DestAddress;  // 6 bytes (48 bits)
    uint64_t SourceAddress; // 6 bytes (48 bits)
    static const uint16_t EtherType = 0xAEFE;  // 2 bytes (16 bits), eCPRI Protocol
    std::vector<uint8_t> Payload;  // varies from 46 to 1474 bytes
    uint32_t FCS;  // 4 bytes (32 bits)
    const int MaxPayloadSize = 1474;
    const int MinPayloadSize = 46;

    // Generate the payload with the size ensuring it's within valid bounds
    void GeneratePayload(size_t PayloadSize)
    {
        if (PayloadSize < MinPayloadSize)
        {
            PayloadSize = MinPayloadSize; // Payload = 46 bytes
        }
        else if (PayloadSize > MaxPayloadSize)
        {
            PayloadSize = MaxPayloadSize; // Payload = 1474 bytes
        }

        Payload = std::vector<uint8_t>(PayloadSize, 0);  // Dummy data

    }

    // Compute CRC32 for the given frame (without FCS)
    uint32_t ComputeCRC32(const std::vector<uint8_t>& frame) {
        uint32_t crc = 0xFFFFFFFF;
        for (uint8_t byte : frame) {
            crc ^= (uint32_t)byte;
            for (int i = 0; i < 8; ++i) {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0x04C11DB7;
                else
                    crc >>= 1;
            }
        }
        return ~crc;  // Finalize the CRC by inverting all bits
    }

   // Function to generate the full Ethernet packets
    std::vector<uint8_t> GenerateEthernetPackets()
    {
          std::vector<uint8_t> Packet;

          // 1. Add Preamble and SFD (8 bytes)
          uint64_t preambleandSFD = PreambleAndSFD;
          for (int i = 7; i >= 0; --i)
            {
              Packet.push_back((preambleandSFD >> (i * 8)) & 0xFF); //shift right by i bytes
            }

          // 2. Add Destination MAC Address (6 bytes)
          for (int i = 5; i >= 0; --i)
            {
              Packet.push_back((DestAddress >> (i * 8)) & 0xFF);
            }

          // 3. Add Source MAC Address (6 bytes)
          for (int i = 5; i >= 0; --i)
            {
              Packet.push_back((SourceAddress >> (i * 8)) & 0xFF);
            }

          // 4. Add EtherType (2 bytes)
          uint16_t etherType = EtherType;
          for (int i = 1; i >= 0; --i)
            {
              Packet.push_back((etherType >> (i * 8)) & 0xFF);
            }

          // 5. Add Payload
          Packet.insert(Packet.end(),Payload.begin(),Payload.end());

          // 6. Compute FCS (without including the FCS itself in the frame)
          FCS = ComputeCRC32(Packet);

          // 7. Add FCS (4 bytes)
          for (int i = 3; i >= 0; --i)
            {
              Packet.push_back((FCS >> (i * 8)) & 0xFF);
            }

    return Packet;
   }

    // Add Inter-Frame Gap (IFG)
    void AddIFG(std::vector<uint8_t>& Packet)
    {
        const uint8_t IFG = 0x07 ;
        while (Packet.size() % 4 != 0)
         {
            Packet.push_back(IFG);  // Add one-byte IFG (0x07) to make it 4-byte aligned
         }
    }
};

// Helper function to convert string MAC address to uint64_t
uint64_t macAddressToUInt64(const std::string& mac)
{
    uint64_t macAddress = 0;
    std::istringstream iss(mac);
    std::string byteStr;
    while (std::getline(iss, byteStr, ':')) {
        macAddress = (macAddress << 8) + std::stoul(byteStr, nullptr, 16);
    }
    return macAddress;
}

int main()
{
    // Declare variables to store the values in the setup file
    int LineRate , CaptureSizeMs , MinNumOfIFGsPerPacket, MaxPacketSize , BurstSize, BurstPeriodicity;
    std::string Dest_Address , Source_Address;

    // Open the Setup File
    ifstream SetupFile;
    SetupFile.open("/Users/zeina/Desktop/Project/SetupFile.txt");

    if(!SetupFile)
    {
       cerr <<"Error opening the Setup File."<<endl;
       return 1;
    }

    std::string line;
    while(std::getline(SetupFile,line))
        {
           std::istringstream obj(line);
           std::string key , value;
            if (std::getline(obj, key, '=') && std::getline(obj, value))
             {
            try {
                if (key == "Eth.LineRate") {
                    LineRate = std::stoi(value);
                }
                else if (key == "Eth.CaptureSizeMs") {
                    CaptureSizeMs = std::stoi(value);
                }
                else if (key == "Eth.MinNumOfIFGsPerPacket") {
                    MinNumOfIFGsPerPacket = std::stoi(value);
                }
                else if (key == "Eth.DestAddress") {
                    Dest_Address = value;
                }
                else if (key == "Eth.SourceAddress") {
                    Source_Address = value;
                }
                else if (key == "Eth.MaxPacketSize") {
                    MaxPacketSize = std::stoi(value);
                }
                else if (key == "Eth.BurstSize") {
                    BurstSize = std::stoi(value);
                }
                else if (key == "Eth.BurstPeriodicity_us") {
                    BurstPeriodicity = std::stoi(value);
                }
            }
            catch (const std::invalid_argument& e) {
                std::cerr << "Invalid value for " << key << ": " << value << std::endl;
                return 1;
            }
            catch (const std::out_of_range& e) {
                std::cerr << "Value out of range for " << key << ": " << value << std::endl;
                return 1;
            }
           }
        }

    SetupFile.close();

    // Convert MAC addresses from strings to uint64_t format
    EthernetPacket packet;
    packet.DestAddress = macAddressToUInt64(Dest_Address);
    packet.SourceAddress = macAddressToUInt64(Source_Address);

    // Timing Calculations (Taking into account IFG)
    // step (1) : Transmission time for one packet
    double T_packet = ((MaxPacketSize + (MinNumOfIFGsPerPacket*1)) * 8) / (LineRate * pow(10.0,9.0)) ;   // in our case T_packet = 1.2 microseconds
    cout<<"T_packet = "<<T_packet<<endl;
    // step (2) : Time for one burst
    double T_burst = T_packet * BurstSize ;   // T_burst = 120 microseconds
    cout<<"T_burst = "<<T_burst<<endl;
    // step (3) : Total cycle time including burst periodicity
    double T_cycle = T_burst + (BurstPeriodicity / pow(10.0,6.0)) ;  // Total cycle time including IFGs
    cout<<"T_cycle = "<<T_cycle<<endl;
    // step (4) : Total number of bursts within 20ms
    long long N_total_bursts = (CaptureSizeMs /pow(10.0,3.0)) / T_cycle ;
    cout<<"N_total_bursts = "<<N_total_bursts<<endl;
    // step (5) : Total number of packets sent in 20 ms
    long long N_total_packets = BurstSize * N_total_bursts ;
    cout<<"N_total_packets = "<<N_total_packets<<endl;
    // step (6) : the no. of IFGs that can be sent during Burst Periodicity
    int N_IFGs = ((BurstPeriodicity / pow(10.0,6.0)) * (LineRate* pow(10.0,9.0)) )/ 8 ;
    cout<<"N_IFGs = "<<N_IFGs<<endl;

    // Open the output file to write the frames
    ofstream OutputFile;
    OutputFile.open("/Users/zeina/Desktop/Project/OutputPackets.txt");

    if (!OutputFile) {
        cerr << "Error opening the output file." << endl;
        return 1;
    }

    cout << "Generating Packets..." <<endl;

    // Generate 100 packets for each burst
    for(int i = 0; i< N_total_bursts ; i++ )
    {
        cout<<"Burst "<< i <<":"<<endl;
        for(int j = 0; j < BurstSize ; j++)
        {
           // Generate the payload
           // randomize payload sizes
           size_t payloadSize = rand () % 2000;

           cout << "Generating packet with payload size: " << payloadSize << " bytes" <<endl;
           packet.GeneratePayload(payloadSize);

           // Generate the Ethernet packet
           std::vector<uint8_t> ethernetPacket = packet.GenerateEthernetPackets();

           // Add IFG after each packet
           packet.AddIFG(ethernetPacket);

           // Output each generated packet to the file
           OutputFile << "\nEthernet Packet " << j + 1 << ": "<<endl;


           int byteCounter = 0;

           for (uint8_t byte : ethernetPacket)
            {
              OutputFile << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
              byteCounter++;

              if (byteCounter % 4 == 0)
                {
                  OutputFile << "\n";
                }
            }


         }
          // Add IFGs to be sent during Burst periodicity
          OutputFile <<"\nSending IFGs during Burst Periodicity...."<<endl;
          for(int k = 0 ; k < N_IFGs ; k++)
          {
            OutputFile << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(0x07) << " ";
            if ((k + 1) % 4 == 0)
             {
                OutputFile << "\n";
             }
          }

           OutputFile << endl;
    }

    OutputFile.close();
    cout << "Packets successfully written to OutputPackets.txt." <<endl;

    return 0;
}









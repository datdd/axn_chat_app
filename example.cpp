#include <iostream>
#include <arpa/inet.h> // Required for ntohl

int num_to_bytes(int num) {
    unsigned char* bytePtr = reinterpret_cast<unsigned char*>(&num);

    std::cout << "Bytes: ";
    for (size_t i = 0; i < sizeof(int); ++i) {
        std::cout << static_cast<int>(bytePtr[i]) << " ";
    }
    std::cout << std::endl;

    return 0;
}

int main() {
    uint32_t netValue = htonl(1); // Convert to network byte order
    uint32_t hostValue = ntohl(netValue);  // Convert back to host byte order

    std::cout << "Display Host Value in Host Order: " << std::hex << hostValue << std::endl;
    num_to_bytes(hostValue);
    std::cout << "Display Host Value in Network Order: " << std::hex << netValue << std::endl;
    num_to_bytes(netValue);

    return 0;
}

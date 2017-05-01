# CSC_573_Project_2
Seth Butler - scbutle2
Akshit Patel - aapatel8

To run this program use the the predefined commands provided in the project document.

## Compile time constants
### Client:
* BUFFER_SIZE - this defines the maxmium buffer size being used at the server
* TIMEOUT - the number of seconds before unacknownledged packets are resent

### Server:
* BUFFER_SIZE - this defines the maxmium buffer size being used to received packets
* ACK_DGRAM_SIZE - the size of a acknowledgement packet
* MAX_TIMES_FAIL - the number of packets that must fail checksum and sequence number verification before the last sent ACK is resent. Currently I am setting this to be 2x the window size being used by the client to prevent clogging the network

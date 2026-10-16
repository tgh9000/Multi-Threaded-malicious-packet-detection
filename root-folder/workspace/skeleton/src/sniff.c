
#include "sniff.h"

#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <netinet/if_ether.h>
#include <netinet/ether.h>
#include <netinet/ip.h> 
#include <string.h>
#include "dispatch.h"
#include <signal.h>
#include <unistd.h>
#include "globals.h"
#include "dispatch.h"
pcap_t *pcap_handle = NULL;

//used for handling the end of code
volatile sig_atomic_t got_sigint = 0;
void handle_sigint(int sig) 
{
	got_sigint = 1; // only set flag (safe)
	if (pcap_handle != NULL)
	{
        pcap_breakloop(pcap_handle); // break the pcap loop if running
	}
}


// prints the final results of the code when the user wishes to terminate the program.
void print_analysis()
{
	printf("\n\nIntrusion Detection Report:\n");
	printf("%d SYN packets detected from %d different IPs (syn attack)\n", synNum,treeSize);
	printf("%d ARP responses (cache poisoning) \n", arpNum);
	printf("%d URL Blacklist violations (%d google %d facebook)\n", urlNum,googleNum,faccebookNum);
	printf("\n\n");
	return;
}


// this is the callback function
void packet_handler(unsigned char *user,const struct pcap_pkthdr *header,const unsigned char *data)
{

	int verbose = *(int *)user;
	// sends packet data over to dispatch
	dispatch((struct pcap_pkthdr *)header,(uint8_t *)data,verbose);
}


// starts sniffind the packets
void sniff(char *interface, int verbose) 
{

	signal(SIGINT, handle_sigint);

	// creates all the threads from the function in dispatch.c
	dispatchInit();


	// creates a character array so that if an error occurs it can be written here.
	char errbuf[PCAP_ERRBUF_SIZE];
	// Open interface for live capture
	// Open the specified network interface for packet capture. pcap_open_live() returns the handle to be used for the packet
	// capturing session. check the man page of pcap_open_live()
	pcap_handle = pcap_open_live(interface, 4096, 1, 1000, errbuf);
	if (pcap_handle == NULL) 
	{
		fprintf(stderr, "Unable to open interface %s\n", errbuf);
		exit(EXIT_FAILURE);
	} 
	else 
	{
		printf("SUCCESS! Opened %s for capture\n", interface);
	}


	//struct pcap_pkthdr header;


	//const unsigned char *packet;  
	// pcap_loop takes a pointer to user data (here: verbose)
	int user_verbose = verbose;

	// -1 means "capture packets forever"
	//pcap_handle is the current interface
	// packet_handler called once per packet
	if (pcap_loop(pcap_handle, -1, packet_handler, (unsigned char *)&user_verbose) == -1) 
	{
		fprintf(stderr, "Error running pcap_loop: %s\n", pcap_geterr(pcap_handle));
		pcap_close(pcap_handle);
		exit(EXIT_FAILURE);
	}


	// clean up the thread pool and clean up memory
	dispatchShutdown();

	if (got_sigint == 1) 
	{
		print_analysis();  
		return;            
	}
	//closes the interface for live capture
	pcap_close(pcap_handle);
}



// Utility/Debugging method for dumping raw packet data
void dump(const unsigned char *data, int length) 
{
	unsigned int i;
	static unsigned long pcount = 0;
	// Decode Packet Header
	struct ether_header * eth_header = (struct ether_header *) data;
	printf("\n\n === PACKET %ld HEADER ===", pcount);
	printf("\nSource MAC: ");
	for (i = 0; i < 6; ++i) {
		printf("%02x", eth_header->ether_shost[i]);
		if (i < 5) {
		printf(":");
		}
	}
	printf("\nDestination MAC: ");
	for (i = 0; i < 6; ++i) {
		printf("%02x", eth_header->ether_dhost[i]);
		if (i < 5) {
		printf(":");
		}
	}
	printf("\nType: %hu\n", eth_header->ether_type);
	printf(" === PACKET %ld DATA == \n", pcount);
	// Decode Packet Data (Skipping over the header)
	int data_bytes = length - ETH_HLEN;
	const unsigned char *payload = data + ETH_HLEN;
	const static int output_sz = 20; // Output this many bytes at a time
	while (data_bytes > 0) {
		int output_bytes = data_bytes < output_sz ? data_bytes : output_sz;
		// Print data in raw hexadecimal form
		for (i = 0; i < output_sz; ++i) {
		if (i < output_bytes) {
			printf("%02x ", payload[i]);
		} else {
			printf ("   "); // Maintain padding for partial lines
		}
		}
		printf ("| ");
		// Print data in ascii form
		for (i = 0; i < output_bytes; ++i) {
		char byte = payload[i];
		if (byte > 31 && byte < 127) {
			// Byte is in printable ascii range
			printf("%c", byte);
		} else {
			printf(".");
		}
		}
		printf("\n");
		payload += output_bytes;
		data_bytes -= output_bytes;
	}
	pcount++;
}

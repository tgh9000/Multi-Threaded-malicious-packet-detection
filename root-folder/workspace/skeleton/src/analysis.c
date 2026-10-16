#include "analysis.h"


#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
//#include <netinet/ether.h>
#include <string.h>
#include <netinet/in.h>
#include "globals.h"
#include <pthread.h>





pthread_mutex_t arpLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t synLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t URLLock = PTHREAD_MUTEX_INITIALIZER;

//debug function for when a malicious URL is found
void badURL(char* company,struct in_addr* src_ip, struct in_addr* dst_ip)
{
    printf("\n================================ \n");
    printf("Blacklisted URL violation detected\n");
    printf("Source IP address: %s\n", inet_ntoa(*src_ip));
    printf("Destination IP address: %s ( %s )", inet_ntoa(*dst_ip),company);
    printf("\n================================ \n");
}

//analyses the content within the packet and updates the global variables depending on the type or contents.
void analyse(struct pcap_pkthdr *header,const unsigned char *data,int verbose ) 
{
    // deal with the packet here
    struct ether_header * eth_header = (struct ether_header *) data;
    
    // reads the EtherType field from the ethernet header and parses it into eth_type
    unsigned short ethType;
    ethType = ntohs(eth_header->ether_type);

    // creates a pointer for the start of the ip or arp header 
    const unsigned char * ip_data = data + ETH_HLEN;



    //==========================================================
    // parsing all the ARP header information.
    //==========================================================


    if(ethType == 0x0806) // this means its an ARP packet
    {
        // creates the struct to parse ARP data at the beginning of the header.
        struct  ether_arp* arphead = (struct ether_arp*) ip_data;
        //parses the the operation into a variable -  this can either be 1 for request or 2 for reply 
        uint16_t operation = ntohs(arphead->ea_hdr.ar_op);
        if(operation == 2)// this checks that the operation is 2 and therefore an ARP response.
        {
            //locks the global variable arpNum so that it can only be changed by the current process.
            pthread_mutex_lock(&arpLock);
            arpNum = arpNum + 1;
            // update global statistics here
            pthread_mutex_unlock(&arpLock);
            // unlock the variable so that it can be used by a different process.
            

        }
    }
    else if (ethType == 0x0800) // this means its an ipv4 packet
    {
        
        //==========================================================
        // parsing all the IP header information that we will need.s
        //==========================================================

        // creates a new pointer to the beginning of the ip header of the data
        struct  iphdr* ipHead = (struct iphdr *) (data + ETH_HLEN);



        // parse the source ip address into a new variable
        struct in_addr src_ip;
        src_ip.s_addr = ipHead->saddr;

        // parse the destination ip address into a new variable
        struct in_addr dst_ip;
        dst_ip.s_addr = ipHead->daddr; 

        //===========================
        // TCP header stuff goes here
        //===========================

        // need to count if it is a syn pack and if so calculate the set of independant ip addresses and total ip addresses currently been detected.
        const unsigned char * tcp_data = ip_data + (ipHead -> ihl *4);
        // creates a ponter to the beginning of the tcp header.
        struct tcphdr* tcpHead = (struct tcphdr *) tcp_data; 
        
        // find the port number here
        //uint16_t src_port = ntohs(tcpHead->th_sport);  // Source port
        uint16_t destPort = ntohs(tcpHead->th_dport); // destination port
        
        if(destPort == 80)
        {
            //========================
            // check for a bad address
            //========================
            // find the size of the tcp header so that i can navigate to the payload
            unsigned short tcp_header_size = (tcpHead->th_off) * 4;
            const unsigned char* httpsPayload = tcp_data + tcp_header_size;
            

            ////////////////////////////////////////////////////////////////
            // converts the binary pointer of https to a string pointer so that the string itself can be parsed
            char payload_str[1500];
            
            //int payload_len = header->caplen - (tcp_data - data);
            int payload_len = header->caplen - (httpsPayload - data);
            // If the value is invalid or negative, stop.
            if (payload_len <= 0)
            {
                return;
            }
            // Clamp the payload length so we don't overflow payload_str.
            if (payload_len > sizeof(payload_str) - 1)
            {
                payload_len = sizeof(payload_str) - 1;
            }
            // copies the whole https payload as a string and copies it into another variable so that the actual host name can be parsed later
            memcpy(payload_str, httpsPayload, payload_len);
            // sets the last value of the string to the \0 so that C knows its a string.
            payload_str[ payload_len ] = '\0';
            

            // Function that finds the first occurance of the substring "Host: " withing the https payload.
            char *host_header = strstr(payload_str, "Host: ");
            
            // if there exists a host header 
            if (host_header) 
            {
                // Move pointer past "Host: "
                host_header += 6;  
                // Look for the end of the Host header
                char *end_of_host = strstr(host_header, "\r\n");
                
                // if it exitst which it should if its a well formatted HTTPS packet
                if (end_of_host) 
                {
                    // Sets the end of the https line to be the end of the string so that it can be manipulated like a regular string. 
                    *end_of_host = '\0'; 

                    // checks the actual link is google
                    if(strcmp(host_header, "www.google.co.uk") == 0) //  google condition
                    {
                        pthread_mutex_lock(&URLLock);

                        // update global statistics here
                        googleNum = googleNum + 1;
                        urlNum = urlNum + 1;
                        badURL("google",&src_ip, &dst_ip);


                        pthread_mutex_unlock(&URLLock);



                    }
                    // checks the actual link is facebook
                    else if (strcmp(host_header, "www.facebook.com") == 0) //  facebook condition
                    {
                        pthread_mutex_lock(&URLLock);


                        //printf("Found facebook blacklisted URL \n");
                        faccebookNum = faccebookNum + 1;
                        urlNum = urlNum + 1;
                        badURL("facebook",&src_ip, &dst_ip);

                        pthread_mutex_unlock(&URLLock);
                    }

                }
            }
            
                
        }
        
        
        //==============================
        // Parses all SYN packet content
        //==============================

        if((tcpHead->th_flags & TH_SYN) && !(tcpHead->th_flags & TH_ACK))//checks its a syn packet from the user not the server 
        {
            // locks all global variables to do with SYN packet detection
            pthread_mutex_lock(&synLock);
            synNum = synNum + 1;
            // tries to add the source ip address to the tree if it is not already in the tree.
            tree = insert_unique(tree,src_ip);
            //unlocks all the SYN related variables.
            pthread_mutex_unlock(&synLock);
            

            
        }
        

         





        
    }


    

    
}

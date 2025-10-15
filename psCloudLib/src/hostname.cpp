#include "hostname.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#endif



int get_host_name(char *buf, unsigned long buflen){
    #ifdef _WIN32
        //unsigned long len;
        // Get and display the name of the computer.
        // if( !GetComputerNameA(buf, &len) ){
        if( !GetComputerNameExA(ComputerNameDnsHostname, buf, &buflen) ){
            std::cout << "ERROR: Could not get Computer Name\n";
            return -1;
        }
        std::cout << "Name Length: " << buflen << std::endl;
        std::cout << "Computer name: " << buf << std::endl;
    #elif __linux__
        if(gethostname(buf, (unsigned long long)buflen) != 0){
            std::cout << "ERROR: Could not get Computer Name\n";
            return -1;
        }
        std::cout << "Computer name: " << buf << std::endl;
    #else
        std::cout << "Unknown Operating System\n"
    #endif
    return 1;
}

#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>

using namespace std;

// Source of all code in this file is lecture materials from this class

int main(int argc, char **argv) {
    // Validate arguments
    if (argc <= 1) {
        exit(0);
    } 

    for (int i = 1; i < argc; i++) {
        // Open the file and check validity
        int fileDescriptor = open(argv[i], O_RDONLY);
        if (fileDescriptor < 0) {
            stringstream stringOut;
            stringOut << "wcat: cannot open file" << endl;
            string str = stringOut.str();
            write(STDOUT_FILENO, str.c_str(), str.length()); // length of string = bytes (char = 8bit)
            exit(1);
        } else if (fileDescriptor == -1) {
            cerr << "Error calling open()" << endl;
            exit(1);
        }

        // Load file data into a buffer. Print as it is being read
        int ret = 0;
        int bytesRead = 0;
        char buffer[4096];
        while ((ret = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
            bytesRead = ret;
            write(STDOUT_FILENO, buffer, bytesRead); // sizeof buffer is in bytes
        }

        if (fileDescriptor != STDIN_FILENO) {
            close(fileDescriptor);
        }
    }
    
    return 0;
}
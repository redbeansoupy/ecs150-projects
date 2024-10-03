#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>

using namespace std;

// Source of all code in this file is lecture materials from this class

void writeLine2Stdout(string msg) {
    stringstream stringOut;
    stringOut << msg << endl;
    string str = stringOut.str();
    write(STDOUT_FILENO, str.c_str(), str.length()); // length of string = bytes (char = 8bit)
}

void write5ByteBuffer(string chain) {
    uint8_t writeBuffer[5];
    // write the number and the char
    uint32_t chainLength = chain.length();

    if (chain.length() == 0) return;

    // load the 32 bit uint into the write buffer (right side = MSB = index 3)
    writeBuffer[3] = (chainLength >> 24) & 0xff;
    writeBuffer[2] = (chainLength >> 16) & 0xff;
    writeBuffer[1] = (chainLength >> 8) & 0xff;
    writeBuffer[0] = chainLength & 0xff;

    // load the char into the last position in buffer
    writeBuffer[4] = uint8_t(chain[0]);

    // write
    write(STDOUT_FILENO, writeBuffer, 5);
}

int zip(int fd, string& currentCharChain) {
    // For the whole file, read into a buffer and then parse from that buffer
    char buffer[4096];
    int ret;
    int bytesRead;
    while ((ret = read(fd, buffer, sizeof(buffer))) > 0) { // equivalent to "ret > 0"
        bytesRead = ret;
        for (int i = 0; i < bytesRead; i++) {
            char guy = buffer[i];
            // if either the length of the current chain is zero or this char is the same as the one in the chain, add
            if ((currentCharChain.length() == 0) || (currentCharChain[0] == guy)) {
                currentCharChain += guy;
            } else {
                write5ByteBuffer(currentCharChain);

                // start the char chain over again
                currentCharChain = guy;
            }
        }
    } 

    return 0;
}

int main(int argc, char **argv) {
    string currentCharChain = "";
    // Validate arguments
    if (argc <= 1) { // case that no arguments are given
        writeLine2Stdout("wzip: file1 [file2 ...]");
        exit(1);
    } else {
        // For each argument, validate fildes and run zip
        for (int i = 1; i < argc; i++) {
            int fileDescriptor = open(argv[i], O_RDONLY);

            if (fileDescriptor < 0) {
                writeLine2Stdout("wzip: file1 [file2 ...]");
                exit(1);
            } else if (fileDescriptor == -1) {
                cerr << "Error calling open()" << endl;
                exit(1);
            }
            zip(fileDescriptor, currentCharChain);
        }

        // at the end of the files, make sure to write again
        write5ByteBuffer(currentCharChain);
    }

    return 0;
}
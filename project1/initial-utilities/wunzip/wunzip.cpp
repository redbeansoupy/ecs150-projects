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

int unzip(int fd) {
    // read the contents of each file using a while loop
    int ret;
    int bytesRead;
    uint8_t buffer[4096];
    uint32_t integer_4byte = 0;
    int byteCounter = 0; // Goes from 0 to 4. Bytes 0-3 will be the integer (MSB = index 3), and byte 4 is the char
    while ((ret = read(fd, buffer, sizeof(buffer))) > 0) { // equivalent to "ret > 0"
        bytesRead = ret;
        // every 5 bytes, simply print the output to stdout
        for (int i = 0; i < bytesRead; i++) {
            if (byteCounter < 4) {
                // integer
                integer_4byte |= (buffer[i] << (byteCounter * 8)); // byteCounter = 3 for the MSB and 0 for LSB

                byteCounter++;
            } else if (byteCounter == 4) {
                // char
                string repeatingChars = "";
                // fill the string with the same char
                for (int j = 0; j < int(integer_4byte); j++) {
                    repeatingChars += buffer[i];
                }

                write(STDOUT_FILENO, repeatingChars.c_str(), integer_4byte);

                integer_4byte = 0;
                byteCounter = 0;
            } else {
                cerr << "byteCounter issue" << endl;
                exit(1);
            }
        }
    }

    
    return 0;
}

int main(int argc, char** argv) {
    // Validate arguments
    if (argc <= 1) { // case that no arguments are given
        writeLine2Stdout("wunzip: file1 [file2 ...]");
        exit(1);
    } else {
        // For each argument, validate fildes and run zip
        for (int i = 1; i < argc; i++) {
            int fileDescriptor = open(argv[i], O_RDONLY);

            if (fileDescriptor < 0) {
                writeLine2Stdout("wunzip: file1 [file2 ...]]");
                exit(1);
            } else if (fileDescriptor == -1) {
                cerr << "Error calling open()" << endl;
                exit(1);
            }
            unzip(fileDescriptor);

            if (fileDescriptor != STDIN_FILENO) {
            close(fileDescriptor);
            }
        }
    }

    return 0;
}
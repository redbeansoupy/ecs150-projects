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

int findWord(int fileDescriptor, char *searchTerm) {
    // Validate fileDescriptor
    if (fileDescriptor < 0) {
        writeLine2Stdout("wgrep: cannot open file");
        exit(1);
    } else if (fileDescriptor == -1) {
        cerr << "Error calling open()" << endl;
        exit(1);
    }

    // Parse file information
    int ret = 0;
    int bytesRead = 0;
    char buffer[4096];
    string currentLine = "";
    while ((ret = read(fileDescriptor, buffer, sizeof(buffer))) > 0) { // equivalent to "ret > 0"
        bytesRead = ret;
        
        // For each byte (character), add to the currentLine string until a newline is found
        for (int i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\n') {
                // Search the string
                int pos = currentLine.find(searchTerm); // If term is found, returns position of first char. Else, -1
                if ((pos > -1) && (pos < int(currentLine.length()))) { // If term was found
                    // write the current line to stdout
                    writeLine2Stdout(currentLine);
                }
                currentLine = "";
            } else {
                currentLine += buffer[i];
            }
        }
    }

    if (fileDescriptor != STDIN_FILENO) {
        close(fileDescriptor);
    }
    return 0;
}

int main(int argc, char **argv) {
    int fileDescriptor = 0;

    // Validate arguments
    if (argc <= 1) { // case that no arguments are given
        writeLine2Stdout("wgrep: searchterm [file ...]");
        exit(1);
    } else if (argc == 2) { // case search term: use stdin
        findWord(STDIN_FILENO, argv[1]);
    } else { // case there are files -- get file descriptors?
        for (int i = 2; i < argc; i++) {
            fileDescriptor = open(argv[i], O_RDONLY);
            findWord(fileDescriptor, argv[1]);
        }
    }

    if (fileDescriptor != STDIN_FILENO) {
            close(fileDescriptor);
        }
    return 0;
}
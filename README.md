# Basic-Text-Analysis-Server

## Overview
This is a simple text analysis server developed for an Operating Systems assignment. The server processes input words, checks for their existence in a dictionary, and provides Levenshtein distance-based suggestions for similar words. If a word does not exist in the dictionary, the user can choose to add it.

## Features
- Loads a dictionary from a file.
- Accepts client connections via sockets.
- Uses Levenshtein distance to find similar words.
- Allows users to add new words to the dictionary.
- Saves the updated dictionary to a file.
- Uses pthreads for concurrent processing.
- Supports multiple clients concurrently.

## Dependencies
- POSIX Threads (`pthread`)
- Sockets (`arpa/inet.h`)
- Standard C Libraries (`stdio.h`, `stdlib.h`, `string.h`, `unistd.h`)

## Compilation
To compile the project, use:
```sh
gcc -o text_server text_server.c -pthread
```

## Usage
1. Start the server:
```sh
./TextAnalysisServer
```
2. Clients can connect and send words to check.
3. The server provides feedback based on dictionary matches.
4. Users can add new words to the dictionary if they are missing.

## Files
- `TextAnalysisServer.c`: Main server implementation.
- `basic_english_2000.txt`: Initial dictionary file.
- `new.txt`: Updated dictionary file after modifications.

## Notes
- The server supports concurrent word processing using pthreads.
- The dictionary is dynamically allocated and managed with mutex locks.
- Users must confirm before adding new words to prevent accidental modifications.
- The server logs all received words and suggestions for debugging purposes.

## License
This project is for educational purposes and does not include a formal license.


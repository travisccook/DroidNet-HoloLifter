/**
 * CommandParser Library Implementation
 */

#include "CommandParser.h"
#include "config.h"
#include <ctype.h>

/**
 * Constructor
 */
CommandParser::CommandParser()
    : bufferIndex(0), commandReady(false), tokenCount(0) {
    memset(cmdBuffer, 0, CMD_BUFFER_SIZE);
    memset(rawLine, 0, CMD_BUFFER_SIZE);
    memset(tokens, 0, sizeof(tokens));
}

/**
 * Initialize serial communication
 */
void CommandParser::begin(uint32_t baud) {
    Serial.begin(baud);
}

/**
 * Update command parser
 */
void CommandParser::update() {
    // Check for incoming serial data
    while (Serial.available() > 0) {
        char c = Serial.read();

        // Handle newline (command complete)
        if (c == '\n' || c == '\r') {
            if (bufferIndex > 0) {
                cmdBuffer[bufferIndex] = '\0';
                parseCommand();
                commandReady = true;

#if DEBUG_COMMANDS
                DEBUG_PRINT(F("Command received: "));
                DEBUG_PRINTLN(cmdBuffer);
#endif
            }
        }
        // Handle backspace
        else if (c == '\b' || c == 127) {
            if (bufferIndex > 0) {
                bufferIndex--;
                cmdBuffer[bufferIndex] = '\0';
            }
        }
        // Add character to buffer
        else if (bufferIndex < CMD_BUFFER_SIZE - 1) {
            // Skip leading whitespace
            if (bufferIndex == 0 && (c == ' ' || c == '\t')) {
                continue;
            }
            cmdBuffer[bufferIndex++] = c;
            // Echo character back for debugging
            Serial.print(c);
        }
    }
}

/**
 * Check if command is ready
 */
bool CommandParser::available() const {
    return commandReady;
}

/**
 * Get parsed command
 */
const char* CommandParser::getCommand() const {
    if (tokenCount > 0) {
        return tokens[0];
    }
    return nullptr;
}

/**
 * Get the trimmed line in its original case
 */
const char* CommandParser::getRawLine() const {
    return rawLine;
}

/**
 * Get command argument
 */
const char* CommandParser::getArg(uint8_t index) const {
    // Arguments start at token 1 (token 0 is command)
    uint8_t tokenIndex = index + 1;
    if (tokenIndex < tokenCount) {
        return tokens[tokenIndex];
    }
    return nullptr;
}

/**
 * Get argument count
 */
uint8_t CommandParser::getArgCount() const {
    if (tokenCount > 0) {
        return tokenCount - 1;  // Don't count command itself
    }
    return 0;
}

/**
 * Get argument as integer
 */
int32_t CommandParser::getArgInt(uint8_t index, int32_t defaultValue) const {
    const char* arg = getArg(index);
    if (arg != nullptr) {
        return atol(arg);
    }
    return defaultValue;
}

/**
 * Get argument as float
 */
float CommandParser::getArgFloat(uint8_t index, float defaultValue) const {
    const char* arg = getArg(index);
    if (arg != nullptr) {
        return atof(arg);
    }
    return defaultValue;
}

/**
 * Clear current command
 */
void CommandParser::clear() {
    bufferIndex = 0;
    commandReady = false;
    tokenCount = 0;
    memset(cmdBuffer, 0, CMD_BUFFER_SIZE);
}

/**
 * Send OK response
 */
void CommandParser::sendOK(const char* message) {
    Console.print(F("OK"));
    if (message != nullptr) {
        Console.print(F(" "));
        Console.print(message);
    }
    Console.println();
}

/**
 * Send error response
 */
void CommandParser::sendError(const char* message) {
    Console.print(F("ERROR "));
    Console.println(message);
}

/**
 * Parse command into tokens
 */
void CommandParser::parseCommand() {
    tokenCount = 0;

    // Trim leading/trailing whitespace manually
    char* start = cmdBuffer;
    char* end = cmdBuffer + strlen(cmdBuffer) - 1;

    // Skip leading whitespace
    while (*start && isspace(*start)) {
        start++;
    }

    // Skip trailing whitespace
    while (end > start && isspace(*end)) {
        *end = '\0';
        end--;
    }

    // Keep the original-case line for case-sensitive arguments
    strncpy(rawLine, start, CMD_BUFFER_SIZE - 1);
    rawLine[CMD_BUFFER_SIZE - 1] = '\0';

    // Convert to lowercase for case-insensitive commands
    for (char* p = start; *p; p++) {
        *p = tolower(*p);
    }

    // Parse tokens (split by whitespace)
    char* token = start;
    bool inToken = false;

    for (char* p = start; *p && tokenCount < CMD_MAX_TOKENS; p++) {
        if (isspace(*p)) {
            if (inToken) {
                // End of token
                *p = '\0';
                strncpy(tokens[tokenCount], token, CMD_MAX_TOKEN_LEN - 1);
                tokens[tokenCount][CMD_MAX_TOKEN_LEN - 1] = '\0';
                tokenCount++;
                inToken = false;
            }
        } else {
            if (!inToken) {
                // Start of token
                token = p;
                inToken = true;
            }
        }
    }

    // Handle last token
    if (inToken && tokenCount < CMD_MAX_TOKENS) {
        strncpy(tokens[tokenCount], token, CMD_MAX_TOKEN_LEN - 1);
        tokens[tokenCount][CMD_MAX_TOKEN_LEN - 1] = '\0';
        tokenCount++;
    }

#if DEBUG_COMMANDS
    DEBUG_PRINT(F("Parsed "));
    DEBUG_PRINT(tokenCount);
    DEBUG_PRINT(F(" tokens: "));
    for (uint8_t i = 0; i < tokenCount; i++) {
        DEBUG_PRINT(tokens[i]);
        DEBUG_PRINT(F(" "));
    }
    DEBUG_PRINTLN();
#endif
}

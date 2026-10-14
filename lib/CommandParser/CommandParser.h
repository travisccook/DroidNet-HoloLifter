/**
 * CommandParser Library
 *
 * Parse serial commands and generate simple text responses.
 * Supports simple text commands with space-delimited arguments.
 *
 * Command Format:
 *   command [arg1] [arg2] ...
 *   Example: "goto 5000"
 *   Example: "speed 128"
 *   Example: "home"
 *
 * Response Format:
 *   Simple text: OK [key=value ...] or ERROR message
 */

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>
#include "config.h"

class CommandParser {
public:
    /**
     * Constructor
     */
    CommandParser();

    /**
     * Initialize serial communication
     * @param baud Baud rate (default 115200)
     */
    void begin(uint32_t baud = 115200);

    /**
     * Update command parser (check for incoming data)
     * Call this in loop()
     */
    void update();

    /**
     * Check if a command is ready to process
     * @return true if command available
     */
    bool available() const;

    /**
     * Get the parsed command (first token)
     * @return Pointer to command string (valid until next update/clear)
     */
    const char* getCommand() const;

    /**
     * Get the whole trimmed line in its original case (for commands whose
     * arguments are case-sensitive, e.g. "mesh pw ...").
     */
    const char* getRawLine() const;

    /**
     * Get command argument by index
     * @param index Argument index (0-based)
     * @return Pointer to argument string, or nullptr if not found
     */
    const char* getArg(uint8_t index) const;

    /**
     * Get number of arguments (excluding command)
     * @return Argument count
     */
    uint8_t getArgCount() const;

    /**
     * Get argument as integer
     * @param index Argument index
     * @param defaultValue Default value if not found or invalid
     * @return Argument as integer
     */
    int32_t getArgInt(uint8_t index, int32_t defaultValue = 0) const;

    /**
     * Get argument as float
     * @param index Argument index
     * @param defaultValue Default value if not found or invalid
     * @return Argument as float
     */
    float getArgFloat(uint8_t index, float defaultValue = 0.0f) const;

    /**
     * Clear current command
     */
    void clear();

    /**
     * Send simple OK response
     * @param message Optional message
     */
    void sendOK(const char* message = nullptr);

    /**
     * Send error response
     * @param message Error message
     */
    void sendError(const char* message);

private:
    char cmdBuffer[CMD_BUFFER_SIZE];              // Command buffer
    char rawLine[CMD_BUFFER_SIZE];                // Trimmed line, original case
    uint8_t bufferIndex;                          // Current buffer position
    bool commandReady;                            // Command ready flag
    char tokens[CMD_MAX_TOKENS][CMD_MAX_TOKEN_LEN];  // Parsed tokens (2D array)
    uint8_t tokenCount;                           // Number of tokens

    /**
     * Parse command into tokens
     */
    void parseCommand();
};

#endif // COMMAND_PARSER_H

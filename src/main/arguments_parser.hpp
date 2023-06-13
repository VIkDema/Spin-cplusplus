#pragma once
#include "launch_settings.hpp"

/**
 * @class ArgumentsParser
 * @brief Class responsible for parsing command-line arguments and generating launch settings.
 */
class ArgumentsParser {
public:
    /**
     * @brief Parses the command-line arguments and generates launch settings.
     * @param[in, out] argc The number of command-line arguments.
     *                     This value may be modified by the parsing process.
     * @param[in, out] argv The array of command-line arguments.
     *                     This array may be modified by the parsing process.
     * @return The generated launch settings based on the parsed command-line arguments.
     */
    LaunchSettings Parse(int& argc, char **&argv);
};

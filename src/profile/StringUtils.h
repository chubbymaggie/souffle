/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include "CellInterface.h"
#include "Row.h"
#include "Table.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace souffle {
namespace profile {

/*
 * A series of functions necessary throughout the code
 * Mostly string manipulation
 */
namespace Tools {
static const std::string arr[] = {"K", "M", "B", "t", "q", "Q", "s", "S", "o", "n", "d", "U"};
static const std::vector<std::string> abbreviations(arr, arr + sizeof(arr) / sizeof(arr[0]));

inline std::string formatNum(double amount) {
    std::stringstream ss;
    ss << amount;
    return ss.str();
}

inline std::string formatNum(int precision, int64_t amount) {
    // assumes number is < 999*10^12
    if (amount == 0) {
        return "0";
    }

    if (precision == -1) {
        return std::to_string(amount);
    }

    std::string result;

    if (amount < 1000) {
        return std::to_string(amount);
    }

    for (size_t i = 0; i < abbreviations.size(); ++i) {
        if (amount > std::pow(1000, i + 2)) {
            continue;
        }

        double r = amount / std::pow(1000, i + 1);
        result = std::to_string(r);

        if (r >= 100) {  // 1000 > result >= 100

            // not sure why anyone would do such a thing.
            switch (precision) {
                case -1:
                    break;
                case 0:
                    result = "0";
                    break;  // I guess 0 would be valid?
                case 1:
                    result = result.substr(0, 1) + "00";
                    break;
                case 2:
                    result = result.substr(0, 2) + "0";
                    break;
                case 3:
                    result = result.substr(0, 3);
                    break;
                default:
                    result = result.substr(0, precision + 1);
            }
        } else if (r >= 10) {  // 100 > result >= 10
            switch (precision) {
                case -1:
                    break;
                case 0:
                    result = "0";
                    break;  // I guess 0 precision would be valid?
                case 1:
                    result = result.substr(0, 1) + "0";
                    break;
                case 2:
                    result = result.substr(0, 2);
                    break;
                default:
                    result = result.substr(0, precision + 1);
            }
        } else {  // 10 > result > 0
            switch (precision) {
                case -1:
                    break;
                case 0:
                    result = "0";
                    break;  // I guess 0 precision would be valid?
                case 1:
                    result = result.substr(0, 1);
                    break;
                default:
                    result = result.substr(0, precision + 1);
            }
        }
        result += abbreviations.at(i);
        return result;
    }
    return nullptr;
}

inline std::string formatMemory(uint64_t kbytes) {
    if (kbytes < 1024L * 2) {
        return std::to_string(kbytes) + "kB";
    } else if (kbytes < 1024L * 1024 * 2) {
        return std::to_string(kbytes / 1024) + "MB";
    } else if (kbytes < 1024L * 1024 * 1024 * 2) {
        return std::to_string(kbytes / (1024 * 1024)) + "GB";
    }
    return std::to_string(kbytes / (1024 * 1024 * 1024)) + "TB";
}

inline std::string formatTime(std::chrono::microseconds number) {
    uint64_t sec = number.count() / 1000000;
    if (sec >= 100) {
        uint64_t min = std::floor(sec / 60);
        if (min >= 100) {
            uint64_t hours = std::floor(min / 60);
            if (hours >= 100) {
                uint64_t days = std::floor(hours / 24);
                return std::to_string(days) + "D";
            }
            return std::to_string(hours) + "h";
        }
        if (min < 10) {
            // temp should always be 1 digit long
            uint64_t temp = std::floor((sec - (min * 60.0)) * 10.0 / 6.0);
            return std::to_string(min) + "." + std::to_string(temp).substr(0, 1) + "m";
        }
        return std::to_string(min) + "m";
    } else if (sec >= 10) {
        return std::to_string(sec) + "s";
    } else if (number.count() >= 1000000) {
        std::string temp = std::to_string(number.count() / 100);
        return temp.substr(0, 1) + "." + temp.substr(1, 2) + "s";
    } else if (number.count() >= 100000) {
        std::string temp = std::to_string(number.count() / 1000);
        return "." + temp.substr(0, 3) + "s";
    } else if (number.count() >= 10000) {
        std::string temp = std::to_string(number.count() / 1000);
        return ".0" + temp.substr(0, 2) + "s";
    } else if (number.count() >= 1000) {
        std::string temp = std::to_string(number.count() / 1000);
        return ".00" + temp.substr(0, 1) + "s";
    }

    return ".000s";
}

inline std::vector<std::vector<std::string>> formatTable(Table table, int precision) {
    std::vector<std::vector<std::string>> result;
    for (auto& row : table.getRows()) {
        std::vector<std::string> result_row;
        for (auto& cell : row->getCells()) {
            if (cell != nullptr) {
                result_row.push_back(cell->toString(precision));
            } else {
                result_row.push_back("-");
            }
        }
        result.push_back(result_row);
    }
    return result;
}

inline std::vector<std::string> split(std::string str, std::string split_str) {
    // repeat value when splitting so "a   b" -> ["a","b"] not ["a","","","","b"]
    bool repeat = (split_str.compare(" ") == 0);

    std::vector<std::string> elems;

    std::string temp;
    std::string hold;
    for (size_t i = 0; i < str.size(); i++) {
        if (repeat) {
            if (str.at(i) == split_str.at(0)) {
                while (str.at(++i) == split_str.at(0))
                    ;  // set i to be at the end of the search string
                elems.push_back(temp);
                temp = "";
            }
            temp += str.at(i);
        } else {
            temp += str.at(i);
            hold += str.at(i);
            for (size_t j = 0; j < hold.size(); j++) {
                if (hold[j] != split_str[j]) {
                    hold = "";
                }
            }
            if (hold.size() == split_str.size()) {
                elems.push_back(temp.substr(0, temp.size() - hold.size()));
                hold = "";
                temp = "";
            }
        }
    }
    if (!temp.empty()) {
        elems.push_back(temp);
    }

    return elems;
}

inline std::vector<std::string> splitAtSemiColon(std::string str) {
    for (size_t i = 0; i < str.size(); i++) {
        if (i > 0 && str[i] == ';' && str[i - 1] == '\\') {
            // I'm assuming this isn't a thing that will be naturally found in souffle profiler files
            str[i - 1] = '\b';
            str.erase(i--, 1);
        }
    }
    bool changed = false;
    std::vector<std::string> result = split(str, ";");
    for (auto& i : result) {
        for (char& j : i) {
            if (j == '\b') {
                j = ';';
                changed = true;
            }
        }
        if (changed) {
            changed = false;
        }
    }

    return result;
}

inline std::string trimWhitespace(std::string str) {
    std::string whitespace = " \t";
    size_t first = str.find_first_not_of(whitespace);
    if (first != std::string::npos) {
        str.erase(0, first);
        size_t last = str.find_last_not_of(whitespace);
        str.erase(last + 1);
    } else {
        str.clear();
    }

    return str;
}

inline bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

inline std::string cleanString(std::string val) {
    if (val.size() < 2) {
        return val;
    }

    size_t start_pos = 0;
    while ((start_pos = val.find('\\', start_pos)) != std::string::npos) {
        val.erase(start_pos, 1);
        if (start_pos < val.size()) {
            if (val[start_pos] == 'n' || val[start_pos] == 't') {
                val.replace(start_pos, 1, " ");
            }
        }
    }

    if (val.at(0) == '"' && val.at(val.size() - 1) == '"') {
        val = val.substr(1, val.size() - 2);
    }

    std::replace(val.begin(), val.end(), '\n', ' ');
    std::replace(val.begin(), val.end(), '\t', ' ');

    return val;
}
inline std::string cleanJsonOut(std::string val) {
    if (val.size() < 2) {
        return val;
    }

    if (val.at(0) == '"' && val.at(val.size() - 1) == '"') {
        val = val.substr(1, val.size() - 2);
    }

    size_t start_pos = 0;
    while ((start_pos = val.find('\\', start_pos)) != std::string::npos) {
        val.replace(start_pos, 1, "\\\\");
        start_pos += 2;
    }
    start_pos = 0;
    while ((start_pos = val.find('"', start_pos)) != std::string::npos) {
        val.replace(start_pos, 1, "\\\"");
        start_pos += 2;
    }
    return val;
}
inline std::string escapeQuotes(std::string val) {
    if (val.size() < 2) {
        return val;
    }

    if (val.at(0) == '"' && val.at(val.size() - 1) == '"') {
        val = val.substr(1, val.size() - 2);
    }

    size_t start_pos = 0;
    while ((start_pos = val.find('"', start_pos)) != std::string::npos) {
        val.replace(start_pos, 1, "\\\"");
        start_pos += 2;
    }
    return val;
}
inline std::string cleanJsonOut(double val) {
    if (std::isnan(val)) {
        return "NaN";
    }
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(6) << val;
    return ss.str();
}

inline std::string stripWhitespace(std::string val) {
    size_t first = val.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = val.find_last_not_of(' ');
    return val.substr(first, (last - first + 1));
}
}  // namespace Tools

}  // namespace profile
}  // namespace souffle

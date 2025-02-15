#ifndef PARSED_INTEREST_H
#define PARSED_INTEREST_H

#include <ndn-cxx/name.hpp>
#include <string>
#include <vector>

/**
 * @brief Represents a parsed NDN Interest name with named fields.
 */
struct ParsedInterest {
  std::string prefix;              ///< Typically the first component (e.g., "/SNDS")
  std::string command;             ///< The command or type (e.g., "CAR" or "SNDS_ID_scope_X")
  std::vector<std::string> params; ///< Any additional parameters (e.g., IDs, node identifiers)
};

/**
 * @brief Parses an ndn::Name into a ParsedInterest structure.
 *
 * For example, an Interest name such as:
 *     /SNDS/CAR/car1
 * would be parsed as:
 *     prefix  = "/SNDS"
 *     command = "CAR"
 *     params  = ["car1"]
 *
 * @param name The NDN name to parse.
 * @return ParsedInterest The structure containing the parsed fields.
 */
inline ParsedInterest parseInterestName(const ndn::Name& name) {
  ParsedInterest pi;
  size_t n = name.size();
  if(n > 0) {
    pi.prefix = name.get(0).toUri();
  }
  if(n > 1) {
    pi.command = name.get(1).toUri();
  }
  for(size_t i = 2; i < n; ++i) {
    pi.params.push_back(name.get(i).toUri());
  }
  return pi;
}

#endif 

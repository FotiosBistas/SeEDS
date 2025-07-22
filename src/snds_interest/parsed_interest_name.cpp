#include "parsed_interest_name.h"

ParsedInterestName parseInterestName(const ndn::Name& name) {
    ParsedInterestName pi;
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

ndn::Name toName(const ParsedInterestName& pi) {
    ndn::Name name;

    // Handle the prefix (supports multi-component prefixes like "/SNDS/service")
    if (!pi.prefix.empty()) {
        ndn::Name prefixName(pi.prefix);
        name.append(prefixName);
    }

    if (!pi.command.empty()) {
        name.append(pi.command);
    }

    for (const auto& param : pi.params) {
        name.append(param);
    }

    return name;
}

std::string toQueryString(const ParsedInterestName& pi) {
    std::ostringstream oss;
    oss << "/?";

    // id  = first param
    oss << pi.prefix;

    // date = second param
    oss << "&" << pi.command;

    // any extra params get generic names “param2”, “param3”, …
    for (size_t i = 0; i < pi.params.size(); ++i) {
      oss << "&" << pi.params[i];
    }

    return oss.str();
}
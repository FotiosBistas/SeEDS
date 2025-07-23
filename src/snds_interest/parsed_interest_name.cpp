#include "parsed_interest_name.h"

interest_message_type::InterestMessageType interest_message_type::parseInterestMessageType(const std::string& message_component)
{
    constexpr std::string_view delimiter = ":";
    // find the ':' that terminates the type token
    auto pos = message_component.find(delimiter);
    if (pos == std::string::npos) {
        return InterestMessageType::EMPTY;
    }

    std::string_view message_string_type = message_component.substr(0, pos);

    if (message_string_type == "temporalQuery") 
    {
        return InterestMessageType::TEMPORAL_QUERY;
    }
    else if (message_string_type == "version") 
    {
        return InterestMessageType::VERSION;
    }
    else if (message_string_type == "")
    {
        return InterestMessageType::EMPTY;
    }
}

parsed_interest_name::ParsedInterestName parsed_interest_name::parseInterestName(const ndn::Name& name) 
{
    ParsedInterestName pi;
    size_t n = name.size();
    //note that the name is in the following format
    // /SNDS/car2/version:<value>?ndn_params
    if(n > 0) 
    {
        pi.prefix = name.get(0).toUri();
    }

    if(n > 1) 
    {
        pi.name = name.get(1).toUri();
    }

    if(n > 2) 
    {
        pi.command = interest_message_type::parseInterestMessageType(name.get(2).toUri());
    }

    for(size_t i = 3; i < n; ++i) 
    {
        pi.params.push_back(name.get(i).toUri());
    }

    return pi;
}


std::string parsed_interest_name::toCustomNDNName(const parsed_interest_name::ParsedInterestName& pi) 
{

    std::string name; 

    // Handle the prefix (supports multi-component prefixes like "/SNDS/service")
    if (!pi.prefix.empty()) 
    {
        name.append("/" + pi.prefix);
    }

    if (!pi.name.empty()) 
    {
        name.append("/" + pi.name);
    }

    if (pi.command != interest_message_type::InterestMessageType::EMPTY) 
    {
        name.append("/" + interest_message_type::toString(pi.command));
    }

    //TODO these theoretically shouldn't be inside the scopes
    for (const auto& param : pi.params) {
        name.append(param + "&");
    }

    return name;
}

std::string parsed_interest_name::toQueryString(const parsed_interest_name::ParsedInterestName& pi) {
    std::ostringstream oss;
    oss << "/?";

    // prefix  = first param
    oss << pi.prefix;

    // name = second param
    oss << "&" << pi.name;

    // command = third param
    oss << "&" << interest_message_type::toString(pi.command);

    // any extra params get generic names “param2”, “param3”, …
    //TODO these theoretically shouldn't be inside the scopes
    for (size_t i = 0; i < pi.params.size(); ++i) {
        oss << "&" << pi.params[i];
    }

    return oss.str();
}
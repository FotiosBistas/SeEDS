#include "parsed_interest_name.hpp"

parsed_interest_name::ParsedInterestName parsed_interest_name::parse_interest_name(const ndn::Name& name) 
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
        pi.command = interest_message_type::from_string(name.get(2).toUri());
    }

    for(size_t i = 3; i < n; ++i) 
    {
        pi.params.push_back(name.get(i).toUri());
    }

    return pi;
}


std::string parsed_interest_name::to_custom_ndn_name(const parsed_interest_name::ParsedInterestName& pi, const bool url_encode_flag) 
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

    if (pi.command != interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY) 
    {
        name.push_back('/');
        name.append(interest_message_type::to_string(pi.command));
        url_encode_flag ? name.append(url_encode(":")) : name.append(":");
    }

    //TODO these theoretically shouldn't be inside the scopes
    for (const auto& param : pi.params) 
    {
        url_encode_flag ? name.append(url_encode(param + "&")) : name.append(param + "&");
    }

    return name;
}

std::string parsed_interest_name::to_query_string(const parsed_interest_name::ParsedInterestName& pi) {
    std::ostringstream oss;
    oss << "/?";

    // prefix  = first param
    oss << pi.prefix;

    // name = second param
    oss << "&" << pi.name;

    // command = third param
    oss << "&" << interest_message_type::to_string(pi.command);

    // any extra params get generic names “param2”, “param3”, …
    //TODO these theoretically shouldn't be inside the scopes
    for (size_t i = 0; i < pi.params.size(); ++i) {
        oss << "&" << pi.params[i];
    }

    return oss.str();
}

std::vector<std::string> parsed_interest_name::split
(
    std::string_view view,
    const char delimeter
)
{
    std::vector<std::string> tokens;
    size_t last = 0;

    // find each delimiter, extract the token before it
    while (true) {
        auto next = view.find(delimeter, last);
        if (next == std::string_view::npos) 
        {
            break;
        }
        // get substring [last, next)
        if (next > last)
        {
            tokens.emplace_back(view.substr(last, next - last));
        }
        // advance past the delimiter
        last = next + 1;
    }

    // handle the final segment after the last delimiter
    if (last < view.size()) 
    {
        tokens.emplace_back(view.substr(last));
    }

    return tokens;
}


parsed_interest_name::ParsedInterestName parsed_interest_name::parse_from_string(const std::string& name)
{
    //split on '/' and ignore empty pieces
    auto parts = split(name, '/');
    ParsedInterestName pi;

    // assign prefix and name
    if (parts.size() > 0)
    {
        pi.prefix = parts[0];
    }


    size_t command_index = parts.size();

    for (size_t i = 1; i < parts.size(); ++i) 
    {
        // parse all the parts and find the possible command index
        auto t = interest_message_type::from_string(parts[i]);
        if (t != interest_message_type::INTEREST_MESSAGE_TYPE::EMPTY) {
            pi.command = t;
            command_index = i;
            break;
        }
    }

    size_t name_end = (command_index < parts.size() ? command_index : parts.size());
    std::ostringstream oss;
    for (size_t i = 1; i < name_end; ++i) 
    {
        if (i > 1) oss << '/';
        oss << parts[i];
    }
    pi.name = oss.str();

    // parse the “command” token (e.g. "temporalQuery:date=2025…&timeframe=…")
    if (command_index < parts.size()) 
    {
        const auto& command_string = parts[command_index];
        auto pos = command_string.find(':');
        if (pos != std::string::npos && pos + 1 < command_string.size()) 
        {
            auto parameter_list = command_string.substr(pos + 1);
            for (auto& kv : split(parameter_list, '&')) 
            {
                if (!kv.empty()) 
                {
                    pi.params.push_back(std::move(kv));
                }
            }
        }
    }

    // any further slash-separated parts are treated as extra params
    if (command_index < parts.size()) 
    {
        for (size_t i = command_index + 1; i < parts.size(); ++i) 
        {
            pi.params.push_back(parts[i]);
        }
    }

    return pi;
}

#pragma once

#include <string> 

#include "httplib.h"

#include "snds_utils.h" 

static constexpr const char* GET_PARAMETER_ID_TAG      = "id";
static constexpr const char* GET_PARAMETER_TYPE_TAG    = "type";
static constexpr const char* GET_PARAMETER_FIELDS_TAG  = "filters";
static constexpr const char* GET_PARAMETER_SUB_URL_TAG = "suburl";
static constexpr const char* GET_PARAMETER_IS_SUB_TAG  = "isSub";
// all of the below are temporal query related
static constexpr const char* GET_PARAMETER_DATE_TAG="date";
static constexpr const char* GET_PARAMETER_TIMEFRAME_TAG="timeframe";
static constexpr const char* GET_PARAMETER_COUNT_TAG="count";
static constexpr const char* GET_PARAMETER_PARENT_QUERY_HASH_TAG="parentQueryHash";
static constexpr const char* GET_PARAMETER_LAST_METAFILE_INTEREST_TAG="lastInterest";
static constexpr const char* GET_PARAMETER_IS_METAFILE_TAG="metafile";

template<typename T>
struct QueryParam;

template<>
struct QueryParam<std::string> 
{
    static std::optional<std::string>
    extract(const httplib::Request& req, const char* tag, bool decode)
    {
        if (!req.has_param(tag)) return std::nullopt;
        auto v = req.get_param_value(tag);
        return decode ? url_decode(v) : v;
    }
};

template<>
struct QueryParam<bool> 
{
    static std::optional<bool>
    extract(const httplib::Request& req, const char* tag, bool _)
    {
        if (!req.has_param(tag)) return std::nullopt;
        auto v = req.get_param_value(tag);
        return (v == "1" || v == "true");
    }
};

#define QUERY_PARAM_LIST(M)                                                           \
    M(id,               GET_PARAMETER_ID_TAG,                     false, std::string) \
    M(type,             GET_PARAMETER_TYPE_TAG,                   false, std::string) \
    M(count,            GET_PARAMETER_COUNT_TAG,                  false, std::string) \
    M(date,             GET_PARAMETER_DATE_TAG,                   true,  std::string) \
    M(timeframe,        GET_PARAMETER_TIMEFRAME_TAG,              true,  std::string) \
    M(filters,          GET_PARAMETER_FIELDS_TAG,                 true,  std::string) \
    M(parent_query_hash,GET_PARAMETER_PARENT_QUERY_HASH_TAG,      true,  std::string) \
    M(subscription_url, GET_PARAMETER_SUB_URL_TAG,                false, std::string) \
	M(metafile,         GET_PARAMETER_IS_METAFILE_TAG,            true,  std::string) \
    M(last_interest,    GET_PARAMETER_LAST_METAFILE_INTEREST_TAG, true,  std::string) \
    M(is_subscription,  GET_PARAMETER_IS_SUB_TAG,                 false, bool)       

#define DECLARE_PARAM(var, tag, dec, type) std::optional<type> var{};
#define DECLARE_QUERY_PARAMS() QUERY_PARAM_LIST(DECLARE_PARAM)
#define EXTRACT_PARAM(var, tag, dec, type)  var = QueryParam<type>::extract(req, tag, dec);
#define EXTRACT_QUERY_PARAMS(req) QUERY_PARAM_LIST(EXTRACT_PARAM)

#define APPLY_PARAM_TO(msg, var, tag, dec, type) \
  do {                                           \
    if (var.has_value()) {                       \
        msg->set_##var##_(std::move(*var));      \
    }                                            \
  } while (0);

#define APPLY_PARAM_TO_FOR_MESSAGE(var, tag, dec, type) \
    APPLY_PARAM_TO(msg_ptr, var, tag, dec, type)

#define APPLY_QUERY_PARAMS_TO(msg_unique_ptr)               \
  do {                                                      \
    auto *msg_ptr = (msg_unique_ptr).get();                 \
    QUERY_PARAM_LIST(APPLY_PARAM_TO_FOR_MESSAGE)            \
  } while (0);


//#undef EXTRACT_QUERY_PARAMS
//#undef DECLARE_QUERY_PARAMS
//#undef EXTRACT_PARAM
//#undef DECLARE_PARAM
//#undef QUERY_PARAM_LIST
//#undef APPLY_PARAM
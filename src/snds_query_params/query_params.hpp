#pragma once
/**
 * @file query_params.hpp
 * @brief Helpers and X-macros to declare, extract, and apply HTTP query parameters to
 *        higher-level message objects (e.g., SNDS_INTRA_MESSAGE) in a consistent way.
 *
 * @details
 * This header provides:
 *  - String constants for all supported HTTP query parameter keys.
 *  - A small traits-like extractor, `QueryParam<T>`, specialized for `std::string` and `bool`.
 *  - An X-macro list `QUERY_PARAM_LIST(M)` that is the single source of truth for all
 *    supported parameters: their storage variable name, wire key, whether to URL-decode,
 *    and value type.
 *  - Convenience macros to:
 *      * declare a local set of `std::optional<T>` variables for all params,
 *      * extract those params from `httplib::Request`,
 *      * apply the present param values to a target message via generated setters
 *        (e.g., `msg->set_id_(...)`).
 *
 * ### Typical usage
 * @code
 * // 1) Declare locals (std::optional<...> for each param in the list)
 * DECLARE_QUERY_PARAMS();
 *
 * // 2) Extract from an incoming request
 * EXTRACT_QUERY_PARAMS(req);   // fills optionals if present in URL
 *
 * // 3) Apply to your message object (unique_ptr<T> that has set_<param>_ setters)
 * std::unique_ptr<SNDS_INTRA_MESSAGE> msg = std::make_unique<SNDS_INTRA_MESSAGE>();
 * APPLY_QUERY_PARAMS_TO(msg);
 * @endcode
 *
 * @note For string parameters marked with `decode=true` in the list, values are URL-decoded
 *       with `url_decode()`. The `bool` extractor accepts "1" or "true" (case-sensitive) as true.
 */
#include <string> 
#include "httplib.h"
#include "snds_utils.h" 

//------------------------------------------------------------------------------
// Canonical query parameter keys (wire names)
//------------------------------------------------------------------------------
/** @brief Query key for a logical identifier (e.g., document or resource id). */
static constexpr const char* GET_PARAMETER_ID_TAG      = "id";
/** @brief Query key for a logical type/category (e.g., collection name). */
static constexpr const char* GET_PARAMETER_TYPE_TAG    = "type";
/** @brief Query key for a field filter (comma- or app-specific). */
static constexpr const char* GET_PARAMETER_FIELDS_TAG  = "filters";
/** @brief Query key carrying a subscription callback URL (when applicable). */
static constexpr const char* GET_PARAMETER_SUB_URL_TAG = "suburl";
/** @brief Query key flag indicating subscription semantics (true/1). */
static constexpr const char* GET_PARAMETER_IS_SUB_TAG  = "isSub";
// --- Temporal query related keys ---
/** @brief Query key for a reference date/time string (various formats). */
static constexpr const char* GET_PARAMETER_DATE_TAG="date";
/** @brief Query key for timeframe (e.g., NOW/BEFORE/AFTER or app-specific). */
static constexpr const char* GET_PARAMETER_TIMEFRAME_TAG="timeframe";
/** @brief Query key for a numeric limit on returned points/records. */
static constexpr const char* GET_PARAMETER_COUNT_TAG="count";
/** @brief Query key for a parent query hash (lineage/correlation). */
static constexpr const char* GET_PARAMETER_PARENT_QUERY_HASH_TAG="parentQueryHash";
/** @brief Query key for last metafile interest name (transport-specific). */
static constexpr const char* GET_PARAMETER_LAST_METAFILE_INTEREST_TAG="lastInterest";
/** @brief Query key flag indicating a metafile request (as opposed to data). */
static constexpr const char* GET_PARAMETER_IS_METAFILE_TAG="metafile";
/** @brief Query key for retrieval mode (e.g., consistency/strategy hint). */
static constexpr const char* GET_PARAMETER_RETRIEVE_MODE="mode";
//------------------------------------------------------------------------------
// Traits-like extractor for query params (overload by value type)
//------------------------------------------------------------------------------
/**
 * @brief Primary template for query param extraction; specialized per type.
 * @tparam T Target C++ type for the parameter.
*/
template<typename T>
struct QueryParam;
/**
 * @brief String parameter extractor.
 * @details Returns the parameter value as `std::string`. If @p decode is true,
 *          the value is URL-decoded using `url_decode()`.
*/
template<>
struct QueryParam<std::string> 
{
	/**
     * @brief Extract a string query parameter from an HTTP request.
     * @param req    Incoming request.
     * @param tag    Parameter key (wire name).
     * @param decode Whether to URL-decode the value.
     * @return std::nullopt if missing, otherwise the (optionally decoded) value.
    */
    static std::optional<std::string>
    extract(const httplib::Request& req, const char* tag, bool decode)
    {
        if (!req.has_param(tag)) return std::nullopt;
        auto v = req.get_param_value(tag);
        return decode ? url_decode(v) : v;
    }
};
/**
 * @brief Boolean parameter extractor.
 * @details Interprets the value "1" or "true" as true; anything else as false.
 *          The @p decode flag is ignored for booleans.
*/
template<>
struct QueryParam<bool> 
{
	/**
     * @brief Extract a boolean query parameter from an HTTP request.
     * @param req Incoming request.
     * @param tag Parameter key (wire name).
     * @param _   Unused (present to match the `QueryParam<T>::extract` signature).
     * @return std::nullopt if missing, otherwise the parsed boolean.
    */
    static std::optional<bool>
    extract(const httplib::Request& req, const char* tag, bool _)
    {
        if (!req.has_param(tag)) return std::nullopt;
        auto v = req.get_param_value(tag);
        return (v == "1" || v == "true");
    }
};
//------------------------------------------------------------------------------
// X-macro: single list of supported query params
//
// Each entry has the shape: M(var, tag, decode, type)
//   - var   : local variable name (and setter suffix on the target message)
//   - tag   : wire key (const char* from the constants above)
//   - decode: bool; if true, URL-decode string values
//   - type  : C++ type (std::string or bool for current specializations)
//------------------------------------------------------------------------------

/**
 * @def QUERY_PARAM_LIST(M)
 * @brief Expand @p M for each known query parameter.
 *
 * @param M A macro of four parameters `(var, tag, decode, type)` that is invoked
 *          for every listed parameter.
 *
 * @note This is the single source of truth; other macros below reuse it to
 *       declare locals, extract from requests, and apply to messages.
*/
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
//------------------------------------------------------------------------------
// Convenience macros built on the list above
//------------------------------------------------------------------------------
/**
 * @def DECLARE_PARAM(var, tag, dec, type)
 * @brief Declare a local optional variable for one parameter.
*/
#define DECLARE_PARAM(var, tag, dec, type) std::optional<type> var{};
/**
 * @def DECLARE_QUERY_PARAMS()
 * @brief Declare a full set of local `std::optional<...>` variables for all parameters.
 *
 * @details Expands to multiple `std::optional<type> <var>{};` declarations, one per
 * parameter in `QUERY_PARAM_LIST`.
*/
#define DECLARE_QUERY_PARAMS() QUERY_PARAM_LIST(DECLARE_PARAM)
/**
 * @def EXTRACT_PARAM(var, tag, dec, type)
 * @brief Extract one parameter from an `httplib::Request` into the local optional.
*/
#define EXTRACT_PARAM(var, tag, dec, type)  var = QueryParam<type>::extract(req, tag, dec);
/**
 * @def EXTRACT_QUERY_PARAMS(req)
 * @brief Extract all parameters from @p req into the previously declared optionals.
 *
 * @pre Call `DECLARE_QUERY_PARAMS()` beforehand in the same scope.
*/
#define EXTRACT_QUERY_PARAMS(req) QUERY_PARAM_LIST(EXTRACT_PARAM)
/**
 * @def APPLY_PARAM_TO(msg, var, tag, dec, type)
 * @brief If the local optional `<var>` has a value, call `msg->set_<var>_(value)`.
 *
 * @param msg A pointer to a message object that provides setters named `set_<var>_(T)`.
 *
 * @details Ownership of the value is moved into the message (uses `std::move(*var)`).
*/
#define APPLY_PARAM_TO(msg, var, tag, dec, type) \
  do {                                           \
    if (var.has_value()) {                       \
        msg->set_##var##_(std::move(*var));      \
    }                                            \
  } while (0);
/**
 * @def APPLY_PARAM_TO_FOR_MESSAGE(var, tag, dec, type)
 * @brief Helper that binds the `msg_ptr` symbol for use with @ref APPLY_QUERY_PARAMS_TO.
*/
#define APPLY_PARAM_TO_FOR_MESSAGE(var, tag, dec, type) \
    APPLY_PARAM_TO(msg_ptr, var, tag, dec, type)
/**
 * @def APPLY_QUERY_PARAMS_TO(msg_unique_ptr)
 * @brief Apply all present local optionals to the given message unique_ptr.
 *
 * @param msg_unique_ptr `std::unique_ptr<T>` where `T` exposes setters `set_<var>_(T)`.
 *
 * @pre You must have declared and extracted the local optionals in the same scope:
 *      `DECLARE_QUERY_PARAMS(); EXTRACT_QUERY_PARAMS(req);`
*/
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
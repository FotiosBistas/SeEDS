#pragma once
#include <ndn-cxx/face.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <random>
#include <optional>
#include <functional>
#include <chrono>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp> 

#include "parsed_interest_name.hpp"
#include "snds_utils.h"

class ResiliencePoller {
public:

    struct Meta 
    {
        uint64_t    version{0};
        std::string hash;
        std::string ts;
    };

    ResiliencePoller
    (
        ndn::Face& face,
        std::shared_ptr<spdlog::logger> log,
        std::string snds_prefix,
        std::string peer_is_alive_name,   // e.g. "is_alive_2"
        std::string peer_state_name,      // e.g. "state_2"
        std::chrono::milliseconds alive_period = std::chrono::seconds(2),
        std::chrono::milliseconds meta_period  = std::chrono::seconds(2)
    );

    ~ResiliencePoller();

    void start();
    void stop();

    // optional callbacks (run on the face's io_service thread)
    std::function<void(bool ok)>            on_alive;
    std::function<void(const Meta& remote)> on_meta;
    std::function<void(const std::string&)> on_error;
    std::function<bool(const std::string& /*patch*/)> on_apply_patch;
    std::function<bool(const std::string& /*snapshot*/)> on_apply_snapshot;
    std::function<void(uint64_t from_version, uint64_t to_version)> on_patch_applied_ok;
    std::function<void(uint64_t to_version)> on_snapshot_applied_ok;

    bool get_last_alive_ok() const 
    { 
        return last_alive_ok_.load(); 
    }

    const std::optional<Meta>& last_meta() const 
    { 
        return last_meta_; 
    }




private:
    // threads that only schedule work
    void alive_loop_();
    void meta_loop_();

    // run on io_service thread
    void send_is_alive_(uint64_t nonce);
    void send_meta_();
    void send_patch_(uint64_t from_version, uint64_t to_version);
    void send_snapshot_(uint64_t to_version);

    std::optional<uint64_t> last_applied_version_;
    std::atomic<bool>       patch_in_flight_{false};

    std::string make_alive_name_(uint64_t nonce) const;
    std::string make_patch_name_(uint64_t from_version, uint64_t to_version) const;
    std::string make_snapshot_name_(uint64_t to_version) const;
    std::string make_meta_name_() const;

    void handle_is_alive_data_(const ndn::Interest& interest, const ndn::Data& data);
    void handle_is_alive_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    void handle_is_alive_timeout_(const ndn::Interest& interest);

    void handle_meta_data_(const ndn::Interest& interest, const ndn::Data& data);
    void handle_meta_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    void handle_meta_timeout_(const ndn::Interest& interest);

    void handle_patch_data_(const ndn::Interest& interest, const ndn::Data& data);
    void handle_patch_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    void handle_patch_timeout_(const ndn::Interest& interest);

    void handle_snapshot_data_(const ndn::Interest& interest, const ndn::Data& data);
    void handle_snapshot_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    void handle_snapshot_timeout_(const ndn::Interest& interest);


private:
    ndn::Face&                       face_;
    std::shared_ptr<spdlog::logger>  log_;
    const std::string                prefix_;
    const std::string                peer_alive_;
    const std::string                peer_state_;
    const std::chrono::milliseconds  alive_period_;
    const std::chrono::milliseconds  meta_period_;

    std::atomic<bool>                running_{false};
    std::thread                      alive_thr_;
    std::thread                      meta_thr_;
    std::mt19937_64                  rng_{std::random_device{}()};

    std::atomic<bool>                last_alive_ok_{false};
    std::optional<Meta>              last_meta_;
};

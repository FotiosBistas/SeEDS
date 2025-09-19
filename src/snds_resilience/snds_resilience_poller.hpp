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

/**
 * @class ResiliencePoller
 * @brief Periodically probes a peer node for liveness and state metadata, and pulls patches/snapshots.
 *
 * @details
 * The poller runs two lightweight scheduler threads:
 *  - an "alive loop" that sends GET_RESILIENCE_IS_ALIVE interests with a random nonce,
 *  - a "meta loop" that sends GET_RESILIENCE_META interests to discover the peer's head {version,hash,ts}.
 *
 * All network I/O is posted onto the NDN Face's io_context (thread-affine) to keep ndn-cxx happy.
 * When the remote version advances, the poller requests a patch (from last applied → remote head).
 * If a patch cannot be applied (or NACKs/times out), it falls back to fetching a snapshot.
 *
 * Application code should wire the callbacks to actually apply patches/snapshots to local state.
 * Callbacks are invoked on the Face's io_context thread.
*/
class ResiliencePoller {
public:

    /** @brief Lightweight view of a remote head (from GET_RESILIENCE_META). */
    struct Meta 
    {
        /** Monotonic remote version. */
        uint64_t    version{0};
        /** Canonical hash of the remote state. */
        std::string hash;
        /** ISO-8601 timestamp provided by the peer. */
        std::string ts;
    };

    /**
     * @brief Construct a poller bound to an existing NDN Face and logger.
     *
     * @param face             NDN Face; its io_context is used for all network work.
     * @param log              Logger (may be nullptr).
     * @param snds_prefix      Root SNDS prefix (e.g. "SNDS").
     * @param peer_is_alive_name  Peer "is_alive_*" component (e.g. "is_alive_2").
     * @param peer_state_name     Peer "state_*" component (e.g. "state_2").
     * @param alive_period     Period between liveness probes.
     * @param meta_period      Period between meta probes.
     *
     * @note The poller does not take ownership of @p face.
    */
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

    /**
     * @brief Destructor. Stops scheduler threads if still running.
     * @see stop()
    */
    ~ResiliencePoller();
    /**
     * @brief Start periodic liveness and meta polling.
     *
     * @details Idempotent: subsequent calls while running are no-ops. Spawns two scheduler
     * threads that post work to the Face's io_context.
    */
    void start();
    /**
     * @brief Stop periodic polling and join scheduler threads.
     *
     * @details Idempotent: calling when not running is a no-op.
    */
    void stop();

    // optional callbacks (run on the face's io_service thread)
    // =======================
    // Callback hooks
    // =======================
    /**
     * @brief Called after each is_alive reply (true = nonce echoed correctly).
     * @note Runs on the Face's io_context thread.
    */
    std::function<void(bool ok)>            on_alive;
    /**
     * @brief Called when a new remote Meta is received.
     * @note Runs on the Face's io_context thread.
    */
    std::function<void(const Meta& remote)> on_meta;
    /**
     * @brief Called on NACKs, timeouts, or parse errors (human-readable).
     * @note Runs on the Face's io_context thread.
    */
    std::function<void(const std::string&)> on_error;
    /**
     * @brief Apply a received patch (payload is the JSON body as string).
     *
     * @return true if patch applied successfully; false to trigger snapshot fallback.
     * @note Runs on the Face's io_context thread.
    */
    std::function<bool(const std::string& /*patch*/)> on_apply_patch;
    /**
     * @brief Apply a received snapshot (payload is the JSON body as string).
     *
     * @return true on success; false indicates a fatal apply error.
     * @note Runs on the Face's io_context thread.
    */
    std::function<bool(const std::string& /*snapshot*/)> on_apply_snapshot;
    /**
     * @brief Notification after a patch is applied successfully.
     * @note Runs on the Face's io_context thread.
    */
    std::function<void(uint64_t from_version, uint64_t to_version)> on_patch_applied_ok;
    /**
     * @brief Notification after a snapshot is applied successfully.
     * @note Runs on the Face's io_context thread.
    */
    std::function<void(uint64_t to_version)> on_snapshot_applied_ok;
    /**
     * @brief Last liveness result (atomic).
     * @return true if the most recent is_alive check succeeded.
    */
    bool get_last_alive_ok() const 
    { 
        return last_alive_ok_.load(); 
    }
    /**
     * @brief Last received remote Meta (if any).
     * @return Optional Meta structure; std::nullopt until first meta arrives.
    */
    const std::optional<Meta>& last_meta() const 
    { 
        return last_meta_; 
    }
private:
    // threads that only schedule work
    /**
     * @brief Periodically schedule an is-alive probe.
     * @details Runs on a background thread; generates a nonce and posts send_is_alive_(nonce)
     *          to the Face io_context, then sleeps for @ref alive_period_.
    */
    void alive_loop_();
    /**
     * @brief Periodically schedule a meta fetch.
     * @details Runs on a background thread; posts send_meta_() to the Face io_context,
     *          then sleeps for @ref meta_period_.
    */
    void meta_loop_();

    // run on io_service thread
    /**
     * @brief Express an is_alive Interest with a nonce.
     * @details Runs on the Face io_context thread. Enables prefix match, freshness, and a 3s lifetime.
     *          Registers handlers for Data/NACK/timeout.
     * @param nonce Nonce to include in the Interest parameters.
    */
    void send_is_alive_(uint64_t nonce);
    /**
     * @brief Express a meta Interest.
     * @details Runs on the Face io_context thread. Enables prefix match, freshness, and a 2s lifetime.
     *          Registers handlers for Data/NACK/timeout.
    */
    void send_meta_();
    /**
     * @brief Express a patch Interest for a version range.
     * @details Runs on the Face io_context thread. Enables prefix match, freshness, and a 4s lifetime.
     *          Registers handlers for Data/NACK/timeout.
     * @param from_version Local base version.
     * @param to_version   Remote target version.
    */
    void send_patch_(uint64_t from_version, uint64_t to_version);
    /**
     * @brief Express a snapshot Interest for a specific version.
     * @details Runs on the Face io_context thread. Enables prefix match, freshness, and a 6s lifetime.
     *          Registers handlers for Data/NACK/timeout.
     * @param to_version Target version to fetch from the peer.
    */
    void send_snapshot_(uint64_t to_version);

    std::optional<uint64_t> last_applied_version_;
    std::atomic<bool>       patch_in_flight_{false};

    /**
     * @brief Build the fully-qualified is_alive Interest name with a nonce.
     * @param nonce Caller-generated nonce to echo back for verification.
     * @return Encoded NDN name string.
    */
    std::string make_alive_name_(uint64_t nonce) const;
    /**
     * @brief Build the fully-qualified patch Interest name.
     * @param from_version Base version held locally.
     * @param to_version   Target version advertised by the peer.
     * @return Encoded NDN name string.
    */
    std::string make_patch_name_(uint64_t from_version, uint64_t to_version) const;
    /**
     * @brief Build the fully-qualified snapshot Interest name.
     * @param to_version Target version requested from the peer.
     * @return Encoded NDN name string.
    */
    std::string make_snapshot_name_(uint64_t to_version) const;
    /**
     * @brief Build the fully-qualified meta Interest name.
     * @return Encoded NDN name string.
    */
    std::string make_meta_name_() const;
    /**
     * @brief Handle is_alive Data response from the NDN.
     * @details Parses JSON payload, verifies {"ack":true} and that "nonce" matches the one
     *          embedded in the Data name; updates @ref last_alive_ok_ and fires @ref on_alive.
     * @param interest (unused) Interest that triggered the Data.
     * @param data     Received Data packet.
    */
    void handle_is_alive_data_(const ndn::Interest& interest, const ndn::Data& data);
    /**
     * @brief Handle is_alive NACK, from the NDN.
     * @details Marks liveness false and fires @ref on_alive(false).
     * @param interest (unused)
     * @param nack     Received NACK.
    */
    void handle_is_alive_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    /**
     * @brief Handle is_alive timeout, from the NDN.
     * @details Marks liveness false and fires @ref on_alive(false).
     * @param interest (unused)
    */
    void handle_is_alive_timeout_(const ndn::Interest& interest);
    /**
     * @brief Handle meta Data response, from the NDN.
     * @details Parses JSON {"version","hash","ts"}, updates @ref last_meta_, fires @ref on_meta,
     *          and decides how to synchronize:
     *          - If no local version yet, fetch a snapshot at the advertised version.
     *          - If remote advanced and no patch is in flight, request a patch from local→remote.
     * @param interest (unused)
     * @param data     Received Data packet.
    */
    void handle_meta_data_(const ndn::Interest& interest, const ndn::Data& data);
    /**
     * @brief Handle meta NACK, from the NDN.
     * @details Fires @ref on_error("meta NACK").
    */
    void handle_meta_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    /**
     * @brief Handle meta timeout, from the NDN.
     * @details Fires @ref on_error("meta timeout").
    */
    void handle_meta_timeout_(const ndn::Interest& interest);
    /**
     * @brief Handle patch Data response, from the NDN.
     * @details Extracts base/target versions from payload (or Data name fallback), then
     *          invokes @ref on_apply_patch with the raw JSON payload. On success, updates
     *          @ref last_applied_version_, clears @ref patch_in_flight_, and fires
     *          @ref on_patch_applied_ok. On failure, attempts a snapshot fallback for the
     *          inferred target version.
     * @param interest (unused)
     * @param data     Received Data packet.
    */
    void handle_patch_data_(const ndn::Interest& interest, const ndn::Data& data);
    /**
     * @brief Handle patch NACK, from the NDN.
     * @details Attempts snapshot fallback using target version parsed from the Interest name.
     *          Clears @ref patch_in_flight_. Logs if fallback is not possible.
     * @param interest Original Interest (used to parse target version).
     * @param nack     (unused)
    */
    void handle_patch_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    /**
     * @brief Handle snapshot Data response, from the NDN.
     * @details Invokes @ref on_apply_snapshot with the raw JSON payload. On success, updates
     *          @ref last_applied_version_ (if known), clears @ref patch_in_flight_, and fires
     *          @ref on_snapshot_applied_ok. On failure, fires @ref on_error.
     * @param interest (unused)
     * @param data     Received Data packet.
    */
    void handle_patch_timeout_(const ndn::Interest& interest);
    /**
     * @brief Handle snapshot Data response, from the NDN.
     * @details Invokes @ref on_apply_snapshot with the raw JSON payload. On success, updates
     *          @ref last_applied_version_ (if known), clears @ref patch_in_flight_, and fires
     *          @ref on_snapshot_applied_ok. On failure, fires @ref on_error.
     * @param interest (unused)
     * @param data     Received Data packet.
    */
    void handle_snapshot_data_(const ndn::Interest& interest, const ndn::Data& data);
    /**
     * @brief Handle snapshot NACK, from the NDN.
     * @details Fires @ref on_error("snapshot NACK").
    */
    void handle_snapshot_nack_(const ndn::Interest& interest, const ndn::lp::Nack& nack);
    /**
     * @brief Handle snapshot timeout, from the NDN.
     * @details Fires @ref on_error("snapshot timeout").
    */
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

#include <iostream>
#include <optional>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "snds_resilience.hpp"

using snds_resilience::Store;
using snds_resilience::Mirror;
using snds_resilience::Meta;
using snds_resilience::Snapshot;
using snds_resilience::Patch;

static void print_meta(const Meta& m) 
{
    nlohmann::json j = m.to_json();
    std::cout << "\n[Meta]\n" << j.dump(2) << "\n";
}

static void print_snapshot(const Snapshot& s) 
{
    nlohmann::json j = s.to_json();
    std::cout << "\n[Snapshot]\n" << j.dump(2) << "\n";
}

static void print_patch(const Patch& p) 
{
    nlohmann::json j = p.to_json();
    std::cout << "\n[Patch]\n" << j.dump(2) << "\n";
}

int main() 
{
    // ----- logger -----
    auto logger = spdlog::stdout_color_mt("demo");
    spdlog::set_level(spdlog::level::debug);

    std::cout << "=== Resilience demo: Store ↔ Mirror ===\n";

    // ----- create a Store with tiny history to show eviction behavior -----
    Store store(logger);
    store.set_history_keep(2); // keep only the last 2 states

    // initial meta/snapshot (empty object {})
    print_meta(store.get_meta());
    print_snapshot(store.get_snapshot());

    // ----- Mirror starts empty; apply initial snapshot -----
    Mirror mirror(logger);
    mirror.apply_snapshot(store.get_snapshot());
    std::cout << "Mirror at ver=" << mirror.version() << " hash=" << mirror.hash() << "\n";

    // ----- Mutate #1: add fields -----
    store.mutate([](nlohmann::json& s) 
    {
        s["status"]  = "idle";
        s["counter"] = 0;
    });

    auto s1 = store.get_snapshot();
    print_snapshot(s1);
    mirror.apply_snapshot(s1); // bootstrap mirror with latest
    std::cout << "Mirror now ver=" << mirror.version() << " hash=" << mirror.hash() << "\n";

    // ----- Mutate #2: increase counter → make patch from base = s1.version -----
    const auto base_for_patch2 = s1.version; // expect 1
    store.mutate([](nlohmann::json& s) 
    {
        s["counter"] = s.value("counter", 0) + 1;
    });

    auto maybe_p2 = store.make_patch_from(base_for_patch2);
    if (maybe_p2) 
    {
        print_patch(*maybe_p2);
        bool ok = mirror.apply_patch(*maybe_p2);
        std::cout << "Applied patch2? " << (ok ? "yes" : "no") 
                  << " | mirror ver=" << mirror.version() 
                  << " hash=" << mirror.hash() << "\n";
    } 
    else 
    {
        std::cout << "No patch2 available (base evicted) — send snapshot instead.\n";
        mirror.apply_snapshot(store.get_snapshot());
    }

    // ----- Mutate #3: add nested data; history_keep=2 will start evicting old versions -----
    const auto base_for_patch3 = mirror.version(); // patch from the mirror's current version
    store.mutate([](nlohmann::json& s) 
    {
        s["mode"]    = "eco";
        s["sensors"] = nlohmann::json{
            {"t", 22.5},
            {"h", 0.45}
        };
    });

    // Try to make a patch from base_for_patch3 (likely present)
    auto maybe_p3 = store.make_patch_from(base_for_patch3);
    if (maybe_p3) 
    {
        print_patch(*maybe_p3);
        bool ok = mirror.apply_patch(*maybe_p3);
        std::cout << "Applied patch3? " << (ok ? "yes" : "no")
                  << " | mirror ver=" << mirror.version()
                  << " hash=" << mirror.hash() << "\n";
    } 
    else 
    {
        std::cout << "Patch3 unavailable (base evicted) — sending full snapshot.\n";
        auto snap = store.get_snapshot();
        print_snapshot(snap);
        mirror.apply_snapshot(snap);
    }

    // ----- Mutate #4: remove a key to show JSON patch can delete -----
    const auto base_for_patch4 = mirror.version();
    store.mutate([](nlohmann::json& s) 
    {
        s.erase("status");
    });

    auto maybe_p4 = store.make_patch_from(base_for_patch4);
    if (maybe_p4) 
    {
        print_patch(*maybe_p4);
        bool ok = mirror.apply_patch(*maybe_p4);
        std::cout << "Applied patch4? " << (ok ? "yes" : "no")
                  << " | mirror ver=" << mirror.version()
                  << " hash=" << mirror.hash() << "\n";
    } 
    else 
    {
        std::cout << "Patch4 unavailable — sending snapshot.\n";
        auto snap = store.get_snapshot();
        print_snapshot(snap);
        mirror.apply_snapshot(snap);
    }

    // ----- Show wire (de)serialization: to_json() / reconstruct back -----
    std::cout << "\n=== Wire round-trip example ===\n";
    Snapshot latest = store.get_snapshot();
    nlohmann::json wire_snap = latest.to_json();
    std::cout << wire_snap.dump(2) << "\n";

    // reconstruct Snapshot (simple manual mapping; can add from_json helpers)
    Snapshot reconstructed
    {
        wire_snap.at("version").get<uint64_t>(),
        wire_snap.at("hash").get<std::string>(),
        wire_snap.at("state")
    };

    // apply reconstructed to mirror (no-op if already up-to-date)
    mirror.apply_snapshot(reconstructed);
    std::cout << "Mirror after wire snapshot ver=" << mirror.version()
              << " hash=" << mirror.hash() << "\n";

    std::cout << "\nDone.\n";
    return 0;
}

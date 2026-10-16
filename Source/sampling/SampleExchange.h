#pragma once
#include <atomic>
#include <memory>
namespace wee {
// Single loader producer / single audio consumer. The audio thread never destroys
// an asset. It defers a swap until the producer has collected the retired pointer.
template<class Asset> class SampleExchange {
    std::atomic<Asset*> pending {nullptr}, retired {nullptr};
    Asset* active = nullptr; // Audio-thread only until callbacks have stopped.
public:
    static_assert(std::atomic<Asset*>::is_always_lock_free);
    ~SampleExchange() { delete pending.load(); delete retired.load(); delete active; }
    SampleExchange() = default;
    SampleExchange(const SampleExchange&) = delete;
    SampleExchange& operator=(const SampleExchange&) = delete;
    void collect() noexcept { delete retired.exchange(nullptr); } // Loader only.
    void publish(std::unique_ptr<Asset> asset) noexcept { // Loader only.
        delete pending.exchange(asset.release());
        collect();
    }
    bool adopt() noexcept { // Audio only; called at a block boundary.
        if (retired.load() != nullptr) return false;
        auto* next = pending.exchange(nullptr);
        if (!next) return false;
        auto* old = active; active = next;
        if (old) retired.store(old);
        return true;
    }
    const Asset* current() const noexcept { return active; } // Audio only.
};
}

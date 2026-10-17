#ifndef DUST3D_BASE_RETAINED_PAYLOAD_BUDGET_H_
#define DUST3D_BASE_RETAINED_PAYLOAD_BUDGET_H_

#include <array>
#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dust3d {

namespace retained_payload_detail {
    template <typename T>
    struct IsInlinePayload;
    template <typename T>
    struct InlinePayload : std::bool_constant<std::is_trivially_destructible_v<T> && !std::is_pointer_v<T>> { };
    template <typename First, typename Second>
    struct InlinePayload<std::pair<First, Second>> : std::conjunction<IsInlinePayload<First>, IsInlinePayload<Second>> { };
    template <typename T, size_t Count>
    struct InlinePayload<std::array<T, Count>> : IsInlinePayload<T> { };
    template <typename T>
    struct IsInlinePayload : InlinePayload<std::remove_cv_t<T>> { };
}

/// Allocation-free, conservative accounting of owned object/value/buffer payloads.
/// Container-node/allocator bookkeeping, temporary work and process RSS are excluded.
/// Strings include capacity even for inline storage, which safely overcounts small strings.
/// Standard-container allocators must be stateless; policies with owned state need
/// an explicit visitor. Policy accessors are not copied during inspection.
/// Inspected objects must stay alive and unchanged, with valid acyclic ownership.
/// Example: `RetainedPayloadBudget budget(limit); budget.accountObject(cache);`
/// Query `isExceeded()` before using `usedBytes()` as the complete payload estimate.
class RetainedPayloadBudget {
public:
    /// Establish a byte limit. Zero permits only empty dynamic payloads.
    /// @param maximumBytes Maximum accounted object/value/buffer bytes.
    explicit RetainedPayloadBudget(size_t maximumBytes) noexcept
        : m_maximumBytes(maximumBytes)
    {
    }

    /// Charge count * bytesPerItem, checking arithmetic before mutation.
    /// @param count Number of items.
    /// @param bytesPerItem Bytes per item; zero is valid.
    /// @return False on insufficient space or any earlier rejection; failure is sticky.
    bool tryAdd(size_t count, size_t bytesPerItem = 1) noexcept
    {
        if (m_isExceeded || (bytesPerItem && count > (m_maximumBytes - m_usedBytes) / bytesPerItem)) {
            m_isExceeded = true;
            return false;
        }
        m_usedBytes += count * bytesPerItem;
        return true;
    }

    /// Successfully charged bytes; a rejected addition leaves this count unchanged.
    size_t usedBytes() const noexcept { return m_usedBytes; }
    /// Whether any charge exceeded the limit. A fresh budget is required to retry.
    bool isExceeded() const noexcept { return m_isExceeded; }

    /// Charge one owned object's inline size and then its dynamic members.
    template <typename T>
    void accountObject(const T& value) noexcept
    {
        if (tryAdd(1, sizeof(T)))
            accountDynamic(value);
    }

    /// Classify an inherited raw pointer as owning its pointee.
    template <typename T>
    void accountOwnedPointer(const T* value) noexcept
    {
        if (value)
            accountObject(*value);
    }

    /// Classify a borrowed view. The enclosing object already includes its pointer.
    template <typename T>
    void accountBorrowedPointer(T*) noexcept { }

    /// Account dynamic fields whose inline storage was already charged by their owner.
    template <typename... Fields>
    void accountDynamicFields(const Fields&... fields) noexcept
    {
        (accountDynamic(fields), ...);
    }

    /// Trivial RAII leaf values have no owned allocation beyond their inline storage.
    template <typename T, std::enable_if_t<std::is_trivially_destructible_v<T>, int> = 0>
    void accountDynamic(const T&) noexcept
    {
        static_assert(!std::is_pointer_v<T>, "Classify raw pointers as owned or borrowed");
    }

    /// Nontrivial owners must provide a nonthrowing visitor rather than silently losing payloads.
    template <typename T, std::enable_if_t<!std::is_trivially_destructible_v<T>, int> = 0>
    void accountDynamic(const T& value) noexcept
    {
        static_assert(noexcept(value.accountDynamicPayload(*this)), "Payload visitors must not throw");
        if (!m_isExceeded)
            value.accountDynamicPayload(*this);
    }

    void accountDynamic(const std::string& value) noexcept
    {
        if (tryAdd(value.capacity()))
            tryAdd(1);
    }

    template <typename T, typename Allocator,
        std::enable_if_t<retained_payload_detail::IsInlinePayload<T>::value, int> = 0>
    void accountDynamic(const std::vector<T, Allocator>& value) noexcept
    {
        static_assert(std::is_empty_v<Allocator>, "Stateful allocators require explicit ownership accounting");
        tryAdd(value.capacity(), sizeof(T));
    }

    template <typename T, typename Allocator,
        std::enable_if_t<!retained_payload_detail::IsInlinePayload<T>::value, int> = 0>
    void accountDynamic(const std::vector<T, Allocator>& value) noexcept
    {
        static_assert(std::is_empty_v<Allocator>, "Stateful allocators require explicit ownership accounting");
        if (!tryAdd(value.capacity(), sizeof(T)))
            return;
        for (const auto& element : value) {
            accountDynamic(element);
            if (m_isExceeded)
                return;
        }
    }

    template <typename First, typename Second>
    void accountDynamic(const std::pair<First, Second>& value) noexcept
    {
        accountDynamicFields(value.first, value.second);
    }

    /// Fixed inline value arrays need no dynamic charge; new owning array shapes require an audit.
    template <typename T, size_t Count>
    void accountDynamic(const std::array<T, Count>&) noexcept
    {
        static_assert(retained_payload_detail::IsInlinePayload<T>::value,
            "Arrays with dynamic or pointer ownership require an explicit visitor");
    }

    template <typename T, typename Deleter>
    void accountDynamic(const std::unique_ptr<T, Deleter>& value) noexcept
    {
        static_assert(!std::is_array_v<T>, "Owned arrays require an explicit length");
        static_assert(!std::is_reference_v<Deleter>, "Borrowed deleter references require explicit ownership accounting");
        accountDeleter(value.get_deleter());
        accountOwnedPointer(value.get());
    }

    template <typename Key, typename Value, typename Compare, typename Allocator>
    void accountDynamic(const std::map<Key, Value, Compare, Allocator>& value) noexcept
    {
        static_assert(std::is_empty_v<Allocator>, "Stateful allocators require explicit ownership accounting");
        static_assert(std::is_trivially_destructible_v<Compare>, "Owned comparison policy state requires explicit accounting");
        accountElements(value);
    }

    template <typename Key, typename Compare, typename Allocator>
    void accountDynamic(const std::set<Key, Compare, Allocator>& value) noexcept
    {
        static_assert(std::is_empty_v<Allocator>, "Stateful allocators require explicit ownership accounting");
        static_assert(std::is_trivially_destructible_v<Compare>, "Owned comparison policy state requires explicit accounting");
        accountElements(value);
    }

    template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
    void accountDynamic(const std::unordered_map<Key, Value, Hash, Equal, Allocator>& value) noexcept
    {
        static_assert(std::is_empty_v<Allocator>, "Stateful allocators require explicit ownership accounting");
        static_assert(std::is_trivially_destructible_v<Hash> && std::is_trivially_destructible_v<Equal>,
            "Owned hash/equality policy state requires explicit accounting");
        if (tryAdd(value.bucket_count(), sizeof(void*)))
            accountElements(value);
    }

    template <typename T, typename Allocator>
    void accountDynamic(const std::list<T, Allocator>& value) noexcept
    {
        static_assert(std::is_empty_v<Allocator>, "Stateful allocators require explicit ownership accounting");
        accountElements(value);
    }

private:
    template <typename Deleter,
        std::enable_if_t<std::is_pointer_v<Deleter> && std::is_function_v<std::remove_pointer_t<Deleter>>, int> = 0>
    void accountDeleter(const Deleter& value) noexcept
    {
        accountBorrowedPointer(value);
    }

    template <typename Deleter,
        std::enable_if_t<!(std::is_pointer_v<Deleter> && std::is_function_v<std::remove_pointer_t<Deleter>>), int> = 0>
    void accountDeleter(const Deleter& value) noexcept
    {
        accountDynamic(value);
    }

    template <typename Container,
        std::enable_if_t<retained_payload_detail::IsInlinePayload<typename Container::value_type>::value, int> = 0>
    void accountElements(const Container& value) noexcept
    {
        tryAdd(value.size(), sizeof(typename Container::value_type));
    }

    template <typename Container,
        std::enable_if_t<!retained_payload_detail::IsInlinePayload<typename Container::value_type>::value, int> = 0>
    void accountElements(const Container& value) noexcept
    {
        if (!tryAdd(value.size(), sizeof(typename Container::value_type)))
            return;
        for (const auto& element : value) {
            accountDynamic(element);
            if (m_isExceeded)
                return;
        }
    }

    size_t m_maximumBytes;
    size_t m_usedBytes = 0;
    bool m_isExceeded = false;
};

}

#endif

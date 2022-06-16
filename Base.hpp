/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: ECS Base
 */

#pragma once

#include <Kube/Core/StaticSafeAllocator.hpp>

namespace kF::ECS
{
    /** @brief Allocator of the ECS library */
    using ECSAllocator = Core::StaticSafeAllocator<"ECSAllocator">;


    /** @brief Entity */
    using Entity = std::uint32_t;

    /** @brief Entity index */
    using EntityIndex = Entity;

    /** @brief Special null index */
    static constexpr EntityIndex NullEntityIndex = ~static_cast<EntityIndex>(0);


    /** @brief Entity index range */
    struct alignas_eighth_cacheline EntityRange
    {
        Entity begin {};
        Entity end {};

        /** @brief Comparison operators */
        [[nodiscard]] inline bool operator==(const EntityRange &other) const noexcept = default;
        [[nodiscard]] inline bool operator!=(const EntityRange &other) const noexcept = default;
    };

    /** @brief Number of bits in entity type */
    constexpr Entity EntityBitCount = sizeof(Entity) * 8;


    /** @brief Convert hertz into time rate */
    [[nodiscard]] constexpr std::int64_t HzToRate(const std::int64_t hertz) noexcept
        { return 1'000'000'000ll / hertz; }
}
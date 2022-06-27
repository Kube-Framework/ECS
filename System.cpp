/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: System
 */

#include "Executor.hpp"
#include "System.hpp"

using namespace kF;

ECS::Internal::ASystem::ASystem(void) noexcept
    : _parent(&ECS::Executor::Get()), _graph(Flow::GraphPtr::Make())
{
}

ECS::Entity ECS::Internal::ASystem::add(void) noexcept
{
    Entity entity;

    if (_freeEntities.empty()) [[likely]]
        entity = ++_lastEntity;
    else {
        auto &freeRange = _freeEntities.front();
        entity = freeRange.begin++;
        if (freeRange.begin == freeRange.end) [[unlikely]]
            _freeEntities.erase(_freeEntities.begin());
    }
    return entity;
}

ECS::EntityRange ECS::Internal::ASystem::addRange(const Entity count) noexcept
{
    auto it = _freeEntities.begin();
    auto end = _freeEntities.end();
    EntityRange range;

    while (true) {
        // End of free entities reached
        if (it == end) [[likely]] {
            range.begin = _lastEntity + 1;
            range.end = range.begin + count;
            _lastEntity += count;
            break;
        }
        // Test available entity range
        auto &freeRange = *it;
        // If the range is too small, continue
        if (freeRange.end - freeRange.begin < count) [[likely]] {
            ++it;
            continue;
        }
        // Else, allocate the requested range
        range.begin = freeRange.begin;
        range.end = range.begin + count;
        freeRange.begin += count;
        if (freeRange.begin == freeRange.end) [[unlikely]]
            _freeEntities.erase(it); // It is safe to ignore iterators invalidation since we break
        break;
    }
    return range;
}

void ECS::Internal::ASystem::remove(const Entity entity) noexcept
{
    if (entity == _lastEntity) [[likely]]
        --_lastEntity;
    else {
        for (auto &freeRange : _freeEntities) {
            if (freeRange.begin - 1 != entity && freeRange.end != entity) [[likely]]
                continue;
            else if (freeRange.end == entity)
                ++freeRange.end;
            else
                --freeRange.begin;
            return;
        }
        _freeEntities.push(EntityRange { .begin = entity, .end = entity + 1 });
    }
}

void ECS::Internal::ASystem::removeRange(const EntityRange range) noexcept
{
    if (range.end - 1 == _lastEntity) [[likely]]
        _lastEntity = range.begin - 1;
    else {
        for (auto &freeRange : _freeEntities) {
            if (freeRange.begin != range.end && freeRange.end != range.begin) [[likely]]
                continue;
            else if (freeRange.end == range.begin)
                freeRange.end = range.end;
            else
                freeRange.begin = range.begin;
            return;
        }
        _freeEntities.push(range);
    }
}

void ECS::Internal::ASystem::queryPipelineIndex(const Core::HashedName pipelineHash) noexcept
{
    const auto expected = parent().getPipelineIndex(pipelineHash);
    kFEnsure(expected.success(),
        "ECS::System::queryPipelineIndex: System '", systemName(), "' is being instantiated but its pipeline '", pipelineName(), "' was not registered");
    _executorPipelineIndex = *expected;
    _isTimeBound = parent().isPipelineTimeBound(_executorPipelineIndex);
    _tickRate = parent().getPipelineTickRate(_executorPipelineIndex);
}

Core::Expected<std::uint32_t> ECS::Internal::ASystem::getPipelineIndex(const Core::HashedName pipelineHash) const noexcept
{
    return parent().getPipelineIndex(pipelineHash);
}

ECS::Internal::ASystem *ECS::Internal::ASystem::getSystemOpaque(const std::uint32_t pipelineIndex, const Core::HashedName systemName) noexcept
{
    if (const auto systemIndex = parent().getSystemIndex(pipelineIndex, systemName); systemIndex.success()) [[likely]]
        return parent().getSystemOpaque(pipelineIndex, systemIndex.value());
    else [[unlikely]]
        return nullptr;
}

#include <functional>

void ECS::Internal::ASystem::sendEventOpaque(const std::uint32_t pipelineIndex, Core::Functor<void(void), ECSAllocator> &&callback) noexcept
{
    static_assert(std::is_same_v<Core::Functor<void(void), ECSAllocator>, Executor::PipelineEvent>,
        "Please update this function to keep perfect functor forwarding");

    parent().sendEvent(pipelineIndex, std::move(callback));
}

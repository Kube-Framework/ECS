/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: Pipeline
 */

#include "ComponentTable.hpp"

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename ...Args>
inline ComponentType &kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::add(const Entity entity, Args &&...args) noexcept
{
    kFAssert(!exists(entity),
        "ECS::ComponentTable::add: Entity '", entity, "' already exists");

    const auto componentIndex = _entities.size();
    _indexSet.add(entity, componentIndex);
    _entities.push(entity);
    return _components.push(std::forward<Args>(args)...);
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline ComponentType &kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::addUpdate(const Entity entity, ComponentType &&component) noexcept
{
    if (auto componentIndex = findIndex(entity); componentIndex != NullIndex) [[likely]] {
        return get(entity) = std::move(component);
    } else {
        componentIndex = _entities.size();
        _indexSet.add(entity, componentIndex);
        _entities.push(entity);
        return _components.push(std::move(component));
    }
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename Functor>
inline ComponentType &kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::addUpdate(const Entity entity, Functor &&functor) noexcept
{
    ComponentType *ptr;
    if (auto componentIndex = findIndex(entity); componentIndex != NullIndex) [[likely]] {
        ptr = &get(entity);
    } else {
        componentIndex = _entities.size();
        _indexSet.add(entity, componentIndex);
        _entities.push(entity);
        ptr = &_components.push();
    }
    functor(*ptr);
    return *ptr;
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::addRange(const EntityRange range, const ComponentType &component) noexcept
{
    const auto lastIndex = _entities.size();
    const auto count = range.end - range.begin;

    if constexpr (KUBE_DEBUG_BUILD) {
        // Ensure no entity exists in range
        for (const auto entity : _entities) {
            kFEnsure(entity < range.begin || entity >= range.end,
                "ECS::ComponentTable::addRange: Entity '", entity, "' from entity range [", range.begin, ", ", range.end, "[ already exists");
        }
    }

    // Insert entities & indexes
    _entities.insertDefault(_entities.end(), count);
    for (Entity i = lastIndex, it = range.begin; it != range.end; ++i, ++it) {
        _entities[i] = it;
    }
    for (Entity i = lastIndex, it = range.begin; it != range.end; ++i, ++it) {
        _indexSet.add(it, i);
    }

    // Insert components
    _components.insertCopy(_components.end(), count, component);
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::remove(const Entity entity) noexcept
{
    kFAssert(exists(entity),
        "ECS::ComponentTable::remove: Entity '", entity, "' doesn't exists");
    const auto pageIndex = IndexSparseSet::GetPageIndex(entity);
    const auto elementIndex = IndexSparseSet::GetElementIndex(entity);
    const auto componentIndex = _indexSet.extract(pageIndex, elementIndex);
    removeImpl(entity, componentIndex);
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::tryRemove(const Entity entity) noexcept
{
    if (const auto componentIndex = findIndex(entity); componentIndex != NullIndex) [[likely]]
        removeImpl(entity, componentIndex);
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::removeImpl(const Entity entity, const EntityIndex componentIndex) noexcept
{
    if (_components.size() != componentIndex + 1) [[likely]] {
        const auto lastEntity = _entities.back();
        _indexSet.at(lastEntity) = componentIndex;
        _entities.at(componentIndex) = lastEntity;
        _components.at(componentIndex) = std::move(_components.back());
    }
    _indexSet.remove(entity);
    _entities.pop();
    _components.pop();
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::removeRange(const EntityRange range) noexcept
{
    Core::SmallVector<Entity, 128, Allocator, Entity> indexes;

    // Retreive erased indexes
    for (Entity i = 0; auto entity : _entities) {
        ++i;
        if (entity >= range.begin && entity < range.end) [[unlikely]] {
            indexes.push(i - 1);
            _indexSet.remove(entity);
        }
    }

    if (indexes.empty()) [[likely]]
        return;

    // Sort indexes in descent order
    std::sort(indexes.rbegin(), indexes.rend());

    Core::SmallVector<EntityRange, 128, Allocator, Entity> moveIndexes;

    // Retreive move indexes
    auto last = _entities.size() - 1;
    for (const auto index : indexes) {
        if (index != last) [[likely]]
            moveIndexes.push(EntityRange { .begin = last, .end = index });
        --last;
    }

    // Erase entities
    for (const auto moveIndex : moveIndexes) {
        const auto movedEntity = _entities[moveIndex.begin];
        _entities[moveIndex.end] = movedEntity;
        _indexSet.at(movedEntity) = moveIndex.end;
    }
    _entities.erase(_entities.begin() + last + 1, _entities.end());

    // Erase components
    for (const auto moveIndex : moveIndexes) {
        _components.at(moveIndex.end) = std::move(_components.at(moveIndex.begin));
    }
    _components.erase(_components.begin() + last + 1, _components.end());
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline ComponentType kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::extract(const Entity entity) noexcept
{
    kFAssert(exists(entity),
        "ECS::ComponentTable::remove: Entity '", entity, "' doesn't exists");

    const auto pageIndex = IndexSparseSet::GetPageIndex(entity);
    const auto elementIndex = IndexSparseSet::GetElementIndex(entity);
    const auto componentIndex = _indexSet.extract(pageIndex, elementIndex);
    ComponentType value(std::move(_components.at(componentIndex)));

    if (_components.size() != componentIndex + 1) [[likely]] {
        const auto lastEntity = _entities.back();
        _indexSet.at(lastEntity) = componentIndex;
        _entities.at(componentIndex) = lastEntity;
        _components.at(componentIndex) = std::move(_components.back());
    }
    _indexSet.remove(entity);
    _entities.pop();
    _components.pop();
    return value;
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::clear(void) noexcept
{
    if constexpr (IndexSparseSet::IsSafeToClear) {
        _indexSet.clearUnsafe();
    } else {
        for (const auto entity : _entities)
            _indexSet.remove(entity);
    }
    _entities.clear();
    _components.clear();
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::release(void) noexcept
{
    if constexpr (IndexSparseSet::IsSafeToClear) {
        _indexSet.releaseUnsafe();
    } else {
        for (const auto entity : _entities)
            _indexSet.remove(entity);
    }
    _entities.release();
    _components.release();
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline kF::ECS::EntityIndex kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::findIndex(const Entity entity) const noexcept
{
    const auto it = _entities.find(entity);
    if (it != _entities.end()) [[likely]]
        return static_cast<Entity>(std::distance(_entities.begin(), it));
    else [[unlikely]]
        return NullIndex;
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename CompareFunctor>
inline void kF::ECS::ComponentTable<ComponentType, EntityPageSize, Allocator>::sort(CompareFunctor &&compareFunc) noexcept
{
    // Sort entities
    std::sort(_entities.begin(), _entities.end(), std::forward<CompareFunctor>(compareFunc));

    // Apply sort patch to components & sparse set
    for (std::uint32_t from {}, to = _entities.size(); from != to; ++from) {
        auto current = from;
        auto currentEntity = _entities.at(current);
        std::uint32_t next = _indexSet.at(currentEntity);

        while (current != next) [[unlikely]] {
            const auto nextEntity = _entities.at(next);
            const auto following = _indexSet.at(nextEntity);
            std::swap(_components.at(next), _components.at(following));
            _indexSet.at(currentEntity) = current;
            current = std::exchange(next, following);
            currentEntity = _entities.at(current);
        }
    }
}
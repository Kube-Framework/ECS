/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: Pipeline
 */

#include <Kube/Core/SmallVector.hpp>

#include "StableComponentTable.hpp"

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename Type>
inline kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::IteratorType<Type> &
    kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::IteratorType<Type>::operator++(void) noexcept
{
    const auto &entities = _table->entities();
    const auto max = entities.size();

    while (true) {
        if (_index == max)
            break;
        ++_index;
        if (entities.at(_index) != NullEntityIndex)
            break;
    }
    return *this;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename Type>
inline kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::IteratorType<Type> &
    kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::IteratorType<Type>::operator--(void) noexcept
{
    const auto max = _entities.size();

    while (true) {
        if (_index == 0u)
            break;
        --_index;
        if (_entities.at(_index) != NullEntityIndex)
            break;
    }
    return *this;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::~StableComponentTable(void) noexcept
{
    destroyComponents();
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::pack(void) noexcept
{
    // If there are no component to pack, return now
    if (_tombstones.empty()) [[likely]]
        return;

    // Sort tombstones in descending order
    std::sort(_tombstones.rbegin(), _tombstones.rend());

    Core::SmallVector<EntityRange, 128, Allocator, Entity> moveIndexes;

    // Retreive move indexes from tombstones
    auto last = _entities.size() - 1;
    for (const auto index : _tombstones) {
        if (index != last) [[likely]]
            moveIndexes.push(EntityRange { .begin = last, .end = index });
        --last;
    }

    // Clear tombstones
    _tombstones.clear();

    // Erase entities
    for (const auto moveIndex : moveIndexes) {
        const auto movedEntity = _entities[moveIndex.begin];
        _entities[moveIndex.end] = movedEntity;
        _indexSet.at(movedEntity) = moveIndex.end;
    }
    const auto from = last + 1; // If '--last' did overflow, re-overflow in the other side
    _entities.erase(_entities.begin() + from, _entities.end());

    // Erase components
    for (const auto moveIndex : moveIndexes) {
        auto &toMove = atIndex(moveIndex.begin);
        new (&atIndex(moveIndex.end)) ComponentType(std::move(toMove));
        toMove.~ComponentType();
    }
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename ...Args>
inline ComponentType &kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::add(const Entity entity, Args &&...args) noexcept
{
    kFAssert(!exists(entity),
        "ECS::StableComponentTable::add: Entity '", entity, "' already exists");

    return addImpl(entity, std::forward<Args>(args)...);
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline ComponentType &kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::tryAdd(const Entity entity, ComponentType &&component) noexcept
{
    if (auto componentIndex = findIndex(entity); componentIndex != NullEntityIndex) [[likely]] {
        return get(entity) = std::move(component);
    } else {
        return addImpl(entity, std::move(component));
    }
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename Functor>
inline ComponentType &kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::tryAdd(const Entity entity, Functor &&functor) noexcept
{
    ComponentType *ptr;
    if (const auto componentIndex = findIndex(entity); componentIndex != NullEntityIndex) [[likely]] {
        ptr = &atIndex(componentIndex);
    } else {
        ptr = &addImpl(entity);
    }
    functor(*ptr);
    return *ptr;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename ...Args>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::addRange(const EntityRange range, const Args &...args) noexcept
{
    const auto lastIndex = _entities.size();
    const auto count = range.end - range.begin;

    if constexpr (KUBE_DEBUG_BUILD) {
        // Ensure no entity exists in range
        for (const auto entity : _entities) {
            kFEnsure(entity < range.begin || entity >= range.end,
                "ECS::StableComponentTable::addRange: Entity '", entity, "' from entity range [", range.begin, ", ", range.end, "[ already exists");
        }
    }

    // Insert entities
    _entities.insertCustom(_entities.end(), count, [range](const auto count, const auto out) {
        for (EntityIndex i = 0; i != count; ++i)
            out[i] = range.begin + i;
    });

    // Insert indexes
    for (EntityIndex i = lastIndex, it = range.begin; it != range.end; ++i, ++it) {
        _indexSet.add(it, i);
    }

    // Insert components
    for (EntityIndex i = lastIndex, it = range.begin; it != range.end; ++i, ++it) {
        insertComponent(i, args...);
    }
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename ...Args>
inline ComponentType &kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::addImpl(const Entity entity, Args &&...args) noexcept
{
    // Find a suitable component index
    ECS::EntityIndex entityIndex;
    if (_tombstones.empty()) {
        entityIndex = _entities.size();
        _entities.push(entity);
    } else {
        entityIndex = _tombstones.back();
        _tombstones.pop();
        _entities.at(entityIndex) = entity;
    }
    // Insert at index
    _indexSet.add(entity, entityIndex);
    return insertComponent(entityIndex, std::forward<Args>(args)...);
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename ...Args>
inline ComponentType &kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::insertComponent(const EntityIndex entityIndex, Args &&...args) noexcept
{
    const auto pageIndex = GetPageIndex(entityIndex);
    const auto componentIndex = GetComponentIndex(entityIndex);

    // Ensure destination page exists
    while (!pageExists(pageIndex)) [[unlikely]]
        _componentPages.push(ComponentPagePtr::Make());
    // Construct component
    return *new (_componentPages.at(pageIndex)->data() + componentIndex) ComponentType(std::forward<Args>(args)...);
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::remove(const Entity entity) noexcept
{
    kFAssert(exists(entity),
        "ECS::StableComponentTable::remove: Entity '", entity, "' doesn't exists");
    removeImpl(entity, _indexSet.extract(entity));
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline bool kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::tryRemove(const Entity entity) noexcept
{
    if (const auto entityIndex = findIndex(entity); entityIndex != NullEntityIndex) [[likely]] {
        removeImpl(entity, entityIndex);
        return true;
    } else
        return false;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::removeRange(const EntityRange range) noexcept
{
    for (auto it = range.begin; it != range.end; ++it)
        removeImpl(it, _indexSet.at(it));
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::removeImpl(const Entity entity, const EntityIndex entityIndex) noexcept
{
    { // Remove component
        const auto pageIndex = GetPageIndex(entityIndex);
        const auto componentIndex = GetComponentIndex(entityIndex);
        _componentPages.at(pageIndex)->data()[componentIndex].~ComponentType();
    }

    // Remove index
    _indexSet.remove(entity);

    // Remove entity
    _entities.at(entityIndex) = ECS::NullEntity;

    // Add tombstone
    _tombstones.push(entityIndex);
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline ComponentType kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::extract(const Entity entity) noexcept
{
    kFAssert(exists(entity),
        "ECS::StableComponentTable::remove: Entity '", entity, "' doesn't exists");

    const auto entityIndex = _indexSet.extract(entity);
    ComponentType value(std::move(atIndex(entityIndex)));

    removeImpl(entity, entityIndex);
    return value;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline const ComponentType &kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::atIndex(const EntityIndex entityIndex) const noexcept
{
    const auto pageIndex = GetPageIndex(entityIndex);
    const auto componentIndex = GetComponentIndex(entityIndex);

    return _componentPages.at(pageIndex)->data()[componentIndex];
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline kF::ECS::EntityIndex kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::getUnstableIndex(const Entity entity) const noexcept
{
    if (_indexSet.pageExists(entity)) [[likely]]
        return _indexSet.at(entity);
    else
        return NullEntityIndex;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::clear(void) noexcept
{
    destroyComponents();
    _entities.clear();
    _indexSet.clearUnsafe();
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::release(void) noexcept
{
    destroyComponents();
    _componentPages.release();
    _entities.release();
    _indexSet.releaseUnsafe();
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::destroyComponents(void) noexcept
{
    for (ECS::EntityIndex index {}; const auto entity : _entities) {
        if (entity != ECS::NullEntity) [[likely]]
            atIndex(index).~ComponentType();
        ++index;
    }
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
inline kF::ECS::EntityIndex kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::findIndex(const Entity entity) const noexcept
{
    const auto it = _entities.find(entity);
    if (it != _entities.end()) [[likely]]
        return static_cast<Entity>(std::distance(_entities.begin(), it));
    else [[unlikely]]
        return NullEntityIndex;
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename CompareFunctor>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::sort(CompareFunctor &&compareFunc) noexcept
{
    // Pack before sorting
    pack();

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
            std::swap(atIndex(next), atIndex(following));
            _indexSet.at(currentEntity) = current;
            current = std::exchange(next, following);
            currentEntity = _entities.at(current);
        }
    }
}

template<typename ComponentType, kF::ECS::EntityIndex ComponentPageSize, kF::ECS::EntityIndex EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
template<typename Callback>
    requires std::is_invocable_v<Callback, ComponentType &>
        || std::is_invocable_v<Callback, kF::ECS::Entity>
        || std::is_invocable_v<Callback, kF::ECS::Entity, ComponentType &>
inline void kF::ECS::StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>::traverse(Callback &&callback) noexcept
{
    for (EntityIndex index {}, count = _entities.size(); index != count; ++index) {
        const auto entity = _entities[index];
        if (entity == NullEntity) [[unlikely]]
            continue;
        // Entity & component
        if constexpr (std::is_invocable_v<Callback, Entity, ComponentType &>) {
            auto &component = atIndex(index);
            if constexpr (std::is_same_v<std::invoke_result_t<Callback, Entity, ComponentType &>, bool>) {
                if (!callback(entity, component))
                    break;
            } else
                callback(entity, component);
        // Component only
        } else if constexpr (std::is_invocable_v<Callback, ComponentType &>) {
            auto &component = atIndex(index);
            if constexpr (std::is_same_v<std::invoke_result_t<Callback, ComponentType &>, bool>) {
                if (!callback(component))
                    break;
            } else
                callback(component);
        // Entity only
        } else {
            if constexpr (std::is_same_v<std::invoke_result_t<Callback, Entity>, bool>) {
                if (!callback(entity))
                    break;
            } else
                callback(entity);
        }
    }
}
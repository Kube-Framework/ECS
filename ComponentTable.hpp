/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: Pipeline
 */

#pragma once

#include <Kube/Core/SparseSet.hpp>

#include "Base.hpp"

namespace kF::ECS
{
    template<typename ComponentType, Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator>
    class ComponentTable;
}

template<typename ComponentType, kF::ECS::Entity EntityPageSize, kF::Core::StaticAllocatorRequirements Allocator = kF::Core::DefaultStaticAllocator>
class alignas_cacheline kF::ECS::ComponentTable
{
public:
    /** @brief Initializer of entity indexes */
    static constexpr auto EntityIndexInitializer = [](EntityIndex * const begin, EntityIndex * const end) {
        std::fill(begin, end, NullEntityIndex);
    };

    /** @brief Type of stored component */
    using ValueType = ComponentType;

    /** @brief Sparse set that stores indexes of entities' components */
    using IndexSparseSet = Core::SparseSet<Entity, EntityPageSize, Allocator, EntityIndex, EntityIndexInitializer>;

    /** @brief List of active entities */
    using ActiveEntities = Core::Vector<Entity, Allocator, EntityIndex>;

    /** @brief List of entities' components */
    using Components = Core::Vector<ComponentType, Allocator, EntityIndex>;

    static_assert(IndexSparseSet::IsSafeToClear, "There are no reason why index sparse set could not be safely cleared");


    /** @brief Get the number of components inside the table */
    [[nodiscard]] EntityIndex count(void) const noexcept { return _entities.size(); }

    /** @brief Check if an entity exists in the sparse set */
    [[nodiscard]] inline bool exists(const Entity entity) const noexcept
        { return _entities.find(entity) != _entities.end(); }


    /** @brief Add a component into the table */
    template<typename ...Args>
    ComponentType &add(const Entity entity, Args &&...args) noexcept;

    /** @brief Try to add a component into the table
     *  @note If the entity already attached 'ComponentType', the old value is updated */
    ComponentType &addUpdate(const Entity entity, ComponentType &&component) noexcept;

    /** @brief Try to update component of an entity
     *  @note If a component doesn't exists, it is created */
    template<typename Functor>
    ComponentType &addUpdate(const Entity entity, Functor &&functor) noexcept;

    /** @brief Add a range of components into the table */
    void addRange(const EntityRange range, const ComponentType &component) noexcept;


    /** @brief Remove a component from the table
     *  @note The entity must be inside table else its an undefined behavior (use exists to check if an entity is registered) */
    void remove(const Entity entity) noexcept;

    /** @brief Try to remove a component from the table
     *  @note The entity can be inside table, if it isn't this function does nothing */
    void tryRemove(const Entity entity) noexcept;

    /** @brief Remove a range of components from the table
     *  @note The range of entities can be inside table, if none are present this function does nothing */
    void removeRange(const EntityRange range) noexcept;


    /** @brief Extract and remove a component into the table
     *  @note The entity must be inside table else its an undefined behavior (use exists to check if an entity is registered) */
    [[nodiscard]] ComponentType extract(const Entity entity) noexcept;


    /** @brief Get an entity's component */
    [[nodiscard]] inline ComponentType &get(const Entity entity) noexcept { return _components.at(_indexSet.at(entity)); }
    [[nodiscard]] inline const ComponentType &get(const Entity entity) const noexcept { return _components.at(_indexSet.at(entity)); }


    /** @brief Get the unstable index of an entity (NullEntityIndex if not found) */
    [[nodiscard]] EntityIndex getUnstableIndex(const Entity entity) const noexcept;

    /** @brief Get an entity's component using its unstable index */
    [[nodiscard]] inline ComponentType &atIndex(const EntityIndex entityIndex) noexcept { return _components.at(entityIndex); }
    [[nodiscard]] inline const ComponentType &atIndex(const EntityIndex entityIndex) const noexcept { return _components.at(entityIndex); }


    /** @brief Components begin / end iterators */
    [[nodiscard]] inline auto begin(void) noexcept { return _components.begin(); }
    [[nodiscard]] inline auto begin(void) const noexcept { return _components.begin(); }
    [[nodiscard]] inline auto end(void) noexcept { return _components.end(); }
    [[nodiscard]] inline auto end(void) const noexcept { return _components.end(); }

    /** @brief Components reverse begin / end iterators */
    [[nodiscard]] inline auto rbegin(void) noexcept { return _components.rbegin(); }
    [[nodiscard]] inline auto rbegin(void) const noexcept { return _components.rbegin(); }
    [[nodiscard]] inline auto rend(void) noexcept { return _components.rend(); }
    [[nodiscard]] inline auto rend(void) const noexcept { return _components.rend(); }


    /** @brief Get registered entity list */
    [[nodiscard]] inline const auto &entities(void) const noexcept { return _entities; }


    /** @brief Sort the table using a custom functor
     *  @note CompareFunctor must have the following signature: bool(Entity, Entity) */
    template<typename CompareFunctor>
    void sort(CompareFunctor &&compareFunc) noexcept;


    /** @brief Clear the table */
    void clear(void) noexcept;

    /** @brief Release the table */
    void release(void) noexcept;

private:
    IndexSparseSet _indexSet {};
    ActiveEntities _entities {};
    Components _components {};


    /** @brief Check if an entity exists in the sparse set */
    [[nodiscard]] EntityIndex findIndex(const Entity entity) const noexcept;

    /** @brief Hiden implementation of remove function */
    void removeImpl(const Entity entity, const EntityIndex componentIndex) noexcept;
};

#include "ComponentTable.ipp"
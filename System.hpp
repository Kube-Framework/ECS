/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: System
 */

#pragma once

#include <Kube/Core/TupleUtils.hpp>
#include <Kube/Core/Hash.hpp>
#include <Kube/Core/Functor.hpp>
#include <Kube/Core/Expected.hpp>
#include <Kube/Flow/Graph.hpp>

#include "Base.hpp"
#include "Pipeline.hpp"
#include "ComponentTable.hpp"
#include "StableComponentTable.hpp"

namespace kF::ECS
{
    class Executor;

    /** @brief Component stable tag (use StableComponentTable) */
    template<typename ComponentType, EntityIndex ComponentPageSize = 4096 / sizeof(ComponentType)>
    struct StableComponent
    {
        /** @brief Underyling type */
        using ValueType = ComponentType;

        /** @brief Underyling page size */
        static constexpr auto PageSize = ComponentPageSize;
    };

    namespace Internal
    {
        /**
         * @brief Interface class of a system
         * @note Vtable offset is platform specific, this is the only way of
         * ensuring the layout but prevent some optimisation*/
        class ISystem
        {
        public:
            /** @brief Virtual destructor */
            virtual ~ISystem(void) noexcept = default;

            /** @brief Virtual system tick function  */
            [[nodiscard]] virtual bool tick(void) noexcept = 0;

            /** @brief Virtual pipeline name get  */
            [[nodiscard]] virtual std::string_view pipelineName(void) const noexcept = 0;

            /** @brief Virtual system name get  */
            [[nodiscard]] virtual std::string_view systemName(void) const noexcept = 0;
        };

        class ASystem;


        /** @brief Forward component base */
        template<typename ComponentType>
        struct ForwardComponent;

        /** @brief Forward component common case */
        template<typename ComponentType>
        struct ForwardComponent
        {
            using Type = ComponentType;
        };

        /** @brief Forward component stable tag*/
        template<typename ComponentType, EntityIndex ComponentPageSize>
        struct ForwardComponent<StableComponent<ComponentType, ComponentPageSize>> : ForwardComponent<ComponentType> {};


        /** @brief Forward table base */
        template<typename ComponentType, EntityIndex EntityPageSize, typename Allocator>
        struct ForwardComponentTable;

        /** @brief Forward table common case */
        template<typename ComponentType, EntityIndex EntityPageSize, typename Allocator>
        struct ForwardComponentTable
        {
            using Type = ComponentTable<ComponentType, EntityPageSize, Allocator>;
        };

        /** @brief Forward table stable tag*/
        template<typename ComponentType, EntityIndex ComponentPageSize, EntityIndex EntityPageSize, typename Allocator>
        struct ForwardComponentTable<StableComponent<ComponentType, ComponentPageSize>, EntityPageSize, Allocator>
        {
            using Type = StableComponentTable<ComponentType, ComponentPageSize, EntityPageSize, Allocator>;
        };


        /** @brief Tuple of forwarded components */
        template<typename ...ComponentTypes>
        using ForwardComponentsTuple = std::tuple<typename ForwardComponent<ComponentTypes>::Type...>;
    }

    template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::StaticAllocatorRequirements Allocator, typename ...ComponentTypes>
    class System;


    /** @brief Concept that ensure multiple Components match a ComponentsTuple */
    template<typename ComponentsTuple, typename ...Components>
    concept SystemComponentRequirements = (Core::TupleContainsElement<std::remove_cvref_t<Components>, ComponentsTuple> && ...);
}

/** @brief Abstract class of any system */
class alignas_cacheline kF::ECS::Internal::ASystem : public ISystem
{
public:
    /** @brief Virtual destructor */
    virtual ~ASystem(void) noexcept override = default;

    /** @brief Default constructor */
    ASystem(void) noexcept;


    /** @brief Virtual system tick function  */
    [[nodiscard]] virtual bool tick(void) noexcept { return true; }


    /** @brief Get parent executor reference */
    [[nodiscard]] inline Executor &parent(void) const noexcept { return *_parent; }


    /** @brief Get executor pipeline index */
    [[nodiscard]] inline std::uint32_t executorPipelineIndex(void) const noexcept { return _executorPipelineIndex; }


    /** @brief Get internal task graph reference */
    [[nodiscard]] inline Flow::Graph &taskGraph(void) noexcept { return *_graph; }
    [[nodiscard]] inline const Flow::Graph &taskGraph(void) const noexcept { return *_graph; }


    /** @brief Get tick rate */
    [[nodiscard]] inline std::int64_t tickRate(void) const noexcept { return _tickRate; }

    /** @brief Get tick rate */
    [[nodiscard]] inline bool isTimeBound(void) const noexcept { return _isTimeBound; }


    /** @brief Function called whenever executor changes attached pipeline tick rate */
    inline void onTickRateChanged(const std::int64_t tickRate) noexcept { _tickRate = tickRate; }

    /** @brief Set executor pipeline index */
    void queryPipelineIndex(const Core::HashedName pipelineHash) noexcept;


    /** @brief Creates an entity */
    [[nodiscard]] Entity add(void) noexcept;

    /** @brief Creates a range of entities */
    [[nodiscard]] EntityRange addRange(const Entity count) noexcept;

    /** @brief Removes an entity */
    void remove(const Entity entity) noexcept;

    /** @brief Removes a range of entities */
    void removeRange(const EntityRange range) noexcept;

protected:
    /** @brief Get pipeline index from pipeline runtime name */
    [[nodiscard]] Core::Expected<std::uint32_t> getPipelineIndex(const Core::HashedName pipelineHash) const noexcept;

    /** @brief Get opaque system using pipeline index & system hashed name
     *  @return Returns nullptr if system doesn't exist */
    [[nodiscard]] ASystem *getSystemOpaque(const std::uint32_t pipelineIndex, const Core::HashedName systemName) noexcept;

    /** @brief Send event using pipeline hashed name */
    void sendEventOpaque(const std::uint32_t pipelineIndex, Core::Functor<void(void), ECSAllocator> &&callback) noexcept;


private:
    // Cacheline 0
    // vtable pointer
    Executor *_parent {};
    std::uint32_t _executorPipelineIndex {};
    bool _isTimeBound {};
    std::int64_t _tickRate {};
    Flow::GraphPtr _graph {};
    Entity _lastEntity {};
    Core::Vector<EntityRange, ECSAllocator> _freeEntities {};
};
static_assert_alignof_cacheline(kF::ECS::Internal::ASystem);
static_assert_sizeof_cacheline(kF::ECS::Internal::ASystem);


/** @brief Abstract class of any system that contains meta data about the system
 *  @tparam Literal Name of the System
 *  @tparam Pipeline System's execution pipeline */
template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::StaticAllocatorRequirements Allocator =
        kF::Core::DefaultStaticAllocator, typename ...ComponentTypes>
class alignas(sizeof...(ComponentTypes) ? kF::Core::CacheLineDoubleSize : kF::Core::CacheLineSize)
        kF::ECS::System : public Internal::ASystem
{
public:
    /** @brief Name of the system */
    static constexpr auto Name = Literal.toView();

    /** @brief Hashed name of the system */
    static constexpr auto Hash = Core::Hash(Name);

    /** @brief Entity page size */
    static constexpr Entity EntityPageSize = 4096 / sizeof(kF::ECS::Entity);

    /** @brief Execution pipeline of the system */
    using ExecutorPipeline = TargetPipeline;


    /** @brief Static component list */
    using ComponentsTuple = Internal::ForwardComponentsTuple<ComponentTypes...>;

    /** @brief Static component table list */
    using ComponentTablesTuple = std::tuple<typename Internal::ForwardComponentTable<ComponentTypes, EntityPageSize, Allocator>::Type...>;

    /** @brief Number of component tables in this system */
    static constexpr std::size_t ComponentCount = sizeof...(ComponentTypes);

    /** @brief Entity mask of this system */
    static constexpr Entity EntityMask = ~static_cast<Entity>(0) >> static_cast<Entity>(ComponentCount);

    /** @brief Component mask of this system */
    static constexpr Entity ComponentMask = ~EntityMask;

    static_assert(ComponentCount <= EntityBitCount / 2, "ECS::System: Component count cannot exceed half of ECS::Entity bits");


    /** @brief Virtual destructor */
    virtual ~System(void) noexcept override = default;

    /** @brief Default constructor */
    System(void) noexcept { queryPipelineIndex(ExecutorPipeline::Hash); }


    /** @brief Virtual pipeline name getter */
    [[nodiscard]] constexpr std::string_view pipelineName(void) const noexcept final { return ExecutorPipeline::Name; }

    /** @brief Virtual system name getter */
    [[nodiscard]] constexpr std::string_view systemName(void) const noexcept final { return Name; }


    /** @brief Interact with another system using 'this'
     *  @note The callback functor must have a system reference as argument : void(auto &system)
     *  @note If 'this' system and target system are not on the same pipeline, an event is sent to the target pipeline */
    template<typename Callback>
    void interact(Callback &&callback) const noexcept;

    /** @brief Interact with another system using 'this'
     *  @note The callback functor may have a system reference as argument : void(void) | void(auto &system)
     *  @note If 'this' system pipeline is not the same as target pipeline, an event is sent to the target pipeline
     *  @tparam DestinationPipeline Pipeline to which belong the callback */
    template<typename DestinationPipeline, typename Callback>
    void interact(Callback &&callback) const noexcept;


    /** @brief Pack stable component tables */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void pack(void) noexcept;


    /** @brief Creates an entity */
    using Internal::ASystem::add;

    /** @brief Creates an entity with components */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    Entity add(Components &&...components) noexcept;

    /** @brief Creates a range of entities */
    using Internal::ASystem::addRange;

    /** @brief Creates a range of entities with components */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    EntityRange addRange(const Entity count, Components &&...components) noexcept;


    /** @brief Attach components to an entity */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void attach(const Entity entity, Components &&...components) noexcept;

    /** @brief Attach components to an entity, if the component already exists then update it */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void tryAttach(const Entity entity, Components &&...components) noexcept;

    /** @brief Try to update components of an entity
     *  @note If a component doesn't exists, it is created */
    template<typename ...Functors>
    void tryAttach(const Entity entity, Functors &&...functors) noexcept;

    /** @brief Attach components to a range of entities */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void attachRange(const EntityRange range, Components &&...components) noexcept;


    /** @brief Dettach components from an entity */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void dettach(const Entity entity) noexcept;

    /** @brief Dettach components from an entity */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void tryDettach(const Entity entity) noexcept;

    /** @brief Dettach components from a range of entities */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void dettachRange(const EntityRange range) noexcept;


    /** @brief Removes an entity */
    void remove(const Entity entity) noexcept;

    /** @brief Removes a range of entities */
    void removeRange(const EntityRange range) noexcept;

    /** @brief Removes an entity, knowing attached components at compile time
     *  @note Any component attached that is not referenced inside 'Components' will not get destroyed */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void removeUnsafe(const Entity entity) noexcept;

    /** @brief Removes an entity, without destroying its components */
    inline void removeUnsafe(const Entity entity) noexcept { Internal::ASystem::remove(entity); }

    /** @brief Removes a range of entities, knowing attached components at compile time
     *  @note Any component attached that is not referenced inside 'Components' will not get destroyed */
    template<typename ...Components>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Components...>
    void removeUnsafeRange(const EntityRange range) noexcept;

    /** @brief Removes a range of entities, without destroying its components */
    inline void removeUnsafeRange(const EntityRange range) noexcept { Internal::ASystem::removeRange(range); }


    /** @brief Get a component table by component type */
    template<typename Component>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Component>
    [[nodiscard]] inline auto &getTable(void) noexcept
        { return std::get<Core::TupleElementIndex<std::remove_cvref_t<Component>, ComponentsTuple>>(_tables); }
    template<typename Component>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Component>
    [[nodiscard]] inline const auto &getTable(void) const noexcept
        { return std::get<Core::TupleElementIndex<std::remove_cvref_t<Component>, ComponentsTuple>>(_tables); }


    /** @brief Get a component using its type and an entity */
    template<typename Component>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Component>
    [[nodiscard]] inline Component &get(const Entity entity) noexcept
        { return getTable<Component>().get(entity); }
    template<typename Component>
        requires kF::ECS::SystemComponentRequirements<kF::ECS::Internal::ForwardComponentsTuple<ComponentTypes...>, Component>
    [[nodiscard]] inline const Component &get(const Entity entity) const noexcept
        { return getTable<Component>().get(entity); }


    /** @brief Calls a functor for each component table */
    template<typename Functor>
    inline void forEachTable(Functor &&delegate) noexcept
        { (delegate(get<ComponentTypes>()), ...); }

private:
    using Internal::ASystem::queryPipelineIndex;
    using Internal::ASystem::remove;
    using Internal::ASystem::removeRange;

    // Cacheline 1 -> ?
    [[no_unique_address]] ComponentTablesTuple _tables {};
};

#include "System.ipp"

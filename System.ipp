/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: System
 */

#include <Kube/Core/FunctionDecomposer.hpp>

#include "System.hpp"

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline kF::ECS::Entity kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::add(Components &&...components) noexcept
{
    const auto entity = add();

    attach(entity, std::forward<Components>(components)...);
    return entity;
}


template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline kF::ECS::EntityRange kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::addRange(const Entity count, Components &&...components) noexcept
{
    const auto range = addRange(count);

    attachRange(range, std::forward<Components>(components)...);
    return range;
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::attach(const Entity entity, Components &&...components) noexcept
{
    ((getTable<Components>().add(entity, std::forward<Components>(components))), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::attachUpdate(const Entity entity, Components &&...components) noexcept
{
    ((getTable<Components>().addUpdate(entity, std::forward<Components>(components))), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Functors>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::attachUpdate(const Entity entity, Functors &&...functors) noexcept
{
    const auto apply = [this]<typename Functor>(Functor &&functor) {
        using Decomposer = Core::Utils::FunctionDecomposerHelper<Functor>;
        using Component = std::remove_cvref_t<std::tuple_element_t<0, Decomposer::ArgsTuple>>;
        getTable<Component>().addUpdate(std::forward<Functor>(functor));
    };

    ((apply(std::forward<Functors>(functors))), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::attachRange(const EntityRange range, Components &&...components) noexcept
{
    ((getTable<Components>().addRange(range, std::forward<Components>(components))), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::dettach(const Entity entity) noexcept
{
    ((getTable<Components>().remove(entity)), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::tryDettach(const Entity entity) noexcept
{
    ((getTable<Components>().tryRemove(entity)), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::dettachRange(const EntityRange range) noexcept
{
    ((getTable<Components>().removeRange(range)), ...);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::remove(const Entity entity) noexcept
{
    const auto wrapper = [this]<Entity ...Indexes>(const Entity entity, const std::integer_sequence<Entity, Indexes...>) {
        ((std::get<Indexes>(_tables).tryRemove(entity)), ...);
    };

    wrapper(entity, std::make_integer_sequence<Entity, ComponentCount> {});
    Internal::ASystem::remove(entity);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::removeRange(const EntityRange range) noexcept
{
    const auto wrapper = [this]<Entity ...Indexes>(const EntityRange range, const std::integer_sequence<Entity, Indexes...>) {
        ((std::get<Indexes>(_tables).removeRange(range)), ...);
    };

    wrapper(range, std::make_integer_sequence<Entity, ComponentCount> {});
    Internal::ASystem::removeRange(range);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::removeUnsafe(const Entity entity) noexcept
{
    ((getTable<Components>.remove(entity)), ...);
    Internal::ASystem::remove(entity);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename ...Components>
    requires SystemComponentRequirements<std::tuple<ComponentTypes...>, Components...>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::removeUnsafeRange(const EntityRange range) noexcept
{
    ((getTable<Components>.removeRange(range)), ...);
    Internal::ASystem::removeRange(range);
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename Callback>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::interact(Callback &&callback) const noexcept
{
    using Decomposer = Core::Utils::FunctionDecomposerHelper<Callback>;
    using DestinationSystem = std::tuple_element_t<0, Decomposer::ArgsTuple>;
    using FlatSystem = std::remove_reference_t<DestinationSystem>;

    interact<FlatSystem::ExecutorPipeline>(std::forward<Callback>(callback));
}

template<kF::Core::FixedString Literal, kF::ECS::Pipeline TargetPipeline, kF::Core::Utils::StaticAllocator Allocator, typename ...ComponentTypes>
template<typename DestinationPipeline, typename Callback>
inline void kF::ECS::System<Literal, TargetPipeline, Allocator, ComponentTypes...>::interact(Callback &&callback) const noexcept
{
    // If 'this' pipeline is same as destination pipeline
    if constexpr (std::is_same_v<ExecutorPipeline, DestinationPipeline>) {
        using Decomposer = Core::Utils::FunctionDecomposerHelper<Callback>;
        // Callback without argument
        if constexpr (Decomposer::IndexSequence.size() == 0)
            callback();
        // Callback with argument
        else {
            using DestinationSystem = std::tuple_element_t<0, Decomposer::ArgsTuple>;
            using FlatSystem = std::remove_reference_t<DestinationSystem>;

            static_assert(Decomposer::IndexSequence.size() == 1 && std::is_base_of_v<FlatSystem, Internal::ASystem>
                && std::is_reference_v<DestinationSystem>,
                "ECS::System::interact: Event callback must only have one argument that must be a reference to any system");

            static_assert(std::is_same_v<DestinationPipeline, FlatSystem::ExecutorPipeline>,
                "ECS::System::interact: Mismatching destination pipeline and destination system's pipeline");

            // Get system at runtime and call now
            callback(parent()->getSystem<FlatSystem>(executorPipelineIndex()));
        }
    // If 'this' pipeline is not same as destination pipeline
    } else {
        // Send event to target pipeline
        parent()->sendEvent<TargetPipeline>(std::forward<Callback>(callback));
    }
}
/**
 * @ Author: Matthieu Moinvaziri
 * @ Description: Unit tests of System
 */

#include <gtest/gtest.h>

#include <Kube/ECS/ComponentTable.hpp>
#include <Kube/ECS/StableComponentTable.hpp>

using namespace kF;

using TestComponent = std::unique_ptr<int>;

template<typename TableType>
void TestTableBasics(void) noexcept
{
    TableType table;

    ASSERT_EQ(table.count(), 0u);
}

template<typename TableType>
void TestTableAddRemove(void) noexcept
{
    static constexpr int TestValue = 42;
    static constexpr ECS::Entity TestEntity = 1u;

    TableType table;

    // Add
    ASSERT_EQ(table.count(), 0u);
    auto &component = table.add(TestEntity, std::make_unique<int>(TestValue));
    ASSERT_EQ(table.count(), 1u);

    // Test insertion
    ASSERT_TRUE(component);
    ASSERT_EQ(*component, TestValue);
    ASSERT_TRUE(table.exists(TestEntity));
    ASSERT_EQ(&table.get(TestEntity), &component);

    // Remove
    table.remove(TestEntity);
    ASSERT_EQ(table.count(), 0u);

    // Test remove
    ASSERT_FALSE(table.exists(TestEntity));
}

template<typename TableType>
void TestTableAddRemoveRange(void) noexcept
{
    static constexpr ECS::EntityRange TestEntityRange { 0u, 100u };

    TableType table;

    // Add range
    ASSERT_EQ(table.count(), 0u);
    table.addRange(TestEntityRange);
    ASSERT_EQ(table.count(), TestEntityRange.size());

    // Test insertion
    for (auto it = TestEntityRange.begin; it != TestEntityRange.end; ++it) {
        ASSERT_TRUE(table.exists(it));
    }

    // Remove range
    table.removeRange(TestEntityRange);
    ASSERT_EQ(table.count(), 0u);

    // Test remove
    for (auto it = TestEntityRange.begin; it != TestEntityRange.end; ++it) {
        ASSERT_FALSE(table.exists(it));
    }
}

template<typename TableType>
void TestTableTryAddRemove(void) noexcept
{
    static constexpr int TestValue = 42;
    static constexpr int TestValue2 = 24;
    static constexpr int TestValue3 = 123;
    static constexpr ECS::Entity TestEntity = 1u;

    TableType table;

    // TryRemove (not existing)
    ASSERT_FALSE(table.tryRemove(TestEntity));

    // TryAdd (not existing)
    ASSERT_EQ(table.count(), 0u);
    auto &component = table.tryAdd(TestEntity, std::make_unique<int>(TestValue));
    ASSERT_EQ(table.count(), 1u);

    // Test insertion
    ASSERT_TRUE(component);
    ASSERT_EQ(*component, TestValue);
    ASSERT_TRUE(table.exists(TestEntity));
    ASSERT_EQ(&table.get(TestEntity), &component);

    // TryAdd (existing, replace)
    ASSERT_EQ(table.count(), 1u);
    auto &component2 = table.tryAdd(TestEntity, std::make_unique<int>(TestValue2));
    ASSERT_EQ(table.count(), 1u);

    // Test update
    ASSERT_TRUE(component2);
    ASSERT_EQ(&component, &component2);
    ASSERT_EQ(*component2, TestValue2);
    ASSERT_TRUE(table.exists(TestEntity));
    ASSERT_EQ(&table.get(TestEntity), &component2);

    // TryAdd (existing, modify)
    ASSERT_EQ(table.count(), 1u);
    auto &component3 = table.tryAdd(TestEntity, [](auto &component) { component = std::make_unique<int>(TestValue3); });
    ASSERT_EQ(table.count(), 1u);

    // Test update
    ASSERT_TRUE(component3);
    ASSERT_EQ(&component, &component3);
    ASSERT_EQ(*component3, TestValue3);
    ASSERT_TRUE(table.exists(TestEntity));
    ASSERT_EQ(&table.get(TestEntity), &component3);

    // TryRemove (existing)
    ASSERT_TRUE(table.tryRemove(TestEntity));

    // Test remove
    ASSERT_FALSE(table.exists(TestEntity));

    // TryRemove (not existing)
    ASSERT_FALSE(table.tryRemove(TestEntity));
}

template<typename TableType>
void TestTableExtract(void) noexcept
{
    static constexpr int TestValue = 42;
    static constexpr ECS::Entity TestEntity = 1u;

    TableType table;

    // Add
    ASSERT_EQ(table.count(), 0u);
    auto &component = table.add(TestEntity, std::make_unique<int>(TestValue));
    ASSERT_EQ(table.count(), 1u);

    // Test insertion
    ASSERT_TRUE(component);
    ASSERT_EQ(*component, TestValue);
    ASSERT_TRUE(table.exists(TestEntity));
    ASSERT_EQ(&table.get(TestEntity), &component);

    // Extract
    auto value = table.extract(TestEntity);
    ASSERT_EQ(table.count(), 0u);
    ASSERT_EQ(*value, TestValue);

    // Test remove
    ASSERT_FALSE(table.exists(TestEntity));
}

template<typename TableType>
void TestTableSort(void) noexcept
{
    static constexpr ECS::EntityIndex EntityCount = 100u;

    TableType table;
    ECS::Entity entity {};
    auto value = static_cast<int>(EntityCount);

    // Add some values
    for (auto i = 0u; i < EntityCount; ++i)
        table.add(++entity, std::make_unique<int>(--value));
    ASSERT_EQ(table.count(), EntityCount);

    // Sort
    table.sort([&table](const ECS::Entity lhs, const ECS::Entity rhs) {
        return *table.get(lhs) < *table.get(rhs);
    });

    // Test sort
    int lastValue {};
    for (auto index = 0; auto &component : table) {
        if (index++) {
            ASSERT_GT(*component, lastValue);
        }
        lastValue = *component;
    }
}

template<typename TableType>
void TestTableTraverse(void) noexcept
{
    static constexpr ECS::EntityIndex EntityCount = 100u;

    TableType table;
    ECS::Entity entity {};
    int value {};

    for (auto i = 0u; i < EntityCount; ++i)
        table.add(++entity, std::make_unique<int>(++value));
    ASSERT_EQ(table.count(), EntityCount);

    // Entity & Component
    bool error {};
    table.traverse([&error, e = 0u, i = 0](const ECS::Entity entity, const auto &component) mutable {
        error = ++e != entity || ++i != *component;
    });
    ASSERT_FALSE(error);

    // Component only
    table.traverse([&error, i = 0](const auto &component) mutable {
        error = ++i != *component;
    });
    ASSERT_FALSE(error);

    // Entity only
    table.traverse([&error, e = 0u](const auto entity) mutable {
        error = ++e != entity;
    });
    ASSERT_FALSE(error);


    // Entity & Component (return)
    ECS::EntityIndex count {};
    table.traverse([&error, &count, e = 0u, i = 0](const ECS::Entity entity, const auto &component) mutable {
        error = ++e != entity || ++i != *component;
        return ++count != (EntityCount / 2);
    });
    ASSERT_FALSE(error);
    ASSERT_EQ(count, EntityCount / 2);

    // Component (return)
    count = {};
    table.traverse([&error, &count, i = 0](const auto &component) mutable {
        error = ++i != *component;
        return ++count != (EntityCount / 2);
    });
    ASSERT_FALSE(error);
    ASSERT_EQ(count, EntityCount / 2);

    // Entity (return)
    count = {};
    table.traverse([&error, &count, e = 0u](const auto entity) mutable {
        error = ++e != entity;
        return ++count != (EntityCount / 2);
    });
    ASSERT_FALSE(error);
    ASSERT_EQ(count, EntityCount / 2);
}

template<typename TableType>
void TestTableClear(void) noexcept
{
    static constexpr ECS::EntityRange TestEntityRange { 0u, 100u };

    TableType table;

    // Add range
    ASSERT_EQ(table.count(), 0u);
    table.addRange(TestEntityRange);
    ASSERT_EQ(table.count(), TestEntityRange.size());

    // Clear table
    table.clear();
    ASSERT_EQ(table.count(), 0u);
}

template<typename TableType>
void TestTableRelease(void) noexcept
{
    static constexpr ECS::EntityRange TestEntityRange { 0u, 100u };

    TableType table;

    // Add range
    ASSERT_EQ(table.count(), 0u);
    table.addRange(TestEntityRange);
    ASSERT_EQ(table.count(), TestEntityRange.size());

    // Release table
    table.release();
    ASSERT_EQ(table.count(), 0u);
}

#define TEST_COMPONENT_TABLE(TableName, TableType) \
TEST(TableName, Basics) { TestTableBasics<TableType>(); } \
TEST(TableName, AddRemove) { TestTableAddRemove<TableType>(); } \
TEST(TableName, AddRemoveRange) { TestTableAddRemoveRange<TableType>(); } \
TEST(TableName, TryAddRemove) { TestTableTryAddRemove<TableType>(); } \
TEST(TableName, Extract) { TestTableExtract<TableType>(); } \
TEST(TableName, Sort) { TestTableSort<TableType>(); } \
TEST(TableName, Traverse) { TestTableTraverse<TableType>(); } \
TEST(TableName, Clear) { TestTableClear<TableType>(); } \
TEST(TableName, Release) { TestTableRelease<TableType>(); }

using ComponentTableType = ECS::ComponentTable<TestComponent, 4096 / sizeof(ECS::Entity)>;
using StableComponentTableType = ECS::StableComponentTable<TestComponent, 4096 / sizeof(ECS::Entity), 4096 / sizeof(TestComponent)>;

TEST_COMPONENT_TABLE(ComponentTable, ComponentTableType)
TEST_COMPONENT_TABLE(StableComponentTable, StableComponentTableType)

TEST(StableComponentTable, PackSparseHoles)
{
    static constexpr ECS::EntityRange TestEntityRange { 0u, 100u };

    StableComponentTableType table;

    // Add range
    ASSERT_EQ(table.count(), 0u);
    table.addRange(TestEntityRange);
    ASSERT_EQ(table.count(), TestEntityRange.size());

    // Remove some components
    table.remove(0u);
    table.remove(TestEntityRange.end / 8);
    table.remove(TestEntityRange.end / 4);
    table.remove(TestEntityRange.end / 2);
    table.remove(TestEntityRange.end - 1);

    // Test remove
    const ECS::Entity removeCount = 5;
    const ECS::Entity count = TestEntityRange.size() - removeCount;
    ASSERT_EQ(table.count(), count);
    ASSERT_EQ(std::count(table.entities().begin(), table.entities().end(), ECS::NullEntity), removeCount);

    // Pack
    table.pack();

    // Test pack
    ASSERT_EQ(table.count(), count);
    ASSERT_EQ(std::count(table.entities().begin(), table.entities().end(), ECS::NullEntity), 0);
}

TEST(StableComponentTable, PackBigHole)
{
    static constexpr ECS::EntityRange TestEntityRange { 0u, 100u };

    StableComponentTableType table;

    // Add range
    ASSERT_EQ(table.count(), 0u);
    table.addRange(TestEntityRange);
    ASSERT_EQ(table.count(), TestEntityRange.size());

    // Remove some components
    const ECS::Entity removeCount = (TestEntityRange.end - TestEntityRange.begin) / 4;
    table.removeRange(ECS::EntityRange { TestEntityRange.begin, TestEntityRange.begin + removeCount });

    // Test remove
    ASSERT_EQ(table.count(), TestEntityRange.size() - removeCount);
    ASSERT_EQ(std::count(table.entities().begin(), table.entities().end(), ECS::NullEntity), removeCount);

    // Pack
    table.pack();

    // Test pack
    ASSERT_EQ(table.count(), TestEntityRange.size() - removeCount);
    ASSERT_EQ(std::count(table.entities().begin(), table.entities().end(), ECS::NullEntity), 0);

    // Remove some components
    table.removeRange(ECS::EntityRange { TestEntityRange.begin + removeCount, TestEntityRange.begin + removeCount * 2 });

    // Test remove
    ASSERT_EQ(table.count(), TestEntityRange.size() - removeCount * 2);
    ASSERT_EQ(std::count(table.entities().begin(), table.entities().end(), ECS::NullEntity), removeCount);

    // Pack
    table.pack();

    // Test pack
    ASSERT_EQ(table.count(), TestEntityRange.size() - removeCount * 2);
    ASSERT_EQ(std::count(table.entities().begin(), table.entities().end(), ECS::NullEntity), 0);
}
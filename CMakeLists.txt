kube_add_library(ECS
    SOURCES
        Base.hpp
        ComponentTable.hpp
        ComponentTable.ipp
        Executor.cpp
        Executor.hpp
        Executor.ipp
        Pipeline.hpp
        StableComponentTable.hpp
        StableComponentTable.ipp
        System.cpp
        System.hpp
        System.ipp

    LIBRARIES
        Flow
)
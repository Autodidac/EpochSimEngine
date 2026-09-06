# Legacy consumer target spelling. Both packages link the same imported archive
# and expose identical C++ types; loading either package first is supported.
if(TARGET EpochSimEngine::EpochSimEngine)
    if(TARGET SandHybrid::SandHybrid)
        # A previously loaded older package must not silently retain a second
        # archive with the same C++ symbols. Normalize both aliases so matching
        # imported packages and in-tree add_subdirectory consumers still work.
        set(_epochsimengine_canonical_target EpochSimEngine::EpochSimEngine)
        get_target_property(_epochsimengine_alias "${_epochsimengine_canonical_target}" ALIASED_TARGET)
        while(_epochsimengine_alias)
            set(_epochsimengine_canonical_target "${_epochsimengine_alias}")
            get_target_property(_epochsimengine_alias "${_epochsimengine_canonical_target}" ALIASED_TARGET)
        endwhile()
        set(_epochsimengine_legacy_target SandHybrid::SandHybrid)
        get_target_property(_epochsimengine_alias "${_epochsimengine_legacy_target}" ALIASED_TARGET)
        while(_epochsimengine_alias)
            set(_epochsimengine_legacy_target "${_epochsimengine_alias}")
            get_target_property(_epochsimengine_alias "${_epochsimengine_legacy_target}" ALIASED_TARGET)
        endwhile()
        if(NOT _epochsimengine_legacy_target STREQUAL _epochsimengine_canonical_target)
            message(FATAL_ERROR
                "EpochSimEngine compatibility target conflict: SandHybrid::SandHybrid already belongs to a different library.")
        endif()
        unset(_epochsimengine_canonical_target)
        unset(_epochsimengine_legacy_target)
        unset(_epochsimengine_alias)
    else()
        add_library(SandHybrid::SandHybrid ALIAS EpochSimEngine::EpochSimEngine)
    endif()
endif()

/**
 * topology/topology_contract.h
 * 
 * UCQCF Phase-1 Topology Contract
 * 
 * PURPOSE:
 *   Topology is the immutable hardware geometry that domains must satisfy.
 *   This layer maps boot facts to security-relevant physical relationships.
 * 
 * GUARANTEES:
 *   - Cache sharing is explicit (no implicit assumptions)
 *   - NUMA distances are measured, not assumed
 *   - Isolation capabilities are facts, not policies
 *   - Once sealed, topology cannot change
 * 
 * SECURITY PROPERTY:
 *   If topology validation passes, all hardware facts needed for
 *   domain validation are available and immutable.
 */

#ifndef UCQCF_TOPOLOGY_CONTRACT_H
#define UCQCF_TOPOLOGY_CONTRACT_H

#include <stdint.h>
#include <stdbool.h>
#include "../boot/boot_contract.h"

/* ========================================================================
 * CORE TYPES
 * ======================================================================== */

typedef uint32_t core_id_t;
typedef uint32_t cache_domain_t;
typedef uint32_t numa_node_t;

#define CORE_ID_INVALID      0xFFFFFFFF
#define CACHE_DOMAIN_INVALID 0xFFFFFFFF
#define NUMA_NODE_INVALID    0xFFFFFFFF

#define MAX_CORES            256
#define MAX_CACHE_LEVELS     4
#define MAX_NUMA_NODES       8

/* ========================================================================
 * CACHE TOPOLOGY
 * ======================================================================== */

/**
 * Cache level properties
 */
typedef enum {
    CACHE_TYPE_NONE = 0,
    CACHE_TYPE_DATA,
    CACHE_TYPE_INSTRUCTION,
    CACHE_TYPE_UNIFIED
} cache_type_t;

typedef struct {
    cache_type_t type;
    uint32_t     size_bytes;
    uint32_t     line_size;
    uint32_t     associativity;
    bool         shared;          /* Shared with other cores? */
    uint32_t     sharing_count;   /* How many cores share this cache */
    core_id_t    shared_with[MAX_CORES];  /* Which cores share it */
} cache_level_t;

/**
 * Cache hierarchy for a single core
 */
typedef struct {
    cache_level_t levels[MAX_CACHE_LEVELS];
    uint32_t      level_count;
} cache_hierarchy_t;

/* ========================================================================
 * CORE GEOMETRY (Security-Critical)
 * ======================================================================== */

/**
 * Physical geometry of a single core
 * 
 * This is the security-relevant hardware structure.
 * Every field is a measured fact, not a policy decision.
 */
typedef struct {
    /* Core identity */
    core_id_t        physical_core;
    bool             online;
    bool             isolated;        /* Can be isolated from others */
    
    /* Socket/package information */
    uint32_t         socket_id;
    uint32_t         package_id;
    
    /* Cache domains (security-critical) */
    cache_domain_t   l1_domain;       /* L1 cache sharing domain */
    cache_domain_t   l2_domain;       /* L2 cache sharing domain */
    cache_domain_t   l3_domain;       /* L3 cache sharing domain */
    cache_hierarchy_t cache_hierarchy;
    
    /* NUMA information */
    numa_node_t      numa_node;
    uint32_t         numa_distance[MAX_NUMA_NODES];  /* Distance to each node */
    
    /* SMT/Hyperthreading */
    bool             has_smt;
    core_id_t        smt_sibling;     /* If SMT, which core shares execution */
    
    /* Frequency/P-states (for determinism) */
    uint32_t         base_freq_mhz;
    uint32_t         max_freq_mhz;
    bool             freq_scaling_disabled;  /* Required for determinism */
    
    /* Capabilities (negative capabilities explicit) */
    bool             supports_constant_time;
    bool             supports_cache_partitioning;
    bool             supports_memory_encryption;
    
    /* Validation state */
    bool             probed;
    bool             validated;
    
} core_geometry_t;

/* ========================================================================
 * CACHE ISOLATION ANALYSIS
 * ======================================================================== */

/**
 * Cache isolation relationship between two cores
 */
typedef enum {
    CACHE_ISOLATED_NONE = 0,    /* Cores share all caches */
    CACHE_ISOLATED_L1,          /* Private L1, shared L2/L3 */
    CACHE_ISOLATED_L2,          /* Private L1/L2, shared L3 */
    CACHE_ISOLATED_L3,          /* Private L1/L2/L3 */
    CACHE_ISOLATED_FULL         /* No shared cache at any level */
} cache_isolation_level_t;

/**
 * Precomputed cache isolation matrix
 * 
 * This is computed once during topology validation and sealed.
 * Scheduler and domain validator use this for O(1) isolation checks.
 */
typedef struct {
    cache_isolation_level_t isolation[MAX_CORES][MAX_CORES];
    bool                    computed;
    bool                    sealed;
} cache_isolation_matrix_t;

/* ========================================================================
 * NUMA TOPOLOGY
 * ======================================================================== */

/**
 * NUMA node information
 */
typedef struct {
    numa_node_t  id;
    uint32_t     memory_mb;
    uint32_t     core_count;
    core_id_t    cores[MAX_CORES];
    
    /* Distance matrix to other nodes (latency-based) */
    uint32_t     distance[MAX_NUMA_NODES];
    
    bool         validated;
} numa_node_t;

/* ========================================================================
 * TOPOLOGY STATE (Complete Hardware Model)
 * ======================================================================== */

/**
 * Complete topology state
 * 
 * This is the single source of truth for hardware geometry.
 * 
 * CRITICAL: Once sealed, this cannot change.
 * Domain validation depends on sealed topology.
 */
typedef struct {
    /* Core information */
    core_geometry_t cores[MAX_CORES];
    uint32_t        core_count;
    
    /* NUMA information */
    numa_node_t     numa_nodes[MAX_NUMA_NODES];
    uint32_t        numa_node_count;
    
    /* Cache isolation matrix (precomputed) */
    cache_isolation_matrix_t cache_isolation;
    
    /* Global capabilities */
    bool            supports_smt;
    bool            supports_numa;
    bool            supports_cache_partitioning;
    bool            symmetric;        /* All cores identical? */
    
    /* Validation state */
    bool            probed;
    bool            validated;
    bool            sealed;
    
    /* Boot facts reference (immutable) */
    const boot_facts_t *boot_facts;
    
} topology_state_t;

/* ========================================================================
 * TOPOLOGY VALIDATION
 * ======================================================================== */

/**
 * Topology validation error codes
 */
typedef enum {
    TOPOLOGY_ERROR_NONE = 0,
    
    /* Boot consistency errors */
    TOPOLOGY_ERROR_BOOT_FACTS_NULL,
    TOPOLOGY_ERROR_CORE_COUNT_MISMATCH,
    TOPOLOGY_ERROR_NUMA_COUNT_MISMATCH,
    
    /* Hardware errors */
    TOPOLOGY_ERROR_CORE_NOT_PROBED,
    TOPOLOGY_ERROR_CACHE_HIERARCHY_INCOMPLETE,
    TOPOLOGY_ERROR_NUMA_DISTANCE_INVALID,
    TOPOLOGY_ERROR_SMT_SIBLING_INVALID,
    
    /* Consistency errors */
    TOPOLOGY_ERROR_CACHE_DOMAIN_INCONSISTENT,
    TOPOLOGY_ERROR_ASYMMETRIC_TOPOLOGY,
    
    /* Security requirement errors */
    TOPOLOGY_ERROR_NO_ISOLATED_CORES,
    TOPOLOGY_ERROR_FREQ_SCALING_ENABLED,
    TOPOLOGY_ERROR_CONSTANT_TIME_UNSUPPORTED,
    
    /* Warnings */
    TOPOLOGY_WARN_SMT_ENABLED,
    TOPOLOGY_WARN_NUMA_ASYMMETRIC,
    TOPOLOGY_WARN_FREQ_VARIATION,
    
} topology_error_t;

/**
 * Topology validation result
 */
typedef enum {
    TOPOLOGY_VALIDATION_ACCEPT = 0,
    TOPOLOGY_VALIDATION_WARN,
    TOPOLOGY_VALIDATION_HARD_FAIL
} topology_validation_result_t;

/**
 * Topology validation context
 */
typedef struct {
    topology_error_t              errors[64];
    uint32_t                      error_count;
    topology_validation_result_t  worst_result;
} topology_validation_context_t;

/* ========================================================================
 * TOPOLOGY API
 * ======================================================================== */

/**
 * Initialize topology state
 * 
 * REQUIRES: boot_facts is fully initialized and sealed
 * ENSURES:  topology is ready for probing
 */
void topology_init(
    topology_state_t *topology,
    const boot_facts_t *boot_facts
);

/**
 * Probe single core geometry
 * 
 * REQUIRES: topology_init called
 * SIDE EFFECT: Populates cores[core_id] with hardware facts
 */
bool topology_probe_core(
    topology_state_t *topology,
    core_id_t core_id
);

/**
 * Probe all cores
 * 
 * REQUIRES: topology_init called
 * ENSURES:  All cores are probed or error
 */
bool topology_probe_all_cores(topology_state_t *topology);

/**
 * Build cache isolation matrix
 * 
 * REQUIRES: All cores probed
 * ENSURES:  O(1) cache isolation queries
 * 
 * This precomputes all pairwise cache isolation relationships.
 * Critical for scheduler and domain validation performance.
 */
bool topology_build_cache_isolation_matrix(topology_state_t *topology);

/**
 * Validate topology
 * 
 * CRITICAL: This is where hardware facts are verified against
 *           security requirements.
 * 
 * Checks:
 *   - All cores probed successfully
 *   - Cache hierarchies are consistent
 *   - NUMA distances are sane
 *   - SMT siblings are valid
 *   - Frequency scaling is disabled (determinism requirement)
 *   - At least some cores are isolatable
 * 
 * RETURNS: ACCEPT, WARN, or HARD_FAIL
 */
topology_validation_result_t topology_validate(
    topology_state_t *topology,
    topology_validation_context_t *ctx
);

/**
 * Seal topology (make immutable)
 * 
 * REQUIRES: topology_validate returned ACCEPT
 * ENSURES:  No further modifications possible
 * 
 * SECURITY: This is a one-way transition.
 *           Once sealed, topology is immutable for remainder of boot.
 */
bool topology_seal(topology_state_t *topology);

/* ========================================================================
 * TOPOLOGY QUERY API (Safe After Validation)
 * ======================================================================== */

/**
 * Get core geometry
 * 
 * REQUIRES: topology validated
 * RETURNS:  Pointer to core geometry or NULL if invalid
 */
const core_geometry_t* topology_get_core_geometry(
    const topology_state_t *topology,
    core_id_t core_id
);

/**
 * Check cache isolation between two cores
 * 
 * REQUIRES: topology sealed
 * RETURNS:  Cache isolation level (O(1) lookup)
 */
cache_isolation_level_t topology_get_cache_isolation(
    const topology_state_t *topology,
    core_id_t core_a,
    core_id_t core_b
);

/**
 * Check if two cores can be isolated
 * 
 * REQUIRES: topology sealed
 * RETURNS:  true if cores can be cache-isolated at requested level
 */
bool topology_can_isolate_cores(
    const topology_state_t *topology,
    core_id_t core_a,
    core_id_t core_b,
    cache_isolation_level_t required_level
);

/**
 * Get NUMA node for core
 * 
 * REQUIRES: topology validated
 * RETURNS:  NUMA node ID or NUMA_NODE_INVALID
 */
numa_node_t topology_get_numa_node(
    const topology_state_t *topology,
    core_id_t core_id
);

/**
 * Check if cores are on same NUMA node
 * 
 * REQUIRES: topology validated
 * RETURNS:  true if cores share NUMA node
 */
bool topology_same_numa_node(
    const topology_state_t *topology,
    core_id_t core_a,
    core_id_t core_b
);

/**
 * Get NUMA distance between cores
 * 
 * REQUIRES: topology validated
 * RETURNS:  Relative distance (lower = closer)
 */
uint32_t topology_get_numa_distance(
    const topology_state_t *topology,
    core_id_t core_a,
    core_id_t core_b
);

/**
 * Check if core has SMT sibling
 * 
 * REQUIRES: topology validated
 * RETURNS:  true if core shares execution resources
 */
bool topology_has_smt_sibling(
    const topology_state_t *topology,
    core_id_t core_id
);

/**
 * Get cores that share cache at level
 * 
 * REQUIRES: topology validated
 * RETURNS:  Count of cores that share cache
 * SIDE EFFECT: Populates out_cores array
 */
uint32_t topology_get_cache_sharing_cores(
    const topology_state_t *topology,
    core_id_t core_id,
    uint32_t cache_level,
    core_id_t *out_cores,
    uint32_t max_cores
);

/* ========================================================================
 * ERROR REPORTING
 * ======================================================================== */

/**
 * Convert error code to string
 */
const char* topology_error_string(topology_error_t error);

/**
 * Print validation context
 */
void topology_validation_context_print(
    const topology_validation_context_t *ctx
);

/**
 * Check if validation allows boot
 */
bool topology_validation_allows_boot(
    const topology_validation_context_t *ctx
);

/* ========================================================================
 * COMPILE-TIME GUARANTEES
 * ======================================================================== */

_Static_assert(MAX_CORES <= 256, 
    "MAX_CORES must fit in uint8_t for compact representation");

_Static_assert(MAX_NUMA_NODES <= 8,
    "MAX_NUMA_NODES must be reasonable for distance matrix");

_Static_assert(sizeof(cache_isolation_matrix_t) <= 256 * 256,
    "Cache isolation matrix must be cache-friendly");

#endif /* UCQCF_TOPOLOGY_CONTRACT_H */
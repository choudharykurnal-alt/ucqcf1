/**
 * domains/domain_contract.h
 * 
 * UCQCF Phase-1 Security Domain Contract
 * 
 * PURPOSE:
 *   This is where requirements meet reality.
 *   Domains are the choke point where misconfiguration MUST be rejected.
 * 
 * GUARANTEES:
 *   - No overlapping core sets
 *   - No circular dependencies
 *   - Isolation requirements are satisfiable by topology
 *   - No defaults (absence = error)
 *   - Validation is deterministic and complete
 * 
 * SECURITY PROPERTY:
 *   If domain validation passes, the system cannot violate trust boundaries
 *   through scheduling or memory access.
 */

#ifndef UCQCF_DOMAIN_CONTRACT_H
#define UCQCF_DOMAIN_CONTRACT_H

#include <stdint.h>
#include <stdbool.h>
#include "../boot/boot_contract.h"
#include "../topology/topology_contract.h"

/* ========================================================================
 * CORE TYPES
 * ======================================================================== */

/**
 * Domain identifier (requirement-defined, immutable)
 */
typedef uint32_t domain_id_t;

#define DOMAIN_ID_INVALID  0xFFFFFFFF
#define DOMAIN_ID_BOOT     0
#define MAX_DOMAINS        64
#define MAX_DOMAIN_CORES   256
#define MAX_DEPENDENCIES   32

/**
 * Security level (requirement-defined, no interpretation)
 * 
 * IMPORTANT: These are NOT "high/medium/low" in any generic sense.
 * They are ordinal values that encode requirement-specific trust relationships.
 */
typedef enum {
    SECURITY_LEVEL_UNDEFINED = 0,  /* ERROR: Must be explicitly set */
    SECURITY_LEVEL_0 = 1,           /* Requirement-defined */
    SECURITY_LEVEL_1 = 2,
    SECURITY_LEVEL_2 = 3,
    SECURITY_LEVEL_3 = 4,
    SECURITY_LEVEL_4 = 5,
    SECURITY_LEVEL_5 = 6,
    SECURITY_LEVEL_6 = 7,
    SECURITY_LEVEL_7 = 8,
    SECURITY_LEVEL_MAX = SECURITY_LEVEL_7
} security_level_t;

/**
 * Cache isolation requirement
 * 
 * Maps directly to topology capabilities.
 * If topology cannot satisfy, domain validation MUST fail.
 */
typedef enum {
    CACHE_ISOLATION_UNDEFINED = 0,   /* ERROR: Must be explicitly set */
    CACHE_ISOLATION_NONE,            /* Cores may share all caches */
    CACHE_ISOLATION_L1,              /* Private L1 required */
    CACHE_ISOLATION_L2,              /* Private L1 AND L2 required */
    CACHE_ISOLATION_L3,              /* Private L1 AND L2 AND L3 required */
    CACHE_ISOLATION_FULL             /* No shared cache at any level */
} cache_isolation_t;

/**
 * Memory domain type
 * 
 * Determines NUMA and sharing policy.
 */
typedef enum {
    MEMORY_DOMAIN_UNDEFINED = 0,     /* ERROR: Must be explicitly set */
    MEMORY_DOMAIN_ISOLATED,          /* No sharing with other domains */
    MEMORY_DOMAIN_SHARED_READ,       /* Read-only sharing allowed */
    MEMORY_DOMAIN_SHARED_WRITE       /* Read-write sharing (must be explicit) */
} memory_domain_type_t;

/**
 * Preemption policy
 * 
 * Security-critical: determines whether domain can be interrupted.
 */
typedef enum {
    PREEMPTION_UNDEFINED = 0,        /* ERROR: Must be explicitly set */
    PREEMPTION_NEVER,                /* Domain tasks run to completion */
    PREEMPTION_BY_HIGHER,            /* Only higher security levels can preempt */
    PREEMPTION_BY_SAME,              /* Same level can preempt */
    PREEMPTION_BY_ANY                /* Any domain can preempt */
} preemption_policy_t;

/* ========================================================================
 * CORE SET (No Overlaps Allowed)
 * ======================================================================== */

/**
 * Core set representation
 * 
 * INVARIANT: No two domains may have overlapping core sets.
 * INVARIANT: All cores in set must exist in boot_facts.
 */
typedef struct {
    uint64_t bitmap[4];  /* Supports up to 256 cores (4 * 64 bits) */
    uint32_t count;      /* Number of cores in set (cached) */
    bool     explicit;   /* true = explicitly set, false = ERROR */
} core_set_t;

/* Core set operations (validation helpers) */
bool core_set_is_empty(const core_set_t *set);
bool core_set_contains(const core_set_t *set, core_id_t core);
bool core_set_overlaps(const core_set_t *a, const core_set_t *b);
bool core_set_is_valid(const core_set_t *set, const boot_facts_t *boot);
void core_set_add(core_set_t *set, core_id_t core);
void core_set_clear(core_set_t *set);

/* ========================================================================
 * DEPENDENCY GRAPH (Must Be Acyclic)
 * ======================================================================== */

/**
 * Domain dependency
 * 
 * INVARIANT: Dependency graph must be acyclic.
 * INVARIANT: All referenced domains must exist.
 */
typedef struct {
    domain_id_t depends_on[MAX_DEPENDENCIES];
    uint32_t    count;
    bool        explicit;  /* true = explicitly set, false = use empty set */
} dependency_set_t;

/* Dependency operations */
bool dependency_set_is_empty(const dependency_set_t *deps);
bool dependency_set_contains(const dependency_set_t *deps, domain_id_t id);
void dependency_set_add(dependency_set_t *deps, domain_id_t id);
void dependency_set_clear(dependency_set_t *deps);

/* ========================================================================
 * SECURITY DOMAIN DEFINITION
 * ======================================================================== */

/**
 * Security domain
 * 
 * RULE: Every field must be explicitly set.
 * RULE: Absence of explicit value = validation failure.
 * RULE: No defaults, no inference, no fallbacks.
 */
typedef struct {
    /* Identity (immutable) */
    domain_id_t         id;
    char                name[64];
    bool                name_explicit;
    
    /* Security properties (requirement-defined) */
    security_level_t    security_level;
    preemption_policy_t preemption;
    
    /* Core assignment (topology-validated) */
    core_set_t          cores;
    cache_isolation_t   cache_isolation;
    
    /* Memory properties (enforced by memory layer) */
    memory_domain_type_t memory_type;
    bool                numa_local;  /* Require NUMA-local memory */
    bool                numa_local_explicit;
    
    /* Dependencies (graph-validated) */
    dependency_set_t    dependencies;
    
    /* Validation state (computed during validation) */
    bool                validated;
    bool                sealed;
    
} security_domain_t;

/* ========================================================================
 * DOMAIN GRAPH (All Domains + Relationships)
 * ======================================================================== */

/**
 * Domain graph
 * 
 * This is the complete security policy for Phase-1.
 * 
 * INVARIANT: All domains have unique IDs
 * INVARIANT: All dependencies are satisfied
 * INVARIANT: Dependency graph is acyclic
 * INVARIANT: No core assigned to multiple domains
 * INVARIANT: All cache isolation requirements are satisfiable
 */
typedef struct {
    security_domain_t   domains[MAX_DOMAINS];
    uint32_t            domain_count;
    bool                validated;
    bool                sealed;
    
    /* Boot and topology references (immutable) */
    const boot_facts_t     *boot_facts;
    const topology_state_t *topology;
    
} domain_graph_t;

/* ========================================================================
 * VALIDATION OUTCOMES (No Ambiguity)
 * ======================================================================== */

/**
 * Validation result
 * 
 * HARD_FAIL: System cannot boot (abort immediately)
 * WARN:      Logged, but allowed (for non-security properties)
 * ACCEPT:    Validation passed
 */
typedef enum {
    VALIDATION_ACCEPT = 0,
    VALIDATION_WARN,
    VALIDATION_HARD_FAIL
} validation_result_t;

/**
 * Validation error code
 * 
 * Explicit error taxonomy (no generic "invalid" errors).
 */
typedef enum {
    VALIDATION_ERROR_NONE = 0,
    
    /* Field completeness errors (HARD_FAIL) */
    VALIDATION_ERROR_FIELD_NOT_SET,
    VALIDATION_ERROR_NAME_EMPTY,
    VALIDATION_ERROR_CORES_EMPTY,
    VALIDATION_ERROR_SECURITY_LEVEL_UNDEFINED,
    VALIDATION_ERROR_CACHE_ISOLATION_UNDEFINED,
    VALIDATION_ERROR_MEMORY_TYPE_UNDEFINED,
    VALIDATION_ERROR_PREEMPTION_UNDEFINED,
    
    /* Topology constraint errors (HARD_FAIL) */
    VALIDATION_ERROR_CORE_NOT_EXIST,
    VALIDATION_ERROR_CORES_OVERLAP,
    VALIDATION_ERROR_CACHE_ISOLATION_UNSATISFIABLE,
    VALIDATION_ERROR_NUMA_CONSTRAINT_VIOLATED,
    
    /* Dependency errors (HARD_FAIL) */
    VALIDATION_ERROR_DEPENDENCY_NOT_EXIST,
    VALIDATION_ERROR_DEPENDENCY_CIRCULAR,
    VALIDATION_ERROR_DEPENDENCY_SELF,
    
    /* Domain graph errors (HARD_FAIL) */
    VALIDATION_ERROR_DUPLICATE_ID,
    VALIDATION_ERROR_TOO_MANY_DOMAINS,
    VALIDATION_ERROR_BOOT_FACTS_NULL,
    VALIDATION_ERROR_TOPOLOGY_NULL,
    
    /* Warnings (WARN) */
    VALIDATION_WARN_UNUSED_CORES,
    VALIDATION_WARN_ASYMMETRIC_TOPOLOGY,
    
} validation_error_t;

/**
 * Validation context
 * 
 * Accumulates all errors during validation.
 */
typedef struct {
    validation_error_t  errors[64];
    uint32_t            error_count;
    validation_result_t worst_result;  /* Highest severity seen */
} validation_context_t;

/* ========================================================================
 * VALIDATION API (Deterministic, Complete, Explicit)
 * ======================================================================== */

/**
 * Initialize domain graph
 * 
 * REQUIRES: boot_facts and topology are fully initialized
 * ENSURES:  graph is ready for domain addition
 */
void domain_graph_init(
    domain_graph_t *graph,
    const boot_facts_t *boot_facts,
    const topology_state_t *topology
);

/**
 * Add domain to graph (before validation)
 * 
 * REQUIRES: domain is fully populated (all fields explicit)
 * RETURNS:  true if added, false if graph is full
 */
bool domain_graph_add(domain_graph_t *graph, const security_domain_t *domain);

/**
 * Validate entire domain graph
 * 
 * This is the CRITICAL SECURITY FUNCTION.
 * 
 * Checks:
 *   1. All fields are explicitly set (no defaults)
 *   2. No overlapping core sets
 *   3. All cores exist in boot_facts
 *   4. Cache isolation requirements are satisfiable
 *   5. Dependency graph is acyclic
 *   6. All dependencies reference valid domains
 *   7. NUMA constraints are satisfiable
 * 
 * RETURNS: VALIDATION_ACCEPT, VALIDATION_WARN, or VALIDATION_HARD_FAIL
 * SIDE EFFECT: Populates validation_context with all errors
 */
validation_result_t domain_graph_validate(
    domain_graph_t *graph,
    validation_context_t *ctx
);

/**
 * Seal domain graph (make immutable)
 * 
 * REQUIRES: domain_graph_validate returned VALIDATION_ACCEPT
 * ENSURES:  No further modifications possible
 * 
 * SECURITY: This is a one-way transition. Once sealed, the domain
 *           configuration cannot be changed without reboot.
 */
bool domain_graph_seal(domain_graph_t *graph);

/* ========================================================================
 * QUERY FUNCTIONS (safe after validation)
 * ======================================================================== */

/**
 * Get a domain by ID
 */
const security_domain_t* domain_graph_get(
    const domain_graph_t *graph,
    domain_id_t id
);

/**
 * Check if a domain can access another (based on dependencies)
 */
bool domain_graph_can_access(
    const domain_graph_t *graph,
    domain_id_t from,
    domain_id_t to
);

/**
 * Check if cores assigned to two domains are isolated
 */
bool domain_graph_cores_isolated(
    const domain_graph_t *graph,
    domain_id_t a,
    domain_id_t b
);

/* ========================================================================
 * INDIVIDUAL DOMAIN VALIDATORS (Composable)
 * ======================================================================== */

/**
 * Validate domain field completeness
 * 
 * Ensures no field is left uninitialized.
 */
validation_result_t domain_validate_fields(
    const security_domain_t *domain,
    validation_context_t *ctx
);

/**
 * Validate domain against topology
 * 
 * Ensures all cores exist and isolation is achievable.
 */
validation_result_t domain_validate_topology(
    const security_domain_t *domain,
    const topology_state_t *topology,
    validation_context_t *ctx
);

/**
 * Validate domain against boot facts
 * 
 * Ensures all cores exist in hardware.
 */
validation_result_t domain_validate_boot(
    const security_domain_t *domain,
    const boot_facts_t *boot_facts,
    validation_context_t *ctx
);

/**
 * Validate dependencies
 * 
 * Ensures all referenced domains exist.
 * (Acyclic check happens at graph level)
 */
validation_result_t domain_validate_dependencies(
    const security_domain_t *domain,
    const domain_graph_t *graph,
    validation_context_t *ctx
);

/* ========================================================================
 * GRAPH VALIDATORS (Holistic)
 * ======================================================================== */

/**
 * Validate no overlapping cores
 * 
 * CRITICAL: Two domains sharing cores violates isolation.
 */
validation_result_t domain_graph_validate_no_overlap(
    const domain_graph_t *graph,
    validation_context_t *ctx
);

/**
 * Validate dependency graph is acyclic
 * 
 * CRITICAL: Circular dependencies create undefined security states.
 */
validation_result_t domain_graph_validate_acyclic(
    const domain_graph_t *graph,
    validation_context_t *ctx
);

/**
 * Validate cache isolation is achievable
 * 
 * CRITICAL: If topology cannot satisfy isolation, validation MUST fail.
 */
validation_result_t domain_graph_validate_cache_isolation(
    const domain_graph_t *graph,
    validation_context_t *ctx
);

/* ========================================================================
 * ERROR REPORTING (Explicit, No Ambiguity)
 * ======================================================================== */

/**
 * Convert error code to human-readable string
 */
const char* validation_error_string(validation_error_t error);

/**
 * Print validation context (for debugging/logging)
 */
void validation_context_print(const validation_context_t *ctx);

/**
 * Check if validation allows boot
 * 
 * RETURNS: true if no HARD_FAIL errors
 */
bool validation_context_allows_boot(const validation_context_t *ctx);

/* ========================================================================
 * COMPILE-TIME GUARANTEES
 * ======================================================================== */

/* Ensure enums have explicit invalid states */
_Static_assert(SECURITY_LEVEL_UNDEFINED == 0, 
    "SECURITY_LEVEL_UNDEFINED must be zero for safety");
_Static_assert(CACHE_ISOLATION_UNDEFINED == 0,
    "CACHE_ISOLATION_UNDEFINED must be zero for safety");
_Static_assert(MEMORY_DOMAIN_UNDEFINED == 0,
    "MEMORY_DOMAIN_UNDEFINED must be zero for safety");
_Static_assert(PREEMPTION_UNDEFINED == 0,
    "PREEMPTION_UNDEFINED must be zero for safety");

/* Ensure core_set_t can represent all possible cores */
_Static_assert(sizeof(core_set_t) * 8 >= MAX_DOMAIN_CORES,
    "core_set_t bitmap too small for MAX_DOMAIN_CORES");

#endif /* UCQCF_DOMAIN_CONTRACT_H */
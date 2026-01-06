/**
 * boot/boot_contract.h
 * 
 * UCQCF Phase-1 Boot Facts Contract
 * 
 * PURPOSE:
 *   Boot gives you immutable hardware facts.
 *   This is ground truth that everything else depends on.
 * 
 * GUARANTEES:
 *   - All detection is deterministic
 *   - Results are immutable after probing
 *   - Failures are detectable before policy loads
 *   - No memory allocation
 *   - No security decisions
 * 
 * SECURITY PROPERTY:
 *   If boot facts are sealed, hardware cannot change during runtime.
 */

#ifndef UCQCF_BOOT_CONTRACT_H
#define UCQCF_BOOT_CONTRACT_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * CORE TYPES
 * ======================================================================== */

#define MAX_CPU_COUNT        256
#define MAX_NUMA_NODES_BOOT  8
#define MAX_CACHE_LEVELS     4

/* ========================================================================
 * MICROARCHITECTURE DETECTION
 * ======================================================================== */

/**
 * CPU vendor (for microarchitecture-specific handling)
 */
typedef enum {
    CPU_VENDOR_UNKNOWN = 0,
    CPU_VENDOR_INTEL,
    CPU_VENDOR_AMD,
    CPU_VENDOR_ARM,
} cpu_vendor_t;

/**
 * CPU family/model (for errata and capability detection)
 */
typedef struct {
    cpu_vendor_t vendor;
    uint32_t     family;
    uint32_t     model;
    uint32_t     stepping;
    char         brand_string[64];
    bool         valid;
} cpu_info_t;

/* ========================================================================
 * CACHE TOPOLOGY (Boot-Level Detection)
 * ======================================================================== */

/**
 * Boot-level cache information
 * 
 * This is the initial detection. Detailed topology comes later.
 */
typedef struct {
    uint32_t level;           /* 1, 2, 3, etc. */
    uint32_t size_kb;
    uint32_t line_size;
    uint32_t ways;
    bool     shared;          /* Shared between cores? */
    bool     inclusive;       /* Inclusive of lower levels? */
    bool     valid;
} cache_info_t;

typedef struct {
    cache_info_t levels[MAX_CACHE_LEVELS];
    uint32_t     level_count;
} cache_topology_t;

/* ========================================================================
 * HARDWARE CAPABILITIES (Security-Critical)
 * ======================================================================== */

/**
 * Constant-time instruction support
 * 
 * SECURITY: Required for crypto operations to avoid timing attacks
 */
typedef struct {
    bool aes_ni;              /* AES hardware acceleration */
    bool rdrand;              /* Hardware RNG */
    bool rdseed;              /* Hardware seed source */
    bool constant_time_mul;   /* Constant-time multiply */
    bool constant_time_cmp;   /* Constant-time compare */
    bool valid;
} constant_time_support_t;

/**
 * Cache control capabilities
 * 
 * SECURITY: Required for cache isolation
 */
typedef struct {
    bool clflush;             /* Cache line flush */
    bool clflushopt;          /* Optimized cache flush */
    bool clwb;                /* Cache line writeback */
    bool cat;                 /* Cache allocation technology (Intel) */
    bool cdp;                 /* Code/data prioritization */
    bool valid;
} cache_control_t;

/**
 * Memory protection capabilities
 * 
 * SECURITY: Required for memory isolation
 */
typedef struct {
    bool nx;                  /* No-execute bit */
    bool smep;                /* Supervisor mode execution prevention */
    bool smap;                /* Supervisor mode access prevention */
    bool pku;                 /* Memory protection keys */
    bool tme;                 /* Total memory encryption */
    bool valid;
} memory_protection_t;

/**
 * Side-channel mitigation support
 * 
 * SECURITY: Required for speculative execution safety
 */
typedef struct {
    bool ibrs;                /* Indirect branch restricted speculation */
    bool stibp;               /* Single thread indirect branch predictors */
    bool ssbd;                /* Speculative store bypass disable */
    bool md_clear;            /* MDS mitigation */
    bool valid;
} side_channel_mitigation_t;

/* ========================================================================
 * BOOT FACTS (Immutable After Sealing)
 * ======================================================================== */

/**
 * Boot facts structure
 * 
 * This is the complete set of hardware facts discovered at boot.
 * 
 * CRITICAL RULE: Once sealed, this structure is immutable.
 * Topology and domains depend on this remaining constant.
 */
typedef struct {
    /* CPU topology (counts only) */
    uint32_t            cpu_count;
    uint32_t            numa_nodes;
    bool                smt_enabled;
    uint32_t            threads_per_core;  /* 1 if SMT disabled, 2+ if enabled */
    
    /* CPU identification */
    cpu_info_t          cpu_info;
    
    /* Cache topology */
    cache_topology_t    cache_topology;
    
    /* Hardware capabilities (security-critical) */
    constant_time_support_t   constant_time;
    cache_control_t           cache_control;
    memory_protection_t       memory_protection;
    side_channel_mitigation_t side_channel_mitigation;
    
    /* Aggregated capability flags (for quick checks) */
    bool                constant_time_supported;
    bool                cache_partitioning_supported;
    bool                memory_encryption_supported;
    bool                trng_available;
    bool                side_channel_mitigations_available;
    
    /* Platform information */
    uint64_t            total_memory_mb;
    bool                uefi_boot;
    bool                secure_boot_enabled;
    
    /* Boot integrity */
    bool                probed;
    bool                validated;
    bool                sealed;
    
} boot_facts_t;

/* ========================================================================
 * BOOT VALIDATION
 * ======================================================================== */

/**
 * Boot validation error codes
 */
typedef enum {
    BOOT_ERROR_NONE = 0,
    
    /* Detection failures */
    BOOT_ERROR_CPU_DETECTION_FAILED,
    BOOT_ERROR_CACHE_DETECTION_FAILED,
    BOOT_ERROR_NUMA_DETECTION_FAILED,
    
    /* Insufficient hardware */
    BOOT_ERROR_TOO_FEW_CORES,
    BOOT_ERROR_NO_CACHE,
    BOOT_ERROR_NO_NUMA,
    
    /* Missing security features */
    BOOT_ERROR_NO_CONSTANT_TIME_SUPPORT,
    BOOT_ERROR_NO_CACHE_CONTROL,
    BOOT_ERROR_NO_MEMORY_PROTECTION,
    BOOT_ERROR_NO_SIDE_CHANNEL_MITIGATION,
    BOOT_ERROR_NO_TRNG,
    
    /* Configuration issues */
    BOOT_ERROR_SMT_ENABLED_NOT_ALLOWED,
    BOOT_ERROR_FREQ_SCALING_ENABLED,
    BOOT_ERROR_SECURE_BOOT_DISABLED,
    
    /* Warnings */
    BOOT_WARN_ASYMMETRIC_CORES,
    BOOT_WARN_NUMA_DISABLED,
    BOOT_WARN_OLD_MICROCODE,
    
} boot_error_t;

/**
 * Boot validation result
 */
typedef enum {
    BOOT_VALIDATION_ACCEPT = 0,
    BOOT_VALIDATION_WARN,
    BOOT_VALIDATION_HARD_FAIL
} boot_validation_result_t;

/**
 * Boot validation context
 */
typedef struct {
    boot_error_t              errors[32];
    uint32_t                  error_count;
    boot_validation_result_t  worst_result;
} boot_validation_context_t;

/* ========================================================================
 * BOOT API
 * ======================================================================== */

/**
 * Initialize boot facts structure
 * 
 * REQUIRES: Nothing (first function called in Phase-1)
 * ENSURES:  boot_facts is zeroed and ready for probing
 */
void boot_init(boot_facts_t *facts);

/**
 * Probe hardware
 * 
 * REQUIRES: boot_init called
 * SIDE EFFECT: Populates boot_facts with hardware detection
 * 
 * This is the main detection routine. It:
 *   - Detects CPU vendor/family/model
 *   - Counts cores and NUMA nodes
 *   - Probes cache hierarchy
 *   - Detects security capabilities
 *   - Checks for required features
 * 
 * DOES NOT: Allocate memory, make policy decisions, start scheduler
 */
bool boot_probe(boot_facts_t *facts);

/**
 * Validate boot facts
 * 
 * REQUIRES: boot_probe completed
 * 
 * Checks:
 *   - All required features present
 *   - Hardware configuration is sane
 *   - Security requirements are met
 * 
 * RETURNS: ACCEPT, WARN, or HARD_FAIL
 */
boot_validation_result_t boot_validate(
    boot_facts_t *facts,
    boot_validation_context_t *ctx
);

/**
 * Seal boot facts (make immutable)
 * 
 * REQUIRES: boot_validate returned ACCEPT
 * ENSURES:  No further modifications possible
 * 
 * SECURITY: This is a one-way transition.
 */
bool boot_seal(boot_facts_t *facts);

/* ========================================================================
 * BOOT QUERY API (Safe After Validation)
 * ======================================================================== */

/**
 * Get CPU count
 */
static inline uint32_t boot_get_cpu_count(const boot_facts_t *facts) {
    return facts->cpu_count;
}

/**
 * Get NUMA node count
 */
static inline uint32_t boot_get_numa_node_count(const boot_facts_t *facts) {
    return facts->numa_nodes;
}

/**
 * Check if constant-time operations supported
 */
static inline bool boot_supports_constant_time(const boot_facts_t *facts) {
    return facts->constant_time_supported;
}

/**
 * Check if cache partitioning supported
 */
static inline bool boot_supports_cache_partitioning(const boot_facts_t *facts) {
    return facts->cache_partitioning_supported;
}

/**
 * Check if hardware TRNG available
 */
static inline bool boot_has_trng(const boot_facts_t *facts) {
    return facts->trng_available;
}

/**
 * Check if SMT enabled
 */
static inline bool boot_smt_enabled(const boot_facts_t *facts) {
    return facts->smt_enabled;
}

/* ========================================================================
 * ERROR REPORTING
 * ======================================================================== */

/**
 * Convert error code to string
 */
const char* boot_error_string(boot_error_t error);

/**
 * Print validation context
 */
void boot_validation_context_print(const boot_validation_context_t *ctx);

/**
 * Check if validation allows boot
 */
bool boot_validation_allows_boot(const boot_validation_context_t *ctx);

/* ========================================================================
 * ARCHITECTURE-SPECIFIC PROBING
 * ======================================================================== */

/**
 * Architecture-specific probe functions
 * 
 * These are implemented in arch/x86_64/, arch/armv8/, etc.
 */

/* Probe CPU vendor and model */
bool boot_probe_cpu_info(cpu_info_t *info);

/* Probe cache hierarchy */
bool boot_probe_cache_topology(cache_topology_t *cache);

/* Probe constant-time instruction support */
bool boot_probe_constant_time_support(constant_time_support_t *ct);

/* Probe cache control capabilities */
bool boot_probe_cache_control(cache_control_t *cache_ctrl);

/* Probe memory protection features */
bool boot_probe_memory_protection(memory_protection_t *mem_prot);

/* Probe side-channel mitigations */
bool boot_probe_side_channel_mitigation(side_channel_mitigation_t *scm);

/* Check if TRNG available */
bool boot_probe_trng_available(void);

/* Check if SMT enabled */
bool boot_probe_smt_enabled(void);

/* Get total memory size */
uint64_t boot_probe_total_memory_mb(void);

/* ========================================================================
 * COMPILE-TIME GUARANTEES
 * ======================================================================== */

_Static_assert(sizeof(boot_facts_t) < 4096,
    "boot_facts_t should fit in single page for cache efficiency");

#endif /* UCQCF_BOOT_CONTRACT_H */
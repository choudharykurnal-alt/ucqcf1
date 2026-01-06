/**
 * boot/early_init.c
 * 
 * Boot orchestration layer (architecture-neutral)
 * 
 * PURPOSE:
 *   Coordinate hardware detection without architecture-specific logic.
 *   All actual probing is delegated to arch/ layer.
 * 
 * GUARANTEES:
 *   - Deterministic detection order
 *   - No memory allocation
 *   - No policy decisions
 *   - Fail fast on missing features
 * 
 * ARCHITECTURE:
 *   This file is THIN - orchestration only.
 *   Real probing happens in arch/x86_64/, arch/armv8/, etc.
 */

#include "boot_contract.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * BOOT INITIALIZATION
 * ======================================================================== */

void boot_init(boot_facts_t *facts) {
    memset(facts, 0, sizeof(boot_facts_t));
    
    facts->probed = false;
    facts->validated = false;
    facts->sealed = false;
}

/* ========================================================================
 * BOOT PROBING (Orchestration Only)
 * ======================================================================== */

bool boot_probe(boot_facts_t *facts) {
    if (facts->sealed) {
        return false;  /* Cannot reprobe sealed facts */
    }
    
    printf("[BOOT] Starting hardware detection...\n");
    
    /* Step 1: Probe CPU information */
    printf("[BOOT] Probing CPU...\n");
    if (!boot_probe_cpu_info(&facts->cpu_info)) {
        printf("[BOOT] FATAL: CPU detection failed\n");
        return false;
    }
    
    printf("[BOOT] CPU: %s (vendor=%d, family=%d, model=%d)\n",
           facts->cpu_info.brand_string,
           facts->cpu_info.vendor,
           facts->cpu_info.family,
           facts->cpu_info.model);
    
    /* Step 2: Probe cache topology */
    printf("[BOOT] Probing cache hierarchy...\n");
    if (!boot_probe_cache_topology(&facts->cache_topology)) {
        printf("[BOOT] FATAL: Cache detection failed\n");
        return false;
    }
    
    printf("[BOOT] Cache: %u levels detected\n", 
           facts->cache_topology.level_count);
    
    /* Step 3: Count cores */
    printf("[BOOT] Counting cores...\n");
    facts->cpu_count = boot_probe_cpu_count();
    if (facts->cpu_count == 0) {
        printf("[BOOT] FATAL: No CPUs detected\n");
        return false;
    }
    
    printf("[BOOT] CPUs: %u cores\n", facts->cpu_count);
    
    /* Step 4: Probe NUMA topology */
    printf("[BOOT] Probing NUMA...\n");
    facts->numa_nodes = boot_probe_numa_node_count();
    if (facts->numa_nodes == 0) {
        printf("[BOOT] WARNING: No NUMA detected (assuming 1)\n");
        facts->numa_nodes = 1;
    }
    
    printf("[BOOT] NUMA: %u nodes\n", facts->numa_nodes);
    
    /* Step 5: Detect SMT configuration */
    printf("[BOOT] Checking SMT...\n");
    facts->smt_enabled = boot_probe_smt_enabled();
    if (facts->smt_enabled) {
        facts->threads_per_core = boot_probe_threads_per_core();
        printf("[BOOT] SMT: ENABLED (%u threads per core)\n", 
               facts->threads_per_core);
    } else {
        facts->threads_per_core = 1;
        printf("[BOOT] SMT: DISABLED\n");
    }
    
    /* Step 6: Probe constant-time instruction support */
    printf("[BOOT] Probing constant-time support...\n");
    if (!boot_probe_constant_time_support(&facts->constant_time)) {
        printf("[BOOT] WARNING: Constant-time detection failed\n");
        facts->constant_time.valid = false;
    }
    
    /* Aggregate flag */
    facts->constant_time_supported = 
        facts->constant_time.valid &&
        facts->constant_time.aes_ni &&
        facts->constant_time.rdrand;
    
    printf("[BOOT] Constant-time: %s\n",
           facts->constant_time_supported ? "SUPPORTED" : "NOT SUPPORTED");
    
    /* Step 7: Probe cache control capabilities */
    printf("[BOOT] Probing cache control...\n");
    if (!boot_probe_cache_control(&facts->cache_control)) {
        printf("[BOOT] WARNING: Cache control detection failed\n");
        facts->cache_control.valid = false;
    }
    
    facts->cache_partitioning_supported = 
        facts->cache_control.valid &&
        (facts->cache_control.cat || facts->cache_control.cdp);
    
    printf("[BOOT] Cache partitioning: %s\n",
           facts->cache_partitioning_supported ? "SUPPORTED" : "NOT SUPPORTED");
    
    /* Step 8: Probe memory protection features */
    printf("[BOOT] Probing memory protection...\n");
    if (!boot_probe_memory_protection(&facts->memory_protection)) {
        printf("[BOOT] WARNING: Memory protection detection failed\n");
        facts->memory_protection.valid = false;
    }
    
    facts->memory_encryption_supported = 
        facts->memory_protection.valid &&
        facts->memory_protection.tme;
    
    /* Step 9: Probe side-channel mitigations */
    printf("[BOOT] Probing side-channel mitigations...\n");
    if (!boot_probe_side_channel_mitigation(&facts->side_channel_mitigation)) {
        printf("[BOOT] WARNING: Side-channel mitigation detection failed\n");
        facts->side_channel_mitigation.valid = false;
    }
    
    facts->side_channel_mitigations_available = 
        facts->side_channel_mitigation.valid &&
        facts->side_channel_mitigation.ibrs &&
        facts->side_channel_mitigation.stibp;
    
    printf("[BOOT] Side-channel mitigations: %s\n",
           facts->side_channel_mitigations_available ? "AVAILABLE" : "NOT AVAILABLE");
    
    /* Step 10: Check for hardware TRNG */
    printf("[BOOT] Checking TRNG...\n");
    facts->trng_available = boot_probe_trng_available();
    printf("[BOOT] TRNG: %s\n",
           facts->trng_available ? "AVAILABLE" : "NOT AVAILABLE");
    
    /* Step 11: Probe total memory */
    printf("[BOOT] Probing memory...\n");
    facts->total_memory_mb = boot_probe_total_memory_mb();
    printf("[BOOT] Memory: %lu MB\n", facts->total_memory_mb);
    
    /* Step 12: Check boot mode */
    printf("[BOOT] Checking boot mode...\n");
    facts->uefi_boot = boot_probe_uefi_boot();
    facts->secure_boot_enabled = boot_probe_secure_boot_enabled();
    printf("[BOOT] Boot mode: %s, Secure Boot: %s\n",
           facts->uefi_boot ? "UEFI" : "LEGACY",
           facts->secure_boot_enabled ? "ENABLED" : "DISABLED");
    
    /* Mark as probed */
    facts->probed = true;
    
    printf("[BOOT] Hardware detection complete\n");
    return true;
}

/* ========================================================================
 * BOOT VALIDATION
 * ======================================================================== */

static void boot_validation_context_init(boot_validation_context_t *ctx) {
    ctx->error_count = 0;
    ctx->worst_result = BOOT_VALIDATION_ACCEPT;
    memset(ctx->errors, 0, sizeof(ctx->errors));
}

static void boot_validation_context_add_error(
    boot_validation_context_t *ctx,
    boot_error_t error,
    boot_validation_result_t severity
) {
    if (ctx->error_count < 32) {
        ctx->errors[ctx->error_count++] = error;
    }
    
    if (severity > ctx->worst_result) {
        ctx->worst_result = severity;
    }
}

boot_validation_result_t boot_validate(
    boot_facts_t *facts,
    boot_validation_context_t *ctx
) {
    boot_validation_context_init(ctx);
    
    printf("[BOOT] Validating boot facts...\n");
    
    /* Validate probing completed */
    if (!facts->probed) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_CPU_DETECTION_FAILED, BOOT_VALIDATION_HARD_FAIL);
        return BOOT_VALIDATION_HARD_FAIL;
    }
    
    /* Validate minimum core count */
    if (facts->cpu_count < 2) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_TOO_FEW_CORES, BOOT_VALIDATION_HARD_FAIL);
        printf("[BOOT] FAIL: Too few cores (%u < 2)\n", facts->cpu_count);
    }
    
    /* Validate cache detection */
    if (facts->cache_topology.level_count == 0) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_NO_CACHE, BOOT_VALIDATION_HARD_FAIL);
        printf("[BOOT] FAIL: No cache detected\n");
    }
    
    /* Validate NUMA (warning only if absent) */
    if (facts->numa_nodes < 1) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_NO_NUMA, BOOT_VALIDATION_HARD_FAIL);
        printf("[BOOT] FAIL: No NUMA detected\n");
    }
    
    /* Check constant-time support (answer "what exists", not "what is sufficient") */
    if (!facts->constant_time_supported) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_NO_CONSTANT_TIME_SUPPORT, BOOT_VALIDATION_WARN);
        printf("[BOOT] WARN: Constant-time operations not fully supported\n");
    }
    
    /* Check TRNG availability (warning only) */
    if (!facts->trng_available) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_NO_TRNG, BOOT_VALIDATION_WARN);
        printf("[BOOT] WARN: Hardware TRNG not available\n");
    }
    
    /* Check SMT (warning only - some profiles allow it) */
    if (facts->smt_enabled) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_SMT_ENABLED_NOT_ALLOWED, BOOT_VALIDATION_WARN);
        printf("[BOOT] WARN: SMT is enabled\n");
    }
    
    /* Check secure boot (warning only) */
    if (!facts->secure_boot_enabled) {
        boot_validation_context_add_error(
            ctx, BOOT_ERROR_SECURE_BOOT_DISABLED, BOOT_VALIDATION_WARN);
        printf("[BOOT] WARN: Secure boot is disabled\n");
    }
    
    /* Mark as validated if no hard failures */
    if (ctx->worst_result != BOOT_VALIDATION_HARD_FAIL) {
        facts->validated = true;
        printf("[BOOT] Validation: %s\n",
               ctx->worst_result == BOOT_VALIDATION_WARN ? "PASS (with warnings)" : "PASS");
    } else {
        printf("[BOOT] Validation: FAIL\n");
    }
    
    return ctx->worst_result;
}

/* ========================================================================
 * BOOT SEALING
 * ======================================================================== */

bool boot_seal(boot_facts_t *facts) {
    if (!facts->validated) {
        printf("[BOOT] Cannot seal: not validated\n");
        return false;
    }
    
    if (facts->sealed) {
        printf("[BOOT] Already sealed\n");
        return false;
    }
    
    facts->sealed = true;
    printf("[BOOT] Boot facts SEALED (now immutable)\n");
    
    return true;
}

/* ========================================================================
 * ERROR REPORTING
 * ======================================================================== */

const char* boot_error_string(boot_error_t error) {
    switch (error) {
        case BOOT_ERROR_NONE:
            return "No error";
        case BOOT_ERROR_CPU_DETECTION_FAILED:
            return "CPU detection failed";
        case BOOT_ERROR_CACHE_DETECTION_FAILED:
            return "Cache detection failed";
        case BOOT_ERROR_NUMA_DETECTION_FAILED:
            return "NUMA detection failed";
        case BOOT_ERROR_TOO_FEW_CORES:
            return "Too few cores for Phase-1";
        case BOOT_ERROR_NO_CACHE:
            return "No cache hierarchy detected";
        case BOOT_ERROR_NO_NUMA:
            return "No NUMA detected";
        case BOOT_ERROR_NO_CONSTANT_TIME_SUPPORT:
            return "Constant-time operations not supported";
        case BOOT_ERROR_NO_CACHE_CONTROL:
            return "Cache control not available";
        case BOOT_ERROR_NO_MEMORY_PROTECTION:
            return "Memory protection features missing";
        case BOOT_ERROR_NO_SIDE_CHANNEL_MITIGATION:
            return "Side-channel mitigations unavailable";
        case BOOT_ERROR_NO_TRNG:
            return "Hardware TRNG not available";
        case BOOT_ERROR_SMT_ENABLED_NOT_ALLOWED:
            return "SMT is enabled (may violate security requirements)";
        case BOOT_ERROR_FREQ_SCALING_ENABLED:
            return "Frequency scaling is enabled";
        case BOOT_ERROR_SECURE_BOOT_DISABLED:
            return "Secure boot is disabled";
        case BOOT_WARN_ASYMMETRIC_CORES:
            return "Warning: Asymmetric core configuration";
        case BOOT_WARN_NUMA_DISABLED:
            return "Warning: NUMA disabled or not present";
        case BOOT_WARN_OLD_MICROCODE:
            return "Warning: Microcode may be outdated";
        default:
            return "Unknown error";
    }
}

void boot_validation_context_print(const boot_validation_context_t *ctx) {
    printf("Boot validation summary: %u error(s)\n", ctx->error_count);
    printf("Result: ");
    
    switch (ctx->worst_result) {
        case BOOT_VALIDATION_ACCEPT:
            printf("ACCEPT\n");
            break;
        case BOOT_VALIDATION_WARN:
            printf("WARN\n");
            break;
        case BOOT_VALIDATION_HARD_FAIL:
            printf("HARD_FAIL\n");
            break;
    }
    
    for (uint32_t i = 0; i < ctx->error_count; i++) {
        printf("  [%u] %s\n", i, boot_error_string(ctx->errors[i]));
    }
}

bool boot_validation_allows_boot(const boot_validation_context_t *ctx) {
    return ctx->worst_result != BOOT_VALIDATION_HARD_FAIL;
}

/* ========================================================================
 * ARCHITECTURE-SPECIFIC PROBE STUBS
 * 
 * These are weak symbols that MUST be overridden by arch/ layer.
 * If not overridden, they return safe defaults (failure).
 * ======================================================================== */

__attribute__((weak))
uint32_t boot_probe_cpu_count(void) {
    printf("[BOOT] ERROR: boot_probe_cpu_count() not implemented for this architecture\n");
    return 0;
}

__attribute__((weak))
uint32_t boot_probe_numa_node_count(void) {
    printf("[BOOT] ERROR: boot_probe_numa_node_count() not implemented\n");
    return 0;
}

__attribute__((weak))
uint32_t boot_probe_threads_per_core(void) {
    return 1;  /* Safe default: no SMT */
}

__attribute__((weak))
bool boot_probe_uefi_boot(void) {
    return false;  /* Conservative default */
}

__attribute__((weak))
bool boot_probe_secure_boot_enabled(void) {
    return false;  /* Conservative default */
}

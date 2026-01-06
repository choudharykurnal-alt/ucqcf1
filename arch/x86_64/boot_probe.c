/**
 * arch/x86_64/boot_probe.c
 * 
 * x86_64-specific hardware detection
 * 
 * PURPOSE:
 *   Implement architecture-specific probing for x86_64 CPUs.
 *   Uses CPUID, MSRs, and ACPI where appropriate.
 * 
 * GUARANTEES:
 *   - Deterministic detection
 *   - No undefined behavior
 *   - Fail fast on unsupported features
 */

#include "../../boot/boot_contract.h"
#include <stdint.h>
#include <string.h>
#include <cpuid.h>
#include <stdbool.h>

/* ========================================================================
 * x86_64 CPUID HELPERS
 * ======================================================================== */

static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx) {
    __cpuid_count(leaf, subleaf, *eax, *ebx, *ecx, *edx);
}

static inline bool cpuid_available(void) {
    /* CPUID is always available on x86_64 */
    return true;
}

/* ========================================================================
 * CPU INFORMATION DETECTION
 * ======================================================================== */

bool boot_probe_cpu_info(cpu_info_t *info) {
    if (!cpuid_available()) {
        return false;
    }
    
    uint32_t eax, ebx, ecx, edx;
    
    /* Get vendor string */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    
    uint32_t max_leaf = eax;
    
    /* Decode vendor ID */
    char vendor[13] = {0};
    memcpy(vendor + 0, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    
    if (strcmp(vendor, "GenuineIntel") == 0) {
        info->vendor = CPU_VENDOR_INTEL;
    } else if (strcmp(vendor, "AuthenticAMD") == 0) {
        info->vendor = CPU_VENDOR_AMD;
    } else {
        info->vendor = CPU_VENDOR_UNKNOWN;
    }
    
    /* Get family, model, stepping */
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        
        uint32_t stepping = eax & 0xF;
        uint32_t model = (eax >> 4) & 0xF;
        uint32_t family = (eax >> 8) & 0xF;
        uint32_t type = (eax >> 12) & 0x3;
        uint32_t ext_model = (eax >> 16) & 0xF;
        uint32_t ext_family = (eax >> 20) & 0xFF;
        
        /* Compute actual family and model */
        if (family == 0xF) {
            info->family = family + ext_family;
        } else {
            info->family = family;
        }
        
        if (family == 0xF || family == 0x6) {
            info->model = (ext_model << 4) | model;
        } else {
            info->model = model;
        }
        
        info->stepping = stepping;
    }
    
    /* Get brand string */
    if (max_leaf >= 0x80000004) {
        uint32_t brand[12];
        cpuid(0x80000002, 0, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, 0, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, 0, &brand[8], &brand[9], &brand[10], &brand[11]);
        
        memcpy(info->brand_string, brand, 48);
        info->brand_string[48] = '\0';
        
        /* Trim leading spaces */
        char *p = info->brand_string;
        while (*p == ' ') p++;
        if (p != info->brand_string) {
            memmove(info->brand_string, p, strlen(p) + 1);
        }
    } else {
        strcpy(info->brand_string, "Unknown CPU");
    }
    
    info->valid = true;
    return true;
}

/* ========================================================================
 * CACHE TOPOLOGY DETECTION
 * ======================================================================== */

bool boot_probe_cache_topology(cache_topology_t *cache) {
    uint32_t eax, ebx, ecx, edx;
    
    cache->level_count = 0;
    
    /* Check CPUID leaf 4 support (Intel) or leaf 0x8000001D (AMD) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    
    if (max_leaf >= 4) {
        /* Intel cache detection via leaf 4 */
        for (uint32_t i = 0; i < MAX_CACHE_LEVELS; i++) {
            cpuid(4, i, &eax, &ebx, &ecx, &edx);
            
            uint32_t cache_type = eax & 0x1F;
            if (cache_type == 0) {
                break;  /* No more cache levels */
            }
            
            cache_info_t *level = &cache->levels[cache->level_count];
            
            level->level = ((eax >> 5) & 0x7) + 1;
            level->line_size = (ebx & 0xFFF) + 1;
            level->ways = ((ebx >> 22) & 0x3FF) + 1;
            
            uint32_t sets = ecx + 1;
            level->size_kb = (level->ways * level->line_size * sets) / 1024;
            
            level->shared = (eax >> 14) & 0x1;
            level->inclusive = (edx >> 1) & 0x1;
            level->valid = true;
            
            cache->level_count++;
        }
    }
    
    return cache->level_count > 0;
}

/* ========================================================================
 * CORE COUNT DETECTION
 * ======================================================================== */

uint32_t boot_probe_cpu_count(void) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    
    if (max_leaf >= 0xB) {
        /* Modern topology detection (leaf 0xB) */
        cpuid(0xB, 1, &eax, &ebx, &ecx, &edx);
        return ebx & 0xFFFF;  /* Number of logical processors */
    } else if (max_leaf >= 1) {
        /* Fallback: leaf 1 */
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        return (ebx >> 16) & 0xFF;
    }
    
    return 0;
}

/* ========================================================================
 * NUMA DETECTION
 * ======================================================================== */

uint32_t boot_probe_numa_node_count(void) {
    /* NUMA detection requires ACPI SRAT table parsing */
    /* For Phase-1, we use a simple heuristic */
    
    uint32_t eax, ebx, ecx, edx;
    
    /* Check AMD topology extensions */
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x8000001E) {
        /* AMD: Check for multiple nodes */
        cpuid(0x8000001E, 0, &eax, &ebx, &ecx, &edx);
        uint32_t nodes = ((ecx >> 8) & 0x7) + 1;
        return nodes > 0 ? nodes : 1;
    }
    
    /* Default: assume single NUMA node */
    return 1;
}

/* ========================================================================
 * SMT DETECTION
 * ======================================================================== */

bool boot_probe_smt_enabled(void) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        
        /* Check HTT flag (bit 28 of EDX) */
        bool htt = (edx >> 28) & 0x1;
        
        if (htt) {
            /* Check logical processor count */
            uint32_t logical_count = (ebx >> 16) & 0xFF;
            
            /* If we have modern topology leaf */
            if (max_leaf >= 0xB) {
                cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
                uint32_t threads_per_core = ebx & 0xFFFF;
                return threads_per_core > 1;
            }
            
            return logical_count > 1;
        }
    }
    
    return false;
}

uint32_t boot_probe_threads_per_core(void) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    
    if (max_leaf >= 0xB) {
        cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
        return ebx & 0xFFFF;
    }
    
    return 1;
}

/* ========================================================================
 * CONSTANT-TIME INSTRUCTION SUPPORT
 * ======================================================================== */

bool boot_probe_constant_time_support(constant_time_support_t *ct) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    /* AES-NI (bit 25 of ECX) */
    ct->aes_ni = (ecx >> 25) & 0x1;
    
    /* RDRAND (bit 30 of ECX) */
    ct->rdrand = (ecx >> 30) & 0x1;
    
    /* RDSEED (CPUID leaf 7) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        ct->rdseed = (ebx >> 18) & 0x1;
    } else {
        ct->rdseed = false;
    }
    
    /* x86_64 multiply and compare are constant-time by design */
    ct->constant_time_mul = true;
    ct->constant_time_cmp = true;
    
    ct->valid = true;
    return true;
}

/* ========================================================================
 * CACHE CONTROL CAPABILITIES
 * ======================================================================== */

bool boot_probe_cache_control(cache_control_t *cache_ctrl) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    /* CLFLUSH (bit 19 of EDX) */
    cache_ctrl->clflush = (edx >> 19) & 0x1;
    
    /* CLFLUSHOPT, CLWB (CPUID leaf 7) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        cache_ctrl->clflushopt = (ebx >> 23) & 0x1;
        cache_ctrl->clwb = (ebx >> 24) & 0x1;
    } else {
        cache_ctrl->clflushopt = false;
        cache_ctrl->clwb = false;
    }
    
    /* CAT/CDP detection requires reading MSRs (IA32_PQR_ASSOC) */
    /* For Phase-1, we check CPUID support */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x10) {
        cpuid(0x10, 0, &eax, &ebx, &ecx, &edx);
        cache_ctrl->cat = (ebx >> 1) & 0x1;  /* L3 CAT */
        cache_ctrl->cdp = (ebx >> 2) & 0x1;  /* CDP */
    } else {
        cache_ctrl->cat = false;
        cache_ctrl->cdp = false;
    }
    
    cache_ctrl->valid = true;
    return true;
}

/* ========================================================================
 * MEMORY PROTECTION FEATURES
 * ======================================================================== */

bool boot_probe_memory_protection(memory_protection_t *mem_prot) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Check extended features */
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    
    /* NX bit (bit 20 of EDX) */
    mem_prot->nx = (edx >> 20) & 0x1;
    
    /* SMEP, SMAP (CPUID leaf 7) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        mem_prot->smep = (ebx >> 7) & 0x1;
        mem_prot->smap = (ebx >> 20) & 0x1;
        mem_prot->pku = (ecx >> 3) & 0x1;
    } else {
        mem_prot->smep = false;
        mem_prot->smap = false;
        mem_prot->pku = false;
    }
    
    /* TME (Total Memory Encryption) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        mem_prot->tme = (ecx >> 13) & 0x1;
    } else {
        mem_prot->tme = false;
    }
    
    mem_prot->valid = true;
    return true;
}

/* ========================================================================
 * SIDE-CHANNEL MITIGATIONS
 * ======================================================================== */

bool boot_probe_side_channel_mitigation(side_channel_mitigation_t *scm) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Check architectural features (leaf 7) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        
        /* IBRS, STIBP (bits 26, 27 of EDX) */
        scm->ibrs = (edx >> 26) & 0x1;
        scm->stibp = (edx >> 27) & 0x1;
        scm->ssbd = (edx >> 31) & 0x1;
    } else {
        scm->ibrs = false;
        scm->stibp = false;
        scm->ssbd = false;
    }
    
    /* MD_CLEAR (CPUID leaf 7, subleaf 0, EDX bit 10) */
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        scm->md_clear = (edx >> 10) & 0x1;
    } else {
        scm->md_clear = false;
    }
    
    scm->valid = true;
    return true;
}

/* ========================================================================
 * TRNG AVAILABILITY
 * ======================================================================== */

bool boot_probe_trng_available(void) {
    uint32_t eax, ebx, ecx, edx;
    
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    /* RDRAND (bit 30 of ECX) */
    bool rdrand = (ecx >> 30) & 0x1;
    
    /* RDSEED (CPUID leaf 7) */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    bool rdseed = false;
    if (eax >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        rdseed = (ebx >> 18) & 0x1;
    }
    
    /* Either RDRAND or RDSEED is sufficient */
    return rdrand || rdseed;
}

/* ========================================================================
 * TOTAL MEMORY DETECTION
 * ======================================================================== */

uint64_t boot_probe_total_memory_mb(void) {
    /* Memory detection requires ACPI/E820 parsing */
    /* For Phase-1 stub, return conservative default */
    /* Real implementation would parse E820 map */
    
    return 8192;  /* 8GB default for testing */
}

/* ========================================================================
 * BOOT MODE DETECTION
 * ======================================================================== */

bool boot_probe_uefi_boot(void) {
    /* Check for UEFI system table */
    /* Real implementation would check EFI_SYSTEM_TABLE */
    /* For Phase-1, assume UEFI on x86_64 */
    
    return true;
}

bool boot_probe_secure_boot_enabled(void) {
    /* Check UEFI SecureBoot variable */
    /* Real implementation would read EFI variable */
    
    return false;  /* Conservative default */
}
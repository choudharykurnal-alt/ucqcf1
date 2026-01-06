// Copyright 2024 Google LLC
//
#ifndef SCHEDULER_SCHEDULER_CONTRACT_H_
#define SCHEDULER_SCHEDULER_CONTRACT_H_

#include <stdbool.h>

// Opaque handles to other system components (to be defined elsewhere).
typedef struct boot_facts boot_facts_t;
typedef struct topology topology_t;
typedef struct domain_graph domain_graph_t;

// Identifiers for tasks, cores, and domains.
typedef int task_id_t;
typedef int core_id_t;
typedef int domain_id_t;

// The scheduler's state. It holds pointers to the sealed system information.
// This is exposed to allow for stack allocation in tests and other modules.
typedef struct scheduler {
    const boot_facts_t* boot_facts;
    const topology_t* topology;
    const domain_graph_t* domain_graph;
} scheduler_t;

// Initializes the scheduler with sealed system information.
// This function must be called once before any other scheduler function.
void scheduler_init(scheduler_t* scheduler, const boot_facts_t* boot_facts,
                    const topology_t* topology,
                    const domain_graph_t* domain_graph);

// Checks if a given task can be scheduled on a specific core.
// This is a pure, stateless function that relies on the sealed scheduler rules.
bool can_schedule_task(const scheduler_t* scheduler, task_id_t task,
                       core_id_t core);

// Enforces preemption rules between two domains.
// This function is responsible for ensuring that a higher-priority domain
// can preempt a lower-priority one.
void enforce_preemption(const scheduler_t* scheduler, domain_id_t preempting,
                        domain_id_t preempted);

#endif  // SCHEDULER_SCHEDULER_CONTRACT_H_

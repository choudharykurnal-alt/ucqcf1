// Copyright 2024 Google LLC
//
#include <stdio.h>
#include <assert.h>

#include "scheduler/scheduler_contract.h"

// Dummy structs for testing since the real ones are not defined.
// In a real project, these would be included from their respective
// contract headers.
typedef struct boot_facts { int bf_placeholder; } boot_facts_t;
typedef struct topology { int t_placeholder; } topology_t;
typedef struct domain_graph { int dg_placeholder; } domain_graph_t;

void test_scheduler_init() {
    printf("Running test: test_scheduler_init\\n");
    scheduler_t scheduler;
    boot_facts_t bf;
    topology_t topo;
    domain_graph_t dg;

    scheduler_init(&scheduler, &bf, &topo, &dg);

    // Since the state is now exposed, we could add assertions here
    // to check if the pointers were set correctly.
    // For now, we'll just ensure it runs.
    assert(scheduler.boot_facts == &bf);
    assert(scheduler.topology == &topo);
    assert(scheduler.domain_graph == &dg);

    printf("test_scheduler_init: PASS\\n");
}

void test_can_schedule_task() {
    printf("Running test: test_can_schedule_task\\n");
    scheduler_t scheduler;
    boot_facts_t bf;
    topology_t topo;
    domain_graph_t dg;

    scheduler_init(&scheduler, &bf, &topo, &dg);

    // Test case 1: Valid task and core
    assert(can_schedule_task(&scheduler, 1, 1) == true);
    printf("  - Valid task and core: PASS\\n");

    // Test case 2: Invalid task ID
    assert(can_schedule_task(&scheduler, -1, 1) == false);
    printf("  - Invalid task ID: PASS\\n");

    // Test case 3: Invalid core ID
    assert(can_schedule_task(&scheduler, 1, -1) == false);
    printf("  - Invalid core ID: PASS\\n");

    // Test case 4: Null scheduler pointer
    assert(can_schedule_task(NULL, 1, 1) == false);
    printf("  - Null scheduler pointer: PASS\\n");

    printf("test_can_schedule_task: PASS\\n");
}

int main() {
    test_scheduler_init();
    test_can_schedule_task();

    printf("\\nAll scheduler tests passed!\\n");
    return 0;
}

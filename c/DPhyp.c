#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MAX_RELATIONS 64
#define MAX_EDGES 128
#define DP_TABLE_SIZE 65536

typedef uint64_t node_set_t;

/* --- Query Graph & Plan Structures --- */

typedef struct {
    node_set_t u;           // Hypernode u
    node_set_t v;           // Hypernode v (u and v are disjoint)
    double selectivity;     // Predicate selectivity
} HyperEdge;

typedef struct {
    int num_nodes;
    char node_names[MAX_RELATIONS][32];
    double base_cardinalities[MAX_RELATIONS];
    HyperEdge edges[MAX_EDGES];
    int num_edges;
} HyperGraph;

typedef struct Plan {
    node_set_t relations;   // Set of relations joined in this plan
    double cost;            // Plan cost
    double cardinality;     // Estimated output cardinality
    struct Plan *left;      // Left subplan
    struct Plan *right;     // Right subplan
} Plan;

/* --- Hash Table for Dynamic Programming Table --- */

typedef struct DPTableEntry {
    node_set_t key;
    Plan *plan;
    struct DPTableEntry *next;
} DPTableEntry;

typedef struct {
    DPTableEntry *buckets[DP_TABLE_SIZE];
} DPTable;

static inline uint32_t hash_set(node_set_t s) {
    s ^= s >> 33;
    s *= 0xff51afd7ed558ccdULL;
    s ^= s >> 33;
    return (uint32_t)(s % DP_TABLE_SIZE);
}

static void dp_init(DPTable *table) {
    memset(table->buckets, 0, sizeof(table->buckets));
}

static Plan* dp_lookup(DPTable *table, node_set_t key) {
    uint32_t bucket = hash_set(key);
    for (DPTableEntry *e = table->buckets[bucket]; e != NULL; e = e->next) {
        if (e->key == key) return e->plan;
    }
    return NULL;
}

static void dp_insert(DPTable *table, node_set_t key, Plan *p) {
    uint32_t bucket = hash_set(key);
    for (DPTableEntry *e = table->buckets[bucket]; e != NULL; e = e->next) {
        if (e->key == key) {
            e->plan = p;
            return;
        }
    }
    DPTableEntry *entry = (DPTableEntry*)malloc(sizeof(DPTableEntry));
    entry->key = key;
    entry->plan = p;
    entry->next = table->buckets[bucket];
    table->buckets[bucket] = entry;
}

/* --- Helper Bit Utilities --- */

// Lowest index bit (min(S) in paper)
static inline int get_min_node(node_set_t s) {
    return __builtin_ctzll(s);
}

// B_v = {w | w <= v}, where node ordering is 0 < 1 < ... < n-1
static inline node_set_t get_B(int node_idx) {
    if (node_idx >= 63) return ~0ULL;
    return (1ULL << (node_idx + 1)) - 1;
}

/* Checks if there is a hyperedge connecting S1 and S2 */
static bool is_connected(const HyperGraph *g, node_set_t S1, node_set_t S2) {
    for (int i = 0; i < g->num_edges; i++) {
        node_set_t u = g->edges[i].u;
        node_set_t v = g->edges[i].v;
        if (((u & S1) == u && (v & S2) == v) ||
            ((v & S1) == v && (u & S2) == u)) {
            return true;
        }
    }
    return false;
}

/* Computes N(S, X): the neighborhood of S excluding X (Eq. 1 in paper) */
static node_set_t calc_neighborhood(const HyperGraph *g, node_set_t S, node_set_t X) {
    node_set_t candidates[MAX_EDGES * 2];
    int cand_count = 0;

    // Collect all interesting target hypernodes
    for (int i = 0; i < g->num_edges; i++) {
        node_set_t u = g->edges[i].u;
        node_set_t v = g->edges[i].v;

        if ((u & S) == u && (v & (S | X)) == 0) {
            candidates[cand_count++] = v;
        }
        if ((v & S) == v && (u & (S | X)) == 0) {
            candidates[cand_count++] = u;
        }
    }

    // Eliminate subsumed hypernodes (E_down(S, X))
    node_set_t N = 0;
    for (int i = 0; i < cand_count; i++) {
        bool subsumed = false;
        for (int j = 0; j < cand_count; j++) {
            if (i != j && (candidates[j] & candidates[i]) == candidates[j] 
                       && candidates[j] != candidates[i]) {
                subsumed = true;
                break;
            }
        }
        if (!subsumed) {
            // Add min(v) to neighborhood
            int min_elem = get_min_node(candidates[i]);
            N |= (1ULL << min_elem);
        }
    }
    return N;
}

/* --- DPhyp Context & Subroutines --- */

typedef struct {
    const HyperGraph *g;
    DPTable dpTable;
} DPhyp;

// Forward declarations
static void EmitCsg(DPhyp *ctx, node_set_t S1);
static void EnumerateCsgRec(DPhyp *ctx, node_set_t S1, node_set_t X);
static void EnumerateCmpRec(DPhyp *ctx, node_set_t S1, node_set_t S2, node_set_t X);
static void EmitCsgCmp(DPhyp *ctx, node_set_t S1, node_set_t S2);

/* Section 3.5: EmitCsgCmp joins plans for S1 and S2 */
static void EmitCsgCmp(DPhyp *ctx, node_set_t S1, node_set_t S2) {
    Plan *p1 = dp_lookup(&ctx->dpTable, S1);
    Plan *p2 = dp_lookup(&ctx->dpTable, S2);
    if (!p1 || !p2) return;

    node_set_t S = S1 | S2;

    // Calculate selectivity across all connecting hyperedges
    double sel = 1.0;
    for (int i = 0; i < ctx->g->num_edges; i++) {
        node_set_t u = ctx->g->edges[i].u;
        node_set_t v = ctx->g->edges[i].v;
        if (((u & S1) == u && (v & S2) == v) ||
            ((v & S1) == v && (u & S2) == u)) {
            sel *= ctx->g->edges[i].selectivity;
        }
    }

    double card = p1->cardinality * p2->cardinality * sel;
    // Standard C_out cost model
    double cost = p1->cost + p2->cost + card;

    Plan *existing = dp_lookup(&ctx->dpTable, S);
    if (existing == NULL || cost < existing->cost) {
        Plan *new_plan = (Plan*)malloc(sizeof(Plan));
        new_plan->relations = S;
        new_plan->cardinality = card;
        new_plan->cost = cost;
        new_plan->left = p1;
        new_plan->right = p2;
        dp_insert(&ctx->dpTable, S, new_plan);
    }
}

/* Section 3.4: EnumerateCmpRec */
static void EnumerateCmpRec(DPhyp *ctx, node_set_t S1, node_set_t S2, node_set_t X) {
    node_set_t N_set = calc_neighborhood(ctx->g, S2, X);

    // Vance & Maier subset enumeration in ascending order
    for (node_set_t N = (0 - N_set) & N_set; N != 0; N = (N - N_set) & N_set) {
        if (dp_lookup(&ctx->dpTable, S2 | N) != NULL && is_connected(ctx->g, S1, S2 | N)) {
            EmitCsgCmp(ctx, S1, S2 | N);
        }
    }

    X |= N_set;

    for (node_set_t N = (0 - N_set) & N_set; N != 0; N = (N - N_set) & N_set) {
        EnumerateCmpRec(ctx, S1, S2 | N, X);
    }
}

/* Section 3.3: EmitCsg */
static void EmitCsg(DPhyp *ctx, node_set_t S1) {
    int min_s1 = get_min_node(S1);
    node_set_t X = S1 | get_B(min_s1);
    node_set_t N_set = calc_neighborhood(ctx->g, S1, X);

    // Iterate over v in N descending
    node_set_t temp = N_set;
    while (temp > 0) {
        int v_idx = 63 - __builtin_clzll(temp);
        node_set_t v = (1ULL << v_idx);
        temp &= ~v;

        node_set_t S2 = v;
        if (is_connected(ctx->g, S1, S2)) {
            EmitCsgCmp(ctx, S1, S2);
        }
        // Exclude nodes <= v in N to avoid duplicates
        node_set_t B_v_N = N_set & get_B(v_idx);
        EnumerateCmpRec(ctx, S1, S2, X | B_v_N);
    }
}

/* Section 3.2: EnumerateCsgRec */
static void EnumerateCsgRec(DPhyp *ctx, node_set_t S1, node_set_t X) {
    node_set_t N_set = calc_neighborhood(ctx->g, S1, X);

    // First loop: emit connected subgraphs
    for (node_set_t N = (0 - N_set) & N_set; N != 0; N = (N - N_set) & N_set) {
        if (dp_lookup(&ctx->dpTable, S1 | N) != NULL) {
            EmitCsg(ctx, S1 | N);
        }
    }

    // Second loop: recursive expansion
    for (node_set_t N = (0 - N_set) & N_set; N != 0; N = (N - N_set) & N_set) {
        EnumerateCsgRec(ctx, S1 | N, X | N_set);
    }
}

/* Section 3.1: Solve */
static Plan* Solve(DPhyp *ctx) {
    int n = ctx->g->num_nodes;
    dp_init(&ctx->dpTable);

    // Initialize dpTable with single relations
    for (int i = 0; i < n; i++) {
        Plan *p = (Plan*)malloc(sizeof(Plan));
        p->relations = (1ULL << i);
        p->cardinality = ctx->g->base_cardinalities[i];
        p->cost = 0.0;
        p->left = NULL;
        p->right = NULL;
        dp_insert(&ctx->dpTable, 1ULL << i, p);
    }

    // Process nodes descending according to <
    for (int i = n - 1; i >= 0; i--) {
        node_set_t v = (1ULL << i);
        EmitCsg(ctx, v);
        EnumerateCsgRec(ctx, v, get_B(i));
    }

    node_set_t all_nodes = (n == 64) ? ~0ULL : ((1ULL << n) - 1);
    return dp_lookup(&ctx->dpTable, all_nodes);
}

/* --- Pretty Printing of Resulting Plan --- */

static void print_plan(const HyperGraph *g, const Plan *p) {
    if (!p) return;
    if (p->left == NULL && p->right == NULL) {
        int idx = get_min_node(p->relations);
        printf("%s", g->node_names[idx]);
        return;
    }
    printf("(");
    print_plan(g, p->left);
    printf(" ⨝ ");
    print_plan(g, p->right);
    printf(")");
}

/* --- Example: Hypergraph from Figure 2 in the paper --- */
int main(void) {
    HyperGraph g;
    g.num_nodes = 6;

    const char *names[] = {"R1", "R2", "R3", "R4", "R5", "R6"};
    for (int i = 0; i < 6; i++) {
        strcpy(g.node_names[i], names[i]);
        g.base_cardinalities[i] = 1000.0 * (i + 1); // 1K, 2K, ..., 6K tuples
    }

    g.num_edges = 0;

    // Helper macro to add edge
    #define ADD_EDGE(u_mask, v_mask, sel) \
        g.edges[g.num_edges].u = (u_mask); \
        g.edges[g.num_edges].v = (v_mask); \
        g.edges[g.num_edges].selectivity = (sel); \
        g.num_edges++;

    // Simple edges: ({R1}, {R2}), ({R2}, {R3}), ({R4}, {R5}), ({R5}, {R6})
    ADD_EDGE(1ULL << 0, 1ULL << 1, 0.01);
    ADD_EDGE(1ULL << 1, 1ULL << 2, 0.01);
    ADD_EDGE(1ULL << 3, 1ULL << 4, 0.01);
    ADD_EDGE(1ULL << 4, 1ULL << 5, 0.01);

    // Hyperedge: ({R1, R2, R3}, {R4, R5, R6})
    // Corresponding to: R1.a + R2.b + R3.c = R4.d + R5.e + R6.f
    node_set_t u_hyper = (1ULL << 0) | (1ULL << 1) | (1ULL << 2);
    node_set_t v_hyper = (1ULL << 3) | (1ULL << 4) | (1ULL << 5);
    ADD_EDGE(u_hyper, v_hyper, 0.005);

    #undef ADD_EDGE

    DPhyp ctx;
    ctx.g = &g;

    printf("Executing DPhyp for Figure 2 query hypergraph...\n");
    Plan *best_plan = Solve(&ctx);

    if (best_plan) {
        printf("\nOptimal Join Plan:\n  ");
        print_plan(&g, best_plan);
        printf("\nEstimated Cost:        %.2f\n", best_plan->cost);
        printf("Estimated Cardinality: %.2f\n", best_plan->cardinality);
    } else {
        printf("No plan found (query hypergraph is disconnected).\n");
    }

    return 0;
}

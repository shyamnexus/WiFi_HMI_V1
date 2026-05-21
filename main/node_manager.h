#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Node manager — stores and retrieves wireless node details.
 * Nodes are persisted in NVS for recovery after reboot.
 */

#define NODE_ID_LEN     32
#define NODE_IP_LEN     16
#define MAX_NODES       50

typedef struct {
    char node_id[NODE_ID_LEN];
    char ip_addr[NODE_IP_LEN];
    uint16_t port;
    bool active;
} node_info_t;

/**
 * Initialize node manager (load from NVS).
 */
esp_err_t node_manager_init(void);

/**
 * Add a new node / update existing node with same ID.
 * If node_id already exists, updates its IP/port.
 */
esp_err_t node_manager_add(const char *node_id, const char *ip_addr, uint16_t port);

/**
 * Remove a node by ID.
 */
esp_err_t node_manager_remove(const char *node_id);

/**
 * Get node info by ID.
 * Returns ESP_OK if found, ESP_ERR_NOT_FOUND if not.
 */
esp_err_t node_manager_get(const char *node_id, node_info_t *out_node);

/**
 * Get all nodes.
 * Returns number of nodes in array, or error.
 */
int node_manager_get_all(node_info_t *out_nodes, int max_count);

/**
 * Get number of stored nodes.
 */
int node_manager_count(void);

/**
 * Mark a node as active/inactive (for UI display).
 */
esp_err_t node_manager_set_active(const char *node_id, bool active);

/**
 * Save all nodes to NVS.
 * Called automatically on add/remove, but can be called explicitly.
 */
esp_err_t node_manager_save(void);

/**
 * Load all nodes from NVS.
 */
esp_err_t node_manager_load(void);

/**
 * Clear all nodes from memory and NVS.
 */
esp_err_t node_manager_clear(void);

#ifdef __cplusplus
}
#endif

#include "node_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"

static const char *TAG = "node_mgr";
#define NVS_NAMESPACE "nodes"

/* In-memory cache */
static node_info_t s_nodes[MAX_NODES];
static int s_node_count = 0;

esp_err_t node_manager_init(void)
{
    memset(s_nodes, 0, sizeof(s_nodes));
    s_node_count = 0;
    
    esp_err_t ret = node_manager_load();
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No nodes stored yet");
        return ESP_OK;
    }
    return ret;
}

static int find_node_index(const char *node_id)
{
    for (int i = 0; i < s_node_count; i++) {
        if (strcmp(s_nodes[i].node_id, node_id) == 0) {
            return i;
        }
    }
    return -1;
}

esp_err_t node_manager_add(const char *node_id, const char *ip_addr, uint16_t port)
{
    if (!node_id || !ip_addr) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = find_node_index(node_id);
    
    if (idx >= 0) {
        /* Update existing node */
        strncpy(s_nodes[idx].ip_addr, ip_addr, NODE_IP_LEN - 1);
        s_nodes[idx].port = port;
        s_nodes[idx].active = true;
        ESP_LOGI(TAG, "Node updated: %s -> %s:%u", node_id, ip_addr, port);
    } else {
        /* Add new node */
        if (s_node_count >= MAX_NODES) {
            ESP_LOGE(TAG, "Max nodes reached (%d)", MAX_NODES);
            return ESP_ERR_NO_MEM;
        }
        
        strncpy(s_nodes[s_node_count].node_id, node_id, NODE_ID_LEN - 1);
        strncpy(s_nodes[s_node_count].ip_addr, ip_addr, NODE_IP_LEN - 1);
        s_nodes[s_node_count].port = port;
        s_nodes[s_node_count].active = true;
        
        s_node_count++;
        ESP_LOGI(TAG, "Node added: %s -> %s:%u", node_id, ip_addr, port);
    }

    return node_manager_save();
}

esp_err_t node_manager_remove(const char *node_id)
{
    int idx = find_node_index(node_id);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Shift remaining nodes down */
    for (int i = idx; i < s_node_count - 1; i++) {
        s_nodes[i] = s_nodes[i + 1];
    }
    s_node_count--;

    ESP_LOGI(TAG, "Node removed: %s", node_id);
    return node_manager_save();
}

esp_err_t node_manager_get(const char *node_id, node_info_t *out_node)
{
    int idx = find_node_index(node_id);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(out_node, &s_nodes[idx], sizeof(node_info_t));
    return ESP_OK;
}

int node_manager_get_all(node_info_t *out_nodes, int max_count)
{
    int count = (s_node_count < max_count) ? s_node_count : max_count;
    memcpy(out_nodes, s_nodes, count * sizeof(node_info_t));
    return count;
}

int node_manager_count(void)
{
    return s_node_count;
}

esp_err_t node_manager_set_active(const char *node_id, bool active)
{
    int idx = find_node_index(node_id);
    if (idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    s_nodes[idx].active = active;
    return ESP_OK;
}

esp_err_t node_manager_save(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Store node count */
    ret = nvs_set_i32(nvs_handle, "count", s_node_count);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    /* Store each node as blob */
    for (int i = 0; i < s_node_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "node_%d", i);
        ret = nvs_set_blob(nvs_handle, key, &s_nodes[i], sizeof(node_info_t));
        if (ret != ESP_OK) {
            nvs_close(nvs_handle);
            return ret;
        }
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Nodes saved to NVS (%d nodes)", s_node_count);
    }
    return ret;
}

esp_err_t node_manager_load(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    /* Load node count */
    int32_t count = 0;
    ret = nvs_get_i32(nvs_handle, "count", &count);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    if (count > MAX_NODES) {
        count = MAX_NODES;
    }

    /* Load each node */
    for (int i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "node_%d", i);
        size_t blob_len = sizeof(node_info_t);
        ret = nvs_get_blob(nvs_handle, key, &s_nodes[i], &blob_len);
        if (ret != ESP_OK) {
            nvs_close(nvs_handle);
            return ret;
        }
    }

    s_node_count = count;
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Loaded %d nodes from NVS", s_node_count);
    return ESP_OK;
}

esp_err_t node_manager_clear(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_all(nvs_handle);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    memset(s_nodes, 0, sizeof(s_nodes));
    s_node_count = 0;

    ESP_LOGI(TAG, "All nodes cleared");
    return ret;
}

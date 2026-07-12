#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct rad_i2c_device {
    uint32_t bus_id;
    uint8_t address;
    uint32_t irq_count;
    rad_irq_resource_t irqs[RAD_IRQ_MAX_RESOURCES];
    char path[RAD_TREE_MAX_PATH];
    char compatible[RAD_COMPATIBLE_MAX];
    char driver[RAD_DRIVER_NAME_MAX];
    void *controller;
    uint32_t driver_index;
    int bound;
    int open_count;
};

struct rad_spi_device {
    uint32_t bus_id;
    uint8_t cs;
    uint32_t clock_hz;
    uint8_t mode;
    uint8_t bits_per_word;
    uint8_t transfer_mode;
    uint32_t irq_count;
    rad_irq_resource_t irqs[RAD_IRQ_MAX_RESOURCES];
    char path[RAD_TREE_MAX_PATH];
    char compatible[RAD_COMPATIBLE_MAX];
    char driver[RAD_DRIVER_NAME_MAX];
    void *controller;
    uint32_t driver_index;
    int bound;
    int open_count;
};

struct rad_dma_channel_handle {
    uint32_t controller_index;
    uint32_t index;
    rad_dma_request_id_t request_id;
    void *backend_channel;
    int used;
    int active;
};

struct rad_framebuffer_handle {
    uint32_t index;
};

struct rad_irq_domain_handle {
    char name[RAD_IRQ_DOMAIN_NAME_MAX];
    char tree_path[RAD_TREE_MAX_PATH];
    uint32_t interrupt_base;
    uint32_t line_count;
    uint32_t interrupt_cells;
    int used;
};

#ifndef RADIX_KERNEL_MAX_TREE_NODES
#define RADIX_KERNEL_MAX_TREE_NODES 64u
#endif

#ifndef RADIX_KERNEL_MAX_TREE_PROPS
#define RADIX_KERNEL_MAX_TREE_PROPS 192u
#endif

#ifndef RADIX_KERNEL_MAX_OVERLAYS
#define RADIX_KERNEL_MAX_OVERLAYS 16u
#endif

#ifndef RADIX_KERNEL_MAX_I2C_CONTROLLERS
#define RADIX_KERNEL_MAX_I2C_CONTROLLERS 8u
#endif

#ifndef RADIX_KERNEL_MAX_SPI_CONTROLLERS
#define RADIX_KERNEL_MAX_SPI_CONTROLLERS 8u
#endif

#ifndef RADIX_KERNEL_MAX_I2C_DEVICES
#define RADIX_KERNEL_MAX_I2C_DEVICES 32u
#endif

#ifndef RADIX_KERNEL_MAX_SPI_DEVICES
#define RADIX_KERNEL_MAX_SPI_DEVICES 32u
#endif

#ifndef RADIX_KERNEL_MAX_BUS_DRIVERS
#define RADIX_KERNEL_MAX_BUS_DRIVERS 16u
#endif

#ifndef RADIX_KERNEL_MAX_DMA_CONTROLLERS
#define RADIX_KERNEL_MAX_DMA_CONTROLLERS 4u
#endif

#ifndef RADIX_KERNEL_MAX_DMA_CHANNELS
#define RADIX_KERNEL_MAX_DMA_CHANNELS 16u
#endif

#ifndef RADIX_KERNEL_MAX_FRAMEBUFFERS
#define RADIX_KERNEL_MAX_FRAMEBUFFERS 4u
#endif

#ifndef RADIX_KERNEL_MAX_IRQ_DOMAINS
#define RADIX_KERNEL_MAX_IRQ_DOMAINS 16u
#endif

#ifndef RADIX_KERNEL_MAX_SERVICES
#define RADIX_KERNEL_MAX_SERVICES 16u
#endif

#ifndef RADIX_KERNEL_SPI_AUTO_DMA_THRESHOLD
#define RADIX_KERNEL_SPI_AUTO_DMA_THRESHOLD 64u
#endif

namespace {

#pragma pack(push, 1)
struct OverlayHeader {
    char magic[4];
    uint16_t version;
    uint16_t flags;
    uint32_t string_size;
    uint32_t node_count;
    uint32_t property_count;
    uint32_t data_size;
    uint32_t reserved;
    uint32_t crc32;
};

struct OverlayNodeRecord {
    uint32_t path_offset;
    uint32_t name_offset;
    uint32_t parent_offset;
    uint32_t property_start;
    uint32_t property_count;
};

struct OverlayPropertyRecord {
    uint32_t node_index;
    uint32_t name_offset;
    uint32_t type;
    uint32_t value;
    uint32_t size;
};
#pragma pack(pop)

struct TreeProperty {
    char name[RAD_TREE_MAX_PROPERTY_NAME];
    rad_tree_property_type_t type;
    uint32_t u32_value;
    int bool_value;
    char string_value[RAD_TREE_MAX_VALUE];
    uint32_t array_values[8];
    uint32_t array_count;
    int used;
};

struct TreeNode {
    char path[RAD_TREE_MAX_PATH];
    char name[RAD_TREE_MAX_PROPERTY_NAME];
    char parent[RAD_TREE_MAX_PATH];
    char module[RAD_TREE_MAX_PROPERTY_NAME];
    uint16_t properties[16];
    uint32_t property_count;
    int bound;
    int used;
};

struct OverlayRecord {
    char name[RAD_TREE_MAX_PROPERTY_NAME];
    char source[RAD_TREE_MAX_PATH];
    uint32_t node_count;
    uint32_t property_count;
    int used;
};

struct I2cController {
    uint32_t bus_id;
    char name[RAD_I2C_BUS_NAME_MAX];
    char device_name[32];
    char tree_path[RAD_TREE_MAX_PATH];
    uint32_t clock_hz;
    uint8_t sda_gpio;
    uint8_t scl_gpio;
    rad_i2c_controller_ops_t ops;
    rad_device_ops_t device_ops;
    int used;
};

struct I2cDriver {
    rad_i2c_driver_t spec;
    char name[RAD_DRIVER_NAME_MAX];
    char compatible[RAD_COMPATIBLE_MAX];
    int used;
};

struct SpiController {
    uint32_t bus_id;
    char name[RAD_SPI_BUS_NAME_MAX];
    char device_name[32];
    char tree_path[RAD_TREE_MAX_PATH];
    uint32_t clock_hz;
    uint8_t sck_gpio;
    uint8_t tx_gpio;
    uint8_t rx_gpio;
    uint8_t cs_gpio;
    rad_spi_controller_ops_t ops;
    rad_device_ops_t device_ops;
    int used;
};

struct SpiDriver {
    rad_spi_driver_t spec;
    char name[RAD_DRIVER_NAME_MAX];
    char compatible[RAD_COMPATIBLE_MAX];
    int used;
};

struct DmaController {
    char name[RAD_DRIVER_NAME_MAX];
    uint32_t channel_count;
    rad_dma_backend_ops_t ops;
    int used;
};

struct FramebufferRecord {
    char name[RAD_FRAMEBUFFER_NAME_MAX];
    rad_framebuffer_info_t info;
    rad_display_output_type_t output_type;
    char connector[RAD_DISPLAY_CONNECTOR_MAX];
    uint32_t mode_count;
    uint32_t preferred_mode;
    uint32_t current_mode;
    rad_display_mode_t modes[RAD_FRAMEBUFFER_MAX_MODES];
    rad_framebuffer_ops_t ops;
    rad_device_ops_t device_ops;
    rad_framebuffer_handle handle;
    int used;
    int primary;
    int open_count;
};

struct ServiceRecord {
    rad_service_descriptor_t descriptor;
    char name[RAD_SERVICE_NAME_MAX];
    char compatible[RAD_SERVICE_COMPATIBLE_MAX];
    char capability[RAD_SERVICE_CAPABILITY_MAX];
    char tree_path[RAD_TREE_MAX_PATH];
    char backend[RAD_TREE_MAX_VALUE];
    char display[RAD_TREE_MAX_VALUE];
    char keyboard[RAD_TREE_MAX_VALUE];
    char pointer[RAD_TREE_MAX_VALUE];
    char terminal[RAD_TREE_MAX_VALUE];
    rad_service_state_t state;
    int autostart;
    int order;
    int32_t last_status;
    int used;
};

struct OverlayState {
    TreeNode nodes[RADIX_KERNEL_MAX_TREE_NODES];
    TreeProperty props[RADIX_KERNEL_MAX_TREE_PROPS];
    OverlayRecord overlays[RADIX_KERNEL_MAX_OVERLAYS];
    I2cController i2c[RADIX_KERNEL_MAX_I2C_CONTROLLERS];
    I2cDriver i2c_drivers[RADIX_KERNEL_MAX_BUS_DRIVERS];
    rad_i2c_device i2c_devices[RADIX_KERNEL_MAX_I2C_DEVICES];
    SpiController spi[RADIX_KERNEL_MAX_SPI_CONTROLLERS];
    SpiDriver spi_drivers[RADIX_KERNEL_MAX_BUS_DRIVERS];
    rad_spi_device spi_devices[RADIX_KERNEL_MAX_SPI_DEVICES];
    DmaController dma[RADIX_KERNEL_MAX_DMA_CONTROLLERS];
    rad_dma_channel_handle dma_channels[RADIX_KERNEL_MAX_DMA_CHANNELS];
    FramebufferRecord framebuffers[RADIX_KERNEL_MAX_FRAMEBUFFERS];
    rad_irq_domain_handle irq_domains[RADIX_KERNEL_MAX_IRQ_DOMAINS];
    ServiceRecord services[RADIX_KERNEL_MAX_SERVICES];
};

OverlayState g_overlay{};

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    const size_t src_len = strlen(src);
    const size_t copy_len = src_len < dst_size - 1 ? src_len : dst_size - 1;
    if (copy_len > 0) memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size) {
    crc = ~crc;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

const char *string_at(const char *strings, uint32_t string_size, uint32_t offset) {
    if (!strings || offset >= string_size) return nullptr;
    const char *value = strings + offset;
    const void *end = memchr(value, '\0', string_size - offset);
    return end ? value : nullptr;
}

TreeNode *find_node_mut(const char *path) {
    if (!path) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (g_overlay.nodes[i].used && strcmp(g_overlay.nodes[i].path, path) == 0) return &g_overlay.nodes[i];
    }
    return nullptr;
}

TreeNode *allocate_node(const char *path, const char *name, const char *parent) {
    TreeNode *existing = find_node_mut(path);
    if (existing) {
        if (name && *name) copy_string(existing->name, sizeof(existing->name), name);
        if (parent && *parent) copy_string(existing->parent, sizeof(existing->parent), parent);
        return existing;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (g_overlay.nodes[i].used) continue;
        memset(&g_overlay.nodes[i], 0, sizeof(g_overlay.nodes[i]));
        g_overlay.nodes[i].used = 1;
        copy_string(g_overlay.nodes[i].path, sizeof(g_overlay.nodes[i].path), path);
        copy_string(g_overlay.nodes[i].name, sizeof(g_overlay.nodes[i].name), name);
        copy_string(g_overlay.nodes[i].parent, sizeof(g_overlay.nodes[i].parent), parent);
        return &g_overlay.nodes[i];
    }
    return nullptr;
}

TreeProperty *find_property_mut(TreeNode *node, const char *name) {
    if (!node || !name) return nullptr;
    for (uint32_t i = 0; i < node->property_count && i < 16u; ++i) {
        const uint16_t index = node->properties[i];
        if (index < RADIX_KERNEL_MAX_TREE_PROPS && g_overlay.props[index].used && strcmp(g_overlay.props[index].name, name) == 0) {
            return &g_overlay.props[index];
        }
    }
    return nullptr;
}

TreeProperty *allocate_property(TreeNode *node, const char *name) {
    TreeProperty *existing = find_property_mut(node, name);
    if (existing) return existing;
    if (!node || node->property_count >= 16u) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_PROPS; ++i) {
        if (g_overlay.props[i].used) continue;
        memset(&g_overlay.props[i], 0, sizeof(g_overlay.props[i]));
        g_overlay.props[i].used = 1;
        copy_string(g_overlay.props[i].name, sizeof(g_overlay.props[i].name), name);
        node->properties[node->property_count++] = static_cast<uint16_t>(i);
        return &g_overlay.props[i];
    }
    return nullptr;
}

void path_parent_name(const char *path, char *parent, size_t parent_size, char *name, size_t name_size) {
    if (parent && parent_size) parent[0] = '\0';
    if (name && name_size) name[0] = '\0';
    if (!path || !*path || strcmp(path, "/") == 0) {
        if (parent && parent_size) copy_string(parent, parent_size, "");
        if (name && name_size) copy_string(name, name_size, "");
        return;
    }
    const char *last = strrchr(path, '/');
    if (!last) {
        if (parent && parent_size) copy_string(parent, parent_size, "");
        if (name && name_size) copy_string(name, name_size, path);
        return;
    }
    if (name && name_size) copy_string(name, name_size, last + 1);
    if (!parent || parent_size == 0) return;
    if (last == path) {
        copy_string(parent, parent_size, "/");
        return;
    }
    const size_t len = static_cast<size_t>(last - path);
    const size_t copy_len = len < parent_size - 1 ? len : parent_size - 1;
    memcpy(parent, path, copy_len);
    parent[copy_len] = '\0';
}

TreeNode *ensure_node_path(const char *path) {
    if (!path || !*path || path[0] != '/') return nullptr;
    TreeNode *existing = find_node_mut(path);
    if (existing) return existing;
    char parent[RAD_TREE_MAX_PATH];
    char name[RAD_TREE_MAX_PROPERTY_NAME];
    path_parent_name(path, parent, sizeof(parent), name, sizeof(name));
    if (parent[0] && strcmp(parent, path) != 0 && !find_node_mut(parent)) {
        if (!ensure_node_path(parent)) return nullptr;
    }
    return allocate_node(path, name, parent);
}

void set_property_bool(TreeNode *node, const char *name, int value) {
    TreeProperty *prop = allocate_property(node, name);
    if (!prop) return;
    prop->type = RAD_TREE_PROP_BOOL;
    prop->bool_value = value ? 1 : 0;
    prop->u32_value = 0;
    prop->array_count = 0;
    prop->string_value[0] = '\0';
}

void set_property_u32(TreeNode *node, const char *name, uint32_t value) {
    TreeProperty *prop = allocate_property(node, name);
    if (!prop) return;
    prop->type = RAD_TREE_PROP_U32;
    prop->u32_value = value;
    prop->bool_value = 0;
    prop->array_count = 0;
    prop->string_value[0] = '\0';
}

void set_property_string(TreeNode *node, const char *name, const char *value) {
    TreeProperty *prop = allocate_property(node, name);
    if (!prop) return;
    prop->type = RAD_TREE_PROP_STRING;
    prop->u32_value = 0;
    prop->bool_value = 0;
    prop->array_count = 0;
    copy_string(prop->string_value, sizeof(prop->string_value), value);
    if (strcmp(name, "module") == 0) copy_string(node->module, sizeof(node->module), value);
}

rad_status_t apply_property(TreeNode *node, const OverlayPropertyRecord& record, const char *strings, uint32_t string_size, const uint8_t *data, uint32_t data_size) {
    const char *name = string_at(strings, string_size, record.name_offset);
    if (!node || !name) return RAD_STATUS_INVALID_ARGUMENT;
    TreeProperty *prop = allocate_property(node, name);
    if (!prop) return RAD_STATUS_NO_MEMORY;
    prop->type = static_cast<rad_tree_property_type_t>(record.type);
    prop->u32_value = 0;
    prop->bool_value = 0;
    prop->string_value[0] = '\0';
    prop->array_count = 0;
    switch (record.type) {
    case RAD_TREE_PROP_BOOL:
        prop->bool_value = record.value ? 1 : 0;
        break;
    case RAD_TREE_PROP_U32:
        prop->u32_value = record.value;
        break;
    case RAD_TREE_PROP_STRING: {
        const char *value = string_at(strings, string_size, record.value);
        if (!value) return RAD_STATUS_INVALID_ARGUMENT;
        copy_string(prop->string_value, sizeof(prop->string_value), value);
        if (strcmp(name, "module") == 0) copy_string(node->module, sizeof(node->module), value);
        break;
    }
    case RAD_TREE_PROP_U32_ARRAY: {
        if (record.value > data_size || record.size > data_size - record.value || (record.size % 4u) != 0) return RAD_STATUS_INVALID_ARGUMENT;
        const uint32_t count = record.size / 4u;
        prop->array_count = count > 8u ? 8u : count;
        memcpy(prop->array_values, data + record.value, prop->array_count * sizeof(uint32_t));
        break;
    }
    case RAD_TREE_PROP_STRING_ARRAY:
        if (record.value > data_size || record.size > data_size - record.value) return RAD_STATUS_INVALID_ARGUMENT;
        copy_string(prop->string_value, sizeof(prop->string_value), reinterpret_cast<const char*>(data + record.value));
        break;
    default:
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

TreeProperty *find_property(rad_tree_node_t node, const char *name) {
    return find_property_mut(reinterpret_cast<TreeNode*>(node), name);
}

OverlayRecord *allocate_overlay_record() {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_OVERLAYS; ++i) {
        if (g_overlay.overlays[i].used) continue;
        memset(&g_overlay.overlays[i], 0, sizeof(g_overlay.overlays[i]));
        g_overlay.overlays[i].used = 1;
        return &g_overlay.overlays[i];
    }
    return nullptr;
}

int node_is_under_i2c(const TreeNode& node, uint32_t *bus_id) {
    TreeNode *parent = find_node_mut(node.parent);
    if (!parent) return 0;
    if (!strstr(parent->path, "/i2c@")) return 0;
    TreeProperty *bus = find_property_mut(parent, "bus-id");
    if (bus && bus->type == RAD_TREE_PROP_U32) {
        if (bus_id) *bus_id = bus->u32_value;
        return 1;
    }
    return 1;
}

int node_is_under_spi(const TreeNode& node, uint32_t *bus_id) {
    TreeNode *parent = find_node_mut(node.parent);
    if (!parent) return 0;
    if (!strstr(parent->path, "/spi@")) return 0;
    TreeProperty *bus = find_property_mut(parent, "bus-id");
    if (bus && bus->type == RAD_TREE_PROP_U32) {
        if (bus_id) *bus_id = bus->u32_value;
        return 1;
    }
    return 1;
}

I2cController *find_i2c(uint32_t bus_id) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_CONTROLLERS; ++i) {
        if (g_overlay.i2c[i].used && g_overlay.i2c[i].bus_id == bus_id) return &g_overlay.i2c[i];
    }
    return nullptr;
}

SpiController *find_spi(uint32_t bus_id) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_CONTROLLERS; ++i) {
        if (g_overlay.spi[i].used && g_overlay.spi[i].bus_id == bus_id) return &g_overlay.spi[i];
    }
    return nullptr;
}

rad_irq_domain_handle *find_irq_domain_by_path(const char *tree_path) {
    if (!tree_path) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQ_DOMAINS; ++i) {
        if (g_overlay.irq_domains[i].used && strcmp(g_overlay.irq_domains[i].tree_path, tree_path) == 0) {
            return &g_overlay.irq_domains[i];
        }
    }
    return nullptr;
}

rad_irq_trigger_t irq_trigger_from_flags(uint32_t flags) {
    switch (flags) {
    case 1u: return RAD_IRQ_TRIGGER_EDGE_RISING;
    case 2u: return RAD_IRQ_TRIGGER_EDGE_FALLING;
    case 4u: return RAD_IRQ_TRIGGER_LEVEL_HIGH;
    case 8u: return RAD_IRQ_TRIGGER_LEVEL_LOW;
    default: return RAD_IRQ_TRIGGER_NONE;
    }
}

const char *irq_trigger_name(rad_irq_trigger_t trigger) {
    switch (trigger) {
    case RAD_IRQ_TRIGGER_EDGE_RISING: return "edge-rising";
    case RAD_IRQ_TRIGGER_EDGE_FALLING: return "edge-falling";
    case RAD_IRQ_TRIGGER_LEVEL_HIGH: return "level-high";
    case RAD_IRQ_TRIGGER_LEVEL_LOW: return "level-low";
    default: return "none";
    }
}

rad_irq_domain_handle *allocate_irq_domain(const rad_irq_domain_config_t *config) {
    if (!config || !config->tree_path || !*config->tree_path) return nullptr;
    rad_irq_domain_handle *existing = find_irq_domain_by_path(config->tree_path);
    if (existing) return existing;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQ_DOMAINS; ++i) {
        rad_irq_domain_handle& domain = g_overlay.irq_domains[i];
        if (domain.used) continue;
        memset(&domain, 0, sizeof(domain));
        domain.used = 1;
        copy_string(domain.name, sizeof(domain.name), config->name ? config->name : "irq-domain");
        copy_string(domain.tree_path, sizeof(domain.tree_path), config->tree_path);
        domain.interrupt_base = config->interrupt_base;
        domain.line_count = config->line_count;
        domain.interrupt_cells = config->interrupt_cells ? config->interrupt_cells : 2u;
        return &domain;
    }
    return nullptr;
}

rad_status_t bind_irq_domain_node(TreeNode *node) {
    if (!node) return RAD_STATUS_INVALID_ARGUMENT;
    TreeProperty *controller = find_property_mut(node, "interrupt-controller");
    if (!controller || controller->type != RAD_TREE_PROP_BOOL || !controller->bool_value) return RAD_STATUS_OK;
    TreeProperty *cells = find_property_mut(node, "#interrupt-cells");
    if (!cells || cells->type != RAD_TREE_PROP_U32 || cells->u32_value == 0 || cells->u32_value > 2u) return RAD_STATUS_INVALID_ARGUMENT;
    TreeProperty *base = find_property_mut(node, "interrupt-base");
    TreeProperty *count = find_property_mut(node, "interrupt-count");
    if (!count || count->type != RAD_TREE_PROP_U32) count = find_property_mut(node, "line-count");
    TreeProperty *name = find_property_mut(node, "name");
    rad_irq_domain_config_t config{};
    config.size = sizeof(config);
    config.name = name && name->type == RAD_TREE_PROP_STRING ? name->string_value : node->name;
    config.tree_path = node->path;
    config.interrupt_base = base && base->type == RAD_TREE_PROP_U32 ? base->u32_value : 0u;
    config.line_count = count && count->type == RAD_TREE_PROP_U32 ? count->u32_value : 32u;
    config.interrupt_cells = cells->u32_value;
    return allocate_irq_domain(&config) ? RAD_STATUS_OK : RAD_STATUS_NO_MEMORY;
}

rad_status_t bind_irq_domains_from_tree(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        const rad_status_t status = bind_irq_domain_node(&g_overlay.nodes[i]);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

TreeNode *find_interrupt_parent_node(TreeNode *node) {
    TreeNode *cursor = node;
    while (cursor) {
        TreeProperty *parent = find_property_mut(cursor, "interrupt-parent");
        if (parent && parent->type == RAD_TREE_PROP_STRING) return find_node_mut(parent->string_value);
        if (!cursor->parent[0]) break;
        cursor = find_node_mut(cursor->parent);
    }
    if (node) {
        for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQ_DOMAINS; ++i) {
            if (g_overlay.irq_domains[i].used) return find_node_mut(g_overlay.irq_domains[i].tree_path);
        }
    }
    return nullptr;
}

size_t resolve_irq_resources(TreeNode *node, rad_irq_resource_t *resources, size_t capacity) {
    if (!node) return 0;
    TreeProperty *interrupts = find_property_mut(node, "interrupts");
    if (!interrupts || interrupts->type != RAD_TREE_PROP_U32_ARRAY || interrupts->array_count == 0) return 0;
    TreeNode *parent = find_interrupt_parent_node(node);
    if (!parent) return 0;
    rad_irq_domain_handle *domain = find_irq_domain_by_path(parent->path);
    if (!domain) {
        const rad_status_t status = bind_irq_domain_node(parent);
        if (status != RAD_STATUS_OK) return 0;
        domain = find_irq_domain_by_path(parent->path);
    }
    if (!domain || domain->interrupt_cells == 0 || interrupts->array_count < domain->interrupt_cells) return 0;
    size_t count = 0;
    for (uint32_t i = 0; i + domain->interrupt_cells <= interrupts->array_count && count < capacity; i += domain->interrupt_cells) {
        const uint32_t line = interrupts->array_values[i];
        if (domain->line_count && line >= domain->line_count) continue;
        rad_irq_resource_t resource{};
        resource.line = line;
        resource.flags = domain->interrupt_cells > 1u ? interrupts->array_values[i + 1u] : 0u;
        resource.trigger = irq_trigger_from_flags(resource.flags);
        resource.irq = domain->interrupt_base + line;
        copy_string(resource.domain, sizeof(resource.domain), domain->name);
        copy_string(resource.controller_path, sizeof(resource.controller_path), domain->tree_path);
        if (resources) resources[count] = resource;
        ++count;
    }
    return count;
}

I2cDriver *find_i2c_driver_by_name(const char *name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.i2c_drivers[i].used && strcmp(g_overlay.i2c_drivers[i].name, name) == 0) return &g_overlay.i2c_drivers[i];
    }
    return nullptr;
}

I2cDriver *find_i2c_driver_by_compatible(const char *compatible, uint32_t *index) {
    if (!compatible) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.i2c_drivers[i].used && strcmp(g_overlay.i2c_drivers[i].compatible, compatible) == 0) {
            if (index) *index = static_cast<uint32_t>(i);
            return &g_overlay.i2c_drivers[i];
        }
    }
    return nullptr;
}

SpiDriver *find_spi_driver_by_name(const char *name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.spi_drivers[i].used && strcmp(g_overlay.spi_drivers[i].name, name) == 0) return &g_overlay.spi_drivers[i];
    }
    return nullptr;
}

SpiDriver *find_spi_driver_by_compatible(const char *compatible, uint32_t *index) {
    if (!compatible) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.spi_drivers[i].used && strcmp(g_overlay.spi_drivers[i].compatible, compatible) == 0) {
            if (index) *index = static_cast<uint32_t>(i);
            return &g_overlay.spi_drivers[i];
        }
    }
    return nullptr;
}

rad_i2c_device *find_i2c_device(uint32_t bus_id, uint8_t address) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_DEVICES; ++i) {
        if (g_overlay.i2c_devices[i].controller && g_overlay.i2c_devices[i].bus_id == bus_id && g_overlay.i2c_devices[i].address == address) {
            return &g_overlay.i2c_devices[i];
        }
    }
    return nullptr;
}

rad_i2c_device *find_i2c_device_by_path(const char *path) {
    if (!path) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_DEVICES; ++i) {
        if (g_overlay.i2c_devices[i].controller && strcmp(g_overlay.i2c_devices[i].path, path) == 0) return &g_overlay.i2c_devices[i];
    }
    return nullptr;
}

rad_spi_device *find_spi_device(uint32_t bus_id, uint8_t cs) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_DEVICES; ++i) {
        if (g_overlay.spi_devices[i].controller && g_overlay.spi_devices[i].bus_id == bus_id && g_overlay.spi_devices[i].cs == cs) {
            return &g_overlay.spi_devices[i];
        }
    }
    return nullptr;
}

rad_spi_device *find_spi_device_by_path(const char *path) {
    if (!path) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_DEVICES; ++i) {
        if (g_overlay.spi_devices[i].controller && strcmp(g_overlay.spi_devices[i].path, path) == 0) return &g_overlay.spi_devices[i];
    }
    return nullptr;
}

uint8_t parse_spi_transfer_mode(const TreeProperty *prop) {
    if (!prop || prop->type != RAD_TREE_PROP_STRING) return RAD_SPI_TRANSFER_MODE_AUTO;
    if (strcmp(prop->string_value, "pio") == 0) return RAD_SPI_TRANSFER_MODE_PIO;
    if (strcmp(prop->string_value, "dma") == 0) return RAD_SPI_TRANSFER_MODE_DMA;
    return RAD_SPI_TRANSFER_MODE_AUTO;
}

int dma_backend_available(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++i) {
        if (g_overlay.dma[i].used && g_overlay.dma[i].channel_count > 0) return 1;
    }
    return 0;
}

FramebufferRecord *find_framebuffer(const char *name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        if (g_overlay.framebuffers[i].used && strcmp(g_overlay.framebuffers[i].name, name) == 0) return &g_overlay.framebuffers[i];
    }
    return nullptr;
}

FramebufferRecord *find_primary_framebuffer(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        if (g_overlay.framebuffers[i].used && g_overlay.framebuffers[i].primary) return &g_overlay.framebuffers[i];
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        if (g_overlay.framebuffers[i].used) return &g_overlay.framebuffers[i];
    }
    return nullptr;
}

FramebufferRecord *framebuffer_from_handle(rad_framebuffer_t framebuffer) {
    if (!framebuffer || framebuffer->index >= RADIX_KERNEL_MAX_FRAMEBUFFERS) return nullptr;
    FramebufferRecord *record = &g_overlay.framebuffers[framebuffer->index];
    return record->used ? record : nullptr;
}

void populate_framebuffer_display_info(const FramebufferRecord& record, rad_framebuffer_display_info_t *info) {
    if (!info) return;
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    copy_string(info->name, sizeof(info->name), record.name);
    info->framebuffer = record.info;
    info->output_type = record.output_type;
    copy_string(info->connector, sizeof(info->connector), record.connector);
    info->mode_count = record.mode_count;
    info->preferred_mode = record.preferred_mode;
    info->current_mode = record.current_mode;
    info->primary = record.primary;
    const uint32_t count = record.mode_count < RAD_FRAMEBUFFER_MAX_MODES ? record.mode_count : RAD_FRAMEBUFFER_MAX_MODES;
    for (uint32_t i = 0; i < count; ++i) info->modes[i] = record.modes[i];
}

const char *display_output_type_name(rad_display_output_type_t type) {
    switch (type) {
        case RAD_DISPLAY_OUTPUT_MEMORY: return "memory";
        case RAD_DISPLAY_OUTPUT_CIRCLE: return "circle";
        case RAD_DISPLAY_OUTPUT_GRUB: return "grub";
        case RAD_DISPLAY_OUTPUT_RP2350_HSTX: return "rp2350-hstx";
        case RAD_DISPLAY_OUTPUT_SPI_PANEL: return "spi-panel";
        default: return "unknown";
    }
}

const char *service_state_name(rad_service_state_t state) {
    switch (state) {
    case RAD_SERVICE_REGISTERED: return "registered";
    case RAD_SERVICE_CONFIGURED: return "configured";
    case RAD_SERVICE_RUNNING: return "running";
    case RAD_SERVICE_FAILED: return "failed";
    case RAD_SERVICE_STOPPED: return "stopped";
    default: return "unknown";
    }
}

ServiceRecord *find_service_by_name(const char *name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        if (g_overlay.services[i].used && strcmp(g_overlay.services[i].name, name) == 0) return &g_overlay.services[i];
    }
    return nullptr;
}

ServiceRecord *find_service_by_compatible(const char *compatible) {
    if (!compatible) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        if (g_overlay.services[i].used && strcmp(g_overlay.services[i].compatible, compatible) == 0) return &g_overlay.services[i];
    }
    return nullptr;
}

rad_service_config_t service_config_for_record(ServiceRecord& service) {
    rad_service_config_t config{};
    config.size = sizeof(config);
    config.tree_path = service.tree_path;
    config.backend = service.backend;
    config.display = service.display;
    config.keyboard = service.keyboard;
    config.pointer = service.pointer;
    config.terminal = service.terminal;
    config.autostart = service.autostart;
    config.order = service.order;
    return config;
}

int tree_status_is_disabled(TreeNode *node) {
    TreeProperty *status = find_property_mut(node, "status");
    return status && status->type == RAD_TREE_PROP_STRING && strcmp(status->string_value, "disabled") == 0;
}

void copy_optional_service_string(TreeNode *node, const char *property, char *destination, size_t destination_size) {
    TreeProperty *value = find_property_mut(node, property);
    if (value && (value->type == RAD_TREE_PROP_STRING || value->type == RAD_TREE_PROP_STRING_ARRAY)) {
        copy_string(destination, destination_size, value->string_value);
    }
}

rad_status_t configure_service_node(TreeNode *node) {
    if (!node || strncmp(node->path, "/services/", 10) != 0) return RAD_STATUS_OK;
    TreeProperty *compatible = find_property_mut(node, "compatible");
    if (!compatible || compatible->type != RAD_TREE_PROP_STRING) return RAD_STATUS_OK;
    ServiceRecord *service = find_service_by_compatible(compatible->string_value);
    if (!service) return RAD_STATUS_OK;
    copy_string(service->tree_path, sizeof(service->tree_path), node->path);
    service->autostart = 0;
    service->order = service->descriptor.default_order;
    copy_optional_service_string(node, "backend", service->backend, sizeof(service->backend));
    copy_optional_service_string(node, "display", service->display, sizeof(service->display));
    copy_optional_service_string(node, "keyboard", service->keyboard, sizeof(service->keyboard));
    copy_optional_service_string(node, "pointer", service->pointer, sizeof(service->pointer));
    copy_optional_service_string(node, "terminal", service->terminal, sizeof(service->terminal));
    TreeProperty *autostart = find_property_mut(node, "autostart");
    if (autostart && autostart->type == RAD_TREE_PROP_BOOL) service->autostart = autostart->bool_value ? 1 : 0;
    TreeProperty *order = find_property_mut(node, "order");
    if (order && order->type == RAD_TREE_PROP_U32) service->order = static_cast<int>(order->u32_value);
    if (tree_status_is_disabled(node)) {
        service->state = RAD_SERVICE_STOPPED;
        node->bound = 0;
        return RAD_STATUS_OK;
    }
    if (service->state == RAD_SERVICE_RUNNING) {
        node->bound = 1;
        return RAD_STATUS_OK;
    }
    if (service->state != RAD_SERVICE_RUNNING) service->state = RAD_SERVICE_CONFIGURED;
    node->bound = 1;
    if (service->autostart) {
        rad_service_config_t config = service_config_for_record(*service);
        const rad_status_t status = service->descriptor.start
            ? service->descriptor.start(service->descriptor.context, &config)
            : RAD_STATUS_OK;
        service->last_status = status;
        service->state = status == RAD_STATUS_OK ? RAD_SERVICE_RUNNING : RAD_SERVICE_FAILED;
        return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t bind_service_tree(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        const rad_status_t status = configure_service_node(&g_overlay.nodes[i]);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

uint16_t rgb565_from_xrgb(uint32_t color) {
    const uint32_t r = (color >> 16) & 0xffu;
    const uint32_t g = (color >> 8) & 0xffu;
    const uint32_t b = color & 0xffu;
    return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void framebuffer_put_pixel(FramebufferRecord *record, uint32_t x, uint32_t y, uint32_t color) {
    if (!record || !record->info.pixels || x >= record->info.width || y >= record->info.height) return;
    auto *row = static_cast<uint8_t*>(record->info.pixels) + static_cast<size_t>(y) * record->info.stride_bytes;
    if (record->info.pixel_format == RAD_PIXEL_FORMAT_RGB565) {
        auto *pixel = reinterpret_cast<uint16_t*>(row + static_cast<size_t>(x) * 2u);
        *pixel = rgb565_from_xrgb(color);
    } else if (record->info.pixel_format == RAD_PIXEL_FORMAT_XRGB8888) {
        auto *pixel = reinterpret_cast<uint32_t*>(row + static_cast<size_t>(x) * 4u);
        *pixel = color | 0xff000000u;
    }
}

void framebuffer_fill_rect(FramebufferRecord *record, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!record) return;
    const uint32_t max_x = x + width > record->info.width ? record->info.width : x + width;
    const uint32_t max_y = y + height > record->info.height ? record->info.height : y + height;
    for (uint32_t py = y; py < max_y; ++py) {
        for (uint32_t px = x; px < max_x; ++px) framebuffer_put_pixel(record, px, py, color);
    }
}

uint8_t glyph_row(char ch, uint32_t row) {
    if (ch == ' ') return 0u;
    uint8_t seed = static_cast<uint8_t>(ch);
    seed ^= static_cast<uint8_t>((row + 1u) * 0x2du);
    uint8_t bits = static_cast<uint8_t>(((seed << 1) | (seed >> 5)) & 0x3eu);
    if (row == 0 || row == 6) bits |= 0x1eu;
    return bits;
}

void framebuffer_draw_cell(FramebufferRecord *record, uint32_t cell_x, uint32_t cell_y, char ch, uint32_t foreground, uint32_t background) {
    const uint32_t origin_x = cell_x * 8u;
    const uint32_t origin_y = cell_y * 8u;
    framebuffer_fill_rect(record, origin_x, origin_y, 8u, 8u, background);
    if (ch == ' ') return;
    for (uint32_t row = 0; row < 7u; ++row) {
        const uint8_t bits = glyph_row(ch, row);
        for (uint32_t col = 0; col < 6u; ++col) {
            if (bits & (1u << (5u - col))) {
                framebuffer_put_pixel(record, origin_x + 1u + col, origin_y + row, foreground);
            }
        }
    }
}

rad_status_t i2c_device_ioctl(void *context, uint32_t request, void *argument) {
    auto *controller = static_cast<I2cController*>(context);
    if (!controller || request != RAD_DEVICE_IOCTL_I2C_TRANSFER) return RAD_STATUS_NOT_SUPPORTED;
    if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
    if (!controller->ops.transfer) return RAD_STATUS_NOT_SUPPORTED;
    return controller->ops.transfer(controller->ops.context, static_cast<const rad_i2c_transfer_t*>(argument));
}

rad_status_t spi_device_ioctl(void *context, uint32_t request, void *argument) {
    auto *controller = static_cast<SpiController*>(context);
    if (!controller || request != RAD_DEVICE_IOCTL_SPI_TRANSFER) return RAD_STATUS_NOT_SUPPORTED;
    if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
    rad_spi_device device{};
    device.bus_id = controller->bus_id;
    device.cs = 0;
    device.clock_hz = controller->clock_hz;
    device.bits_per_word = 8;
    device.transfer_mode = RAD_SPI_TRANSFER_MODE_AUTO;
    device.controller = controller;
    return rad_spi_device_transfer(&device, static_cast<const rad_spi_transfer_t*>(argument));
}

rad_status_t framebuffer_device_ioctl(void *context, uint32_t request, void *argument) {
    auto *record = static_cast<FramebufferRecord*>(context);
    if (!record || !record->used) return RAD_STATUS_NOT_FOUND;
    if (request == RAD_DEVICE_IOCTL_FRAMEBUFFER_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_framebuffer_info_t*>(argument) = record->info;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_FRAMEBUFFER_FLUSH) {
        const auto *rect = static_cast<const rad_framebuffer_rect_t*>(argument);
        if (record->ops.flush) return record->ops.flush(record->ops.context, rect);
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_FRAMEBUFFER_PRESENT) {
        const auto *present = static_cast<const rad_framebuffer_present_t*>(argument);
        if (record->ops.present) return record->ops.present(record->ops.context, present);
        if (record->ops.flush && present) return record->ops.flush(record->ops.context, &present->rect);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

void write_line(rad_terminal_write_t write, void *context, const char *line) {
    if (!write) return;
    write(line ? line : "", context);
    write("\n", context);
}

rad_status_t bind_spi_tree(void);
rad_status_t bind_framebuffer_tree(void);
rad_status_t bind_service_tree(void);

rad_status_t cmd_overlays(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[192];
    for (size_t i = 0; i < RADIX_KERNEL_MAX_OVERLAYS; ++i) {
        if (!g_overlay.overlays[i].used) continue;
        snprintf(line, sizeof(line), "%s source=%s nodes=%u props=%u",
            g_overlay.overlays[i].name,
            g_overlay.overlays[i].source,
            static_cast<unsigned>(g_overlay.overlays[i].node_count),
            static_cast<unsigned>(g_overlay.overlays[i].property_count));
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_tree(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[192];
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        snprintf(line, sizeof(line), "%s module=%s bound=%d",
            g_overlay.nodes[i].path,
            g_overlay.nodes[i].module,
            g_overlay.nodes[i].bound);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_bind(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    const rad_status_t irq_status = bind_irq_domains_from_tree();
    if (irq_status != RAD_STATUS_OK) return irq_status;
    const rad_status_t status = rad_i2c_bind_tree();
    if (status != RAD_STATUS_OK) return status;
    const rad_status_t spi_status = bind_spi_tree();
    if (spi_status != RAD_STATUS_OK) return spi_status;
    const rad_status_t fb_status = bind_framebuffer_tree();
    if (fb_status != RAD_STATUS_OK) return fb_status;
    const rad_status_t service_status = bind_service_tree();
    if (service_status != RAD_STATUS_OK) return service_status;
    write_line(write, write_context, "bindings refreshed");
    return RAD_STATUS_OK;
}

rad_status_t cmd_drivers(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_driver_info_t drivers[48]{};
    const size_t count = rad_driver_list(drivers, sizeof(drivers) / sizeof(drivers[0]));
    char line[192];
    for (size_t i = 0; i < count && i < sizeof(drivers) / sizeof(drivers[0]); ++i) {
        snprintf(line, sizeof(line), "%.31s bus=%.31s role=%.31s compatible=%.63s",
            drivers[i].name,
            drivers[i].bus,
            drivers[i].role,
            drivers[i].compatible);
        write_line(write, write_context, line);
    }
    return bind_irq_domains_from_tree();
}

rad_status_t cmd_i2c(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[192];
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_CONTROLLERS; ++i) {
        if (!g_overlay.i2c[i].used) continue;
        snprintf(line, sizeof(line), "bus%u name=%s clock=%u device=%s",
            static_cast<unsigned>(g_overlay.i2c[i].bus_id),
            g_overlay.i2c[i].name,
            static_cast<unsigned>(g_overlay.i2c[i].clock_hz),
            g_overlay.i2c[i].device_name);
        write_line(write, write_context, line);
    }
    rad_i2c_device_info_t devices[32]{};
    const size_t count = rad_i2c_list_devices(devices, sizeof(devices) / sizeof(devices[0]));
    for (size_t i = 0; i < count && i < sizeof(devices) / sizeof(devices[0]); ++i) {
        snprintf(line, sizeof(line), "  %s bus%u addr=0x%02x irq_count=%u compatible=%s driver=%s bound=%d",
            devices[i].path,
            static_cast<unsigned>(devices[i].bus_id),
            static_cast<unsigned>(devices[i].address),
            static_cast<unsigned>(devices[i].irq_count),
            devices[i].compatible,
            devices[i].driver,
            devices[i].bound);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_spi(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[224];
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_CONTROLLERS; ++i) {
        if (!g_overlay.spi[i].used) continue;
        snprintf(line, sizeof(line), "bus%u name=%s clock=%u device=%s",
            static_cast<unsigned>(g_overlay.spi[i].bus_id),
            g_overlay.spi[i].name,
            static_cast<unsigned>(g_overlay.spi[i].clock_hz),
            g_overlay.spi[i].device_name);
        write_line(write, write_context, line);
    }
    rad_spi_device_info_t devices[32]{};
    const size_t count = rad_spi_list_devices(devices, sizeof(devices) / sizeof(devices[0]));
    for (size_t i = 0; i < count && i < sizeof(devices) / sizeof(devices[0]); ++i) {
        snprintf(line, sizeof(line), "  %s bus%u cs=%u clock=%u mode=%u bpw=%u transfer=%u irq_count=%u compatible=%s driver=%s bound=%d",
            devices[i].path,
            static_cast<unsigned>(devices[i].bus_id),
            static_cast<unsigned>(devices[i].cs),
            static_cast<unsigned>(devices[i].clock_hz),
            static_cast<unsigned>(devices[i].mode),
            static_cast<unsigned>(devices[i].bits_per_word),
            static_cast<unsigned>(devices[i].transfer_mode),
            static_cast<unsigned>(devices[i].irq_count),
            devices[i].compatible,
            devices[i].driver,
            devices[i].bound);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_dma(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_dma_channel_info_t channels[32]{};
    const size_t count = rad_dma_list_channels(channels, sizeof(channels) / sizeof(channels[0]));
    char line[160];
    for (size_t i = 0; i < count && i < sizeof(channels) / sizeof(channels[0]); ++i) {
        snprintf(line, sizeof(line), "%.31s channel%u request=%u allocated=%d active=%d",
            channels[i].controller,
            static_cast<unsigned>(channels[i].index),
            static_cast<unsigned>(channels[i].request_id),
            channels[i].allocated,
            channels[i].active);
        write_line(write, write_context, line);
    }
    if (count == 0) write_line(write, write_context, "no dma channels");
    return RAD_STATUS_OK;
}

rad_status_t cmd_irq_tree(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[224];
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQ_DOMAINS; ++i) {
        const rad_irq_domain_handle& domain = g_overlay.irq_domains[i];
        if (!domain.used) continue;
        snprintf(line, sizeof(line), "%s path=%s base=%u lines=%u cells=%u",
            domain.name,
            domain.tree_path,
            static_cast<unsigned>(domain.interrupt_base),
            static_cast<unsigned>(domain.line_count),
            static_cast<unsigned>(domain.interrupt_cells));
        write_line(write, write_context, line);
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        rad_irq_resource_t resources[RAD_IRQ_MAX_RESOURCES]{};
        const size_t count = resolve_irq_resources(&g_overlay.nodes[i], resources, RAD_IRQ_MAX_RESOURCES);
        for (size_t r = 0; r < count; ++r) {
            snprintf(line, sizeof(line), "  %s irq=%u line=%u trigger=%s controller=%s",
                g_overlay.nodes[i].path,
                static_cast<unsigned>(resources[r].irq),
                static_cast<unsigned>(resources[r].line),
                irq_trigger_name(resources[r].trigger),
                resources[r].controller_path);
            write_line(write, write_context, line);
        }
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_fb(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[192];
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        const FramebufferRecord& fb = g_overlay.framebuffers[i];
        if (!fb.used) continue;
        snprintf(line, sizeof(line), "%s %ux%u@%u stride=%u format=%u output=%s connector=%s primary=%d pixels=%p",
            fb.name,
            static_cast<unsigned>(fb.info.width),
            static_cast<unsigned>(fb.info.height),
            static_cast<unsigned>(fb.mode_count > fb.current_mode ? fb.modes[fb.current_mode].refresh_hz : 0u),
            static_cast<unsigned>(fb.info.stride_bytes),
            static_cast<unsigned>(fb.info.pixel_format),
            display_output_type_name(fb.output_type),
            fb.connector,
            fb.primary,
            fb.info.pixels);
        write_line(write, write_context, line);
        ++count;
    }
    if (count == 0) write_line(write, write_context, "no framebuffers");
    return RAD_STATUS_OK;
}

rad_status_t cmd_services(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[320];
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        const ServiceRecord& service = g_overlay.services[i];
        if (!service.used) continue;
        snprintf(line, sizeof(line), "%s compatible=%s capability=%s state=%s autostart=%d order=%d backend=%s display=%s keyboard=%s pointer=%s terminal=%s path=%s status=%d",
            service.name,
            service.compatible,
            service.capability,
            service_state_name(service.state),
            service.autostart,
            service.order,
            service.backend,
            service.display,
            service.keyboard,
            service.pointer,
            service.terminal,
            service.tree_path,
            static_cast<int>(service.last_status));
        write_line(write, write_context, line);
        ++count;
    }
    if (count == 0) write_line(write, write_context, "no services");
    return RAD_STATUS_OK;
}

rad_status_t bind_spi_tree(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        uint32_t bus_id = 0;
        if (!node_is_under_spi(g_overlay.nodes[i], &bus_id)) continue;
        TreeProperty *cs_prop = find_property_mut(&g_overlay.nodes[i], "cs");
        if (!cs_prop || cs_prop->type != RAD_TREE_PROP_U32) cs_prop = find_property_mut(&g_overlay.nodes[i], "reg");
        if (!cs_prop || cs_prop->type != RAD_TREE_PROP_U32) continue;
        SpiController *controller = find_spi(bus_id);
        if (!controller) continue;
        TreeProperty *compatible = find_property_mut(&g_overlay.nodes[i], "compatible");
        if (!compatible || compatible->type != RAD_TREE_PROP_STRING) continue;
        rad_spi_device *device = find_spi_device_by_path(g_overlay.nodes[i].path);
        if (!device) {
            for (size_t d = 0; d < RADIX_KERNEL_MAX_SPI_DEVICES; ++d) {
                if (g_overlay.spi_devices[d].controller) continue;
                device = &g_overlay.spi_devices[d];
                memset(device, 0, sizeof(*device));
                break;
            }
        }
        if (!device) return RAD_STATUS_NO_MEMORY;
        if (device->bound) {
            g_overlay.nodes[i].bound = 1;
            continue;
        }
        device->bus_id = bus_id;
        device->cs = static_cast<uint8_t>(cs_prop->u32_value & 0xffu);
        device->controller = controller;
        device->clock_hz = controller->clock_hz;
        device->bits_per_word = 8u;
        device->transfer_mode = RAD_SPI_TRANSFER_MODE_AUTO;
        device->irq_count = static_cast<uint32_t>(resolve_irq_resources(&g_overlay.nodes[i], device->irqs, RAD_IRQ_MAX_RESOURCES));
        TreeProperty *clock = find_property_mut(&g_overlay.nodes[i], "clock-frequency");
        if (clock && clock->type == RAD_TREE_PROP_U32) device->clock_hz = clock->u32_value;
        TreeProperty *mode = find_property_mut(&g_overlay.nodes[i], "mode");
        if (mode && mode->type == RAD_TREE_PROP_U32) device->mode = static_cast<uint8_t>(mode->u32_value & 0xffu);
        TreeProperty *bpw = find_property_mut(&g_overlay.nodes[i], "bits-per-word");
        if (bpw && bpw->type == RAD_TREE_PROP_U32) device->bits_per_word = static_cast<uint8_t>(bpw->u32_value & 0xffu);
        device->transfer_mode = parse_spi_transfer_mode(find_property_mut(&g_overlay.nodes[i], "transfer-mode"));
        copy_string(device->path, sizeof(device->path), g_overlay.nodes[i].path);
        copy_string(device->compatible, sizeof(device->compatible), compatible->string_value);
        uint32_t driver_index = 0;
        SpiDriver *driver = find_spi_driver_by_compatible(device->compatible, &driver_index);
        if (!driver) {
            device->bound = 0;
            device->driver[0] = '\0';
            g_overlay.nodes[i].bound = 0;
            continue;
        }
        device->driver_index = driver_index;
        copy_string(device->driver, sizeof(device->driver), driver->name);
        rad_status_t status = driver->spec.probe
            ? driver->spec.probe(driver->spec.context, device)
            : RAD_STATUS_OK;
        if (status != RAD_STATUS_OK) return status;
        device->bound = 1;
        g_overlay.nodes[i].bound = 1;
    }
    return RAD_STATUS_OK;
}

rad_status_t bind_framebuffer_tree(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        TreeNode& node = g_overlay.nodes[i];
        TreeProperty *fb_prop = find_property_mut(&node, "framebuffer");
        if (!fb_prop || fb_prop->type != RAD_TREE_PROP_STRING) continue;
        FramebufferRecord *fb = find_framebuffer(fb_prop->string_value);
        if (!fb) continue;
        TreeProperty *primary = find_property_mut(&node, "primary");
        if (primary && primary->type == RAD_TREE_PROP_BOOL && primary->bool_value) {
            rad_framebuffer_set_primary(fb->name);
        }
        TreeProperty *module = find_property_mut(&node, "module");
        if (module && module->type == RAD_TREE_PROP_STRING) copy_string(node.module, sizeof(node.module), module->string_value);
        node.bound = 1;
    }
    return RAD_STATUS_OK;
}

} // namespace

extern "C" {

void rad_overlay_reset(void) {
    memset(&g_overlay, 0, sizeof(g_overlay));
    allocate_node("/", "", "");
}

void rad_overlay_register_terminal_commands(void) {
    rad_terminal_register_command("overlays", "List loaded RAD overlays", cmd_overlays, nullptr);
    rad_terminal_register_command("tree", "List RAD runtime tree nodes", cmd_tree, nullptr);
    rad_terminal_register_command("bind", "Refresh RAD tree driver bindings", cmd_bind, nullptr);
    rad_terminal_register_command("drivers", "List registered RAD bus and child drivers", cmd_drivers, nullptr);
    rad_terminal_register_command("i2c", "List RAD I2C buses and devices", cmd_i2c, nullptr);
    rad_terminal_register_command("spi", "List RAD SPI buses and devices", cmd_spi, nullptr);
    rad_terminal_register_command("dma", "List RAD DMA channel status", cmd_dma, nullptr);
    rad_terminal_register_command("irq-tree", "List RAD IRQ domains and tree resources", cmd_irq_tree, nullptr);
    rad_terminal_register_command("fb", "List RAD framebuffers", cmd_fb, nullptr);
    rad_terminal_register_command("services", "List RAD boot services", cmd_services, nullptr);
}

rad_status_t rad_overlay_load_memory(const void *data, size_t size) {
    if (!data || size < sizeof(OverlayHeader)) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *bytes = static_cast<const uint8_t*>(data);
    const auto *header = reinterpret_cast<const OverlayHeader*>(bytes);
    if (memcmp(header->magic, "RADO", 4) != 0 || header->version != RAD_OVERLAY_VERSION) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t nodes_bytes = static_cast<size_t>(header->node_count) * sizeof(OverlayNodeRecord);
    const size_t props_bytes = static_cast<size_t>(header->property_count) * sizeof(OverlayPropertyRecord);
    const size_t payload_size = static_cast<size_t>(header->string_size) + nodes_bytes + props_bytes + static_cast<size_t>(header->data_size);
    if (sizeof(OverlayHeader) + payload_size != size) return RAD_STATUS_INVALID_ARGUMENT;
    if (crc32_update(0, bytes + sizeof(OverlayHeader), payload_size) != header->crc32) return RAD_STATUS_INVALID_ARGUMENT;

    const char *strings = reinterpret_cast<const char*>(bytes + sizeof(OverlayHeader));
    const auto *nodes = reinterpret_cast<const OverlayNodeRecord*>(bytes + sizeof(OverlayHeader) + header->string_size);
    const auto *props = reinterpret_cast<const OverlayPropertyRecord*>(bytes + sizeof(OverlayHeader) + header->string_size + nodes_bytes);
    const uint8_t *prop_data = bytes + sizeof(OverlayHeader) + header->string_size + nodes_bytes + props_bytes;

    OverlayRecord *overlay = allocate_overlay_record();
    if (!overlay) return RAD_STATUS_NO_MEMORY;
    overlay->node_count = header->node_count;
    overlay->property_count = header->property_count;
    copy_string(overlay->name, sizeof(overlay->name), "overlay");
    copy_string(overlay->source, sizeof(overlay->source), "memory");

    for (uint32_t i = 0; i < header->node_count; ++i) {
        const char *path = string_at(strings, header->string_size, nodes[i].path_offset);
        const char *name = string_at(strings, header->string_size, nodes[i].name_offset);
        const char *parent = string_at(strings, header->string_size, nodes[i].parent_offset);
        if (!path || !name || !parent) return RAD_STATUS_INVALID_ARGUMENT;
        TreeNode *node = allocate_node(path, name, parent);
        if (!node) return RAD_STATUS_NO_MEMORY;
        for (uint32_t p = 0; p < nodes[i].property_count; ++p) {
            const uint32_t prop_index = nodes[i].property_start + p;
            if (prop_index >= header->property_count) return RAD_STATUS_INVALID_ARGUMENT;
            const rad_status_t status = apply_property(node, props[prop_index], strings, header->string_size, prop_data, header->data_size);
            if (status != RAD_STATUS_OK) return status;
            if (strcmp(path, "/") == 0 && strcmp(string_at(strings, header->string_size, props[prop_index].name_offset), "name") == 0) {
                const TreeProperty *name_prop = find_property_mut(node, "name");
                if (name_prop && name_prop->type == RAD_TREE_PROP_STRING) copy_string(overlay->name, sizeof(overlay->name), name_prop->string_value);
            }
        }
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_overlay_load_file(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_stat_t stat{};
    rad_status_t status = rad_vfs_stat(path, &stat);
    if (status != RAD_STATUS_OK) return status;
    if (stat.is_directory || stat.size == 0 || stat.size > 65536u) return RAD_STATUS_INVALID_ARGUMENT;
    rad_file_t file = nullptr;
    status = rad_vfs_open(path, RAD_VFS_READ, &file);
    if (status != RAD_STATUS_OK) return status;
    auto *buffer = static_cast<uint8_t*>(rad_memory_alloc(static_cast<size_t>(stat.size)));
    if (!buffer) {
        rad_vfs_close(file);
        return RAD_STATUS_NO_MEMORY;
    }
    size_t done = 0;
    status = rad_vfs_read(file, buffer, static_cast<size_t>(stat.size), &done);
    rad_vfs_close(file);
    if (status == RAD_STATUS_OK && done == static_cast<size_t>(stat.size)) {
        status = rad_overlay_load_memory(buffer, static_cast<size_t>(stat.size));
        if (status == RAD_STATUS_OK) {
            for (size_t i = 0; i < RADIX_KERNEL_MAX_OVERLAYS; ++i) {
                if (g_overlay.overlays[i].used && strcmp(g_overlay.overlays[i].source, "memory") == 0) {
                    copy_string(g_overlay.overlays[i].source, sizeof(g_overlay.overlays[i].source), path);
                    break;
                }
            }
        }
    } else if (status == RAD_STATUS_OK) {
        status = RAD_STATUS_ERROR;
    }
    rad_memory_free(buffer);
    return status;
}

rad_status_t rad_overlay_apply_boot(void) {
    const char *path = rad_boot_arg_get("overlay.path");
    if (path && *path) {
        const rad_status_t status = rad_overlay_load_file(path);
        if (status != RAD_STATUS_OK && status != RAD_STATUS_NOT_FOUND) return status;
    }
    const char *dir_path = rad_boot_arg_get("overlay.dir");
    if (dir_path && *dir_path) {
        rad_dir_t dir = nullptr;
        if (rad_vfs_opendir(dir_path, &dir) == RAD_STATUS_OK) {
            rad_vfs_dirent_t entry{};
            while (rad_vfs_readdir(dir, &entry) == RAD_STATUS_OK) {
                const size_t len = strlen(entry.name);
                if (entry.stat.is_directory || len < 11 || strcmp(entry.name + len - 11, ".radoverlay") != 0) continue;
                char full[RAD_TREE_MAX_PATH];
                const size_t dir_len = strlen(dir_path);
                if (dir_len + 1u + len + 1u > sizeof(full)) continue;
                memcpy(full, dir_path, dir_len);
                full[dir_len] = '/';
                memcpy(full + dir_len + 1u, entry.name, len + 1u);
                rad_overlay_load_file(full);
            }
            rad_vfs_closedir(dir);
        }
    }
    rad_status_t status = bind_irq_domains_from_tree();
    if (status != RAD_STATUS_OK) return status;
    status = rad_i2c_bind_tree();
    if (status != RAD_STATUS_OK) return status;
    status = bind_spi_tree();
    if (status != RAD_STATUS_OK) return status;
    status = bind_framebuffer_tree();
    if (status != RAD_STATUS_OK) return status;
    return bind_service_tree();
}

size_t rad_overlay_list(rad_overlay_info_t *overlays, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_OVERLAYS; ++i) {
        if (!g_overlay.overlays[i].used) continue;
        if (overlays && count < capacity) {
            copy_string(overlays[count].name, sizeof(overlays[count].name), g_overlay.overlays[i].name);
            copy_string(overlays[count].source, sizeof(overlays[count].source), g_overlay.overlays[i].source);
            overlays[count].node_count = g_overlay.overlays[i].node_count;
            overlays[count].property_count = g_overlay.overlays[i].property_count;
        }
        ++count;
    }
    return count;
}

size_t rad_tree_list(rad_tree_node_info_t *nodes, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        if (nodes && count < capacity) {
            copy_string(nodes[count].path, sizeof(nodes[count].path), g_overlay.nodes[i].path);
            copy_string(nodes[count].name, sizeof(nodes[count].name), g_overlay.nodes[i].name);
            copy_string(nodes[count].parent, sizeof(nodes[count].parent), g_overlay.nodes[i].parent);
            copy_string(nodes[count].module, sizeof(nodes[count].module), g_overlay.nodes[i].module);
            nodes[count].bound = g_overlay.nodes[i].bound;
        }
        ++count;
    }
    return count;
}

rad_tree_node_t rad_tree_find(const char *path) {
    return reinterpret_cast<rad_tree_node_t>(find_node_mut(path));
}

rad_status_t rad_irq_domain_register(const rad_irq_domain_config_t *config, rad_irq_domain_t *domain) {
    if (!config || !config->tree_path || !*config->tree_path) return RAD_STATUS_INVALID_ARGUMENT;
    if (config->size && config->size < sizeof(rad_irq_domain_config_t)) return RAD_STATUS_INVALID_ARGUMENT;
    if (config->interrupt_cells == 0 || config->interrupt_cells > 2u) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_irq_domain_by_path(config->tree_path)) return RAD_STATUS_ALREADY_EXISTS;
    TreeNode *node = ensure_node_path(config->tree_path);
    if (!node) return RAD_STATUS_NO_MEMORY;
    set_property_bool(node, "interrupt-controller", 1);
    set_property_u32(node, "#interrupt-cells", config->interrupt_cells);
    set_property_u32(node, "interrupt-base", config->interrupt_base);
    set_property_u32(node, "interrupt-count", config->line_count);
    set_property_string(node, "name", config->name ? config->name : node->name);
    rad_irq_domain_handle *created = allocate_irq_domain(config);
    if (!created) return RAD_STATUS_NO_MEMORY;
    if (domain) *domain = created;
    return RAD_STATUS_OK;
}

rad_status_t rad_irq_domain_unregister(const char *tree_path) {
    rad_irq_domain_handle *domain = find_irq_domain_by_path(tree_path);
    if (!domain) return RAD_STATUS_NOT_FOUND;
    memset(domain, 0, sizeof(*domain));
    return RAD_STATUS_OK;
}

rad_status_t rad_irq_domain_find(const char *tree_path, rad_irq_domain_t *domain) {
    if (!tree_path || !domain) return RAD_STATUS_INVALID_ARGUMENT;
    rad_irq_domain_handle *found = find_irq_domain_by_path(tree_path);
    if (!found) return RAD_STATUS_NOT_FOUND;
    *domain = found;
    return RAD_STATUS_OK;
}

size_t rad_irq_resolve_tree(rad_tree_node_t node, rad_irq_resource_t *resources, size_t capacity) {
    return resolve_irq_resources(reinterpret_cast<TreeNode*>(node), resources, capacity);
}

rad_status_t rad_irq_resource_enable(const rad_irq_resource_t *resource) {
    return resource ? rad_irq_enable(resource->irq) : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t rad_irq_resource_disable(const rad_irq_resource_t *resource) {
    return resource ? rad_irq_disable(resource->irq) : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t rad_irq_resource_register_handler(const rad_irq_resource_t *resource, const char *name, rad_irq_handler_t handler, void *context) {
    if (!resource || !handler) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_irq_register(resource->irq, name ? name : resource->domain, handler, context);
}

rad_status_t rad_tree_get_property_u32(rad_tree_node_t node, const char *name, uint32_t *value) {
    TreeProperty *prop = find_property(node, name);
    if (!prop || !value) return RAD_STATUS_NOT_FOUND;
    if (prop->type != RAD_TREE_PROP_U32) return RAD_STATUS_INVALID_ARGUMENT;
    *value = prop->u32_value;
    return RAD_STATUS_OK;
}

size_t rad_tree_get_property_u32_array(rad_tree_node_t node, const char *name, uint32_t *values, size_t capacity) {
    TreeProperty *prop = find_property(node, name);
    if (!prop || prop->type != RAD_TREE_PROP_U32_ARRAY) return 0;
    for (size_t i = 0; values && i < prop->array_count && i < capacity; ++i) {
        values[i] = prop->array_values[i];
    }
    return prop->array_count;
}

rad_status_t rad_tree_get_property_bool(rad_tree_node_t node, const char *name, int *value) {
    TreeProperty *prop = find_property(node, name);
    if (!prop || !value) return RAD_STATUS_NOT_FOUND;
    if (prop->type != RAD_TREE_PROP_BOOL) return RAD_STATUS_INVALID_ARGUMENT;
    *value = prop->bool_value;
    return RAD_STATUS_OK;
}

rad_status_t rad_tree_get_property_string(rad_tree_node_t node, const char *name, char *buffer, size_t size) {
    TreeProperty *prop = find_property(node, name);
    if (!prop || !buffer || size == 0) return RAD_STATUS_NOT_FOUND;
    if (prop->type != RAD_TREE_PROP_STRING && prop->type != RAD_TREE_PROP_STRING_ARRAY) return RAD_STATUS_INVALID_ARGUMENT;
    copy_string(buffer, size, prop->string_value);
    return RAD_STATUS_OK;
}

rad_status_t rad_service_register(const rad_service_descriptor_t *descriptor) {
    if (!descriptor || !descriptor->name || !descriptor->compatible) return RAD_STATUS_INVALID_ARGUMENT;
    if (descriptor->size && descriptor->size < sizeof(rad_service_descriptor_t)) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_service_by_name(descriptor->name)) return RAD_STATUS_ALREADY_EXISTS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        ServiceRecord& service = g_overlay.services[i];
        if (service.used) continue;
        memset(&service, 0, sizeof(service));
        service.used = 1;
        service.descriptor = *descriptor;
        service.state = RAD_SERVICE_REGISTERED;
        service.order = descriptor->default_order;
        service.last_status = RAD_STATUS_OK;
        copy_string(service.name, sizeof(service.name), descriptor->name);
        copy_string(service.compatible, sizeof(service.compatible), descriptor->compatible);
        copy_string(service.capability, sizeof(service.capability), descriptor->capability ? descriptor->capability : "");
        bind_service_tree();
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_service_unregister(const char *name) {
    ServiceRecord *service = find_service_by_name(name);
    if (!service) return RAD_STATUS_NOT_FOUND;
    if (service->state == RAD_SERVICE_RUNNING && service->descriptor.stop) {
        service->descriptor.stop(service->descriptor.context);
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (g_overlay.nodes[i].used && strcmp(g_overlay.nodes[i].path, service->tree_path) == 0) {
            g_overlay.nodes[i].bound = 0;
        }
    }
    memset(service, 0, sizeof(*service));
    return RAD_STATUS_OK;
}

rad_status_t rad_service_configure_tree(rad_tree_node_t node) {
    return configure_service_node(reinterpret_cast<TreeNode*>(node));
}

rad_status_t rad_service_start(const char *name) {
    ServiceRecord *service = find_service_by_name(name);
    if (!service) return RAD_STATUS_NOT_FOUND;
    if (service->state == RAD_SERVICE_RUNNING) return RAD_STATUS_OK;
    rad_service_config_t config = service_config_for_record(*service);
    const rad_status_t status = service->descriptor.start
        ? service->descriptor.start(service->descriptor.context, &config)
        : RAD_STATUS_OK;
    service->last_status = status;
    service->state = status == RAD_STATUS_OK ? RAD_SERVICE_RUNNING : RAD_SERVICE_FAILED;
    return status;
}

rad_status_t rad_service_stop(const char *name) {
    ServiceRecord *service = find_service_by_name(name);
    if (!service) return RAD_STATUS_NOT_FOUND;
    if (service->state != RAD_SERVICE_RUNNING) return RAD_STATUS_OK;
    if (service->descriptor.stop) service->descriptor.stop(service->descriptor.context);
    service->state = RAD_SERVICE_STOPPED;
    service->last_status = RAD_STATUS_OK;
    return RAD_STATUS_OK;
}

rad_status_t rad_service_poll_all(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        ServiceRecord& service = g_overlay.services[i];
        if (!service.used || service.state != RAD_SERVICE_RUNNING || !service.descriptor.poll) continue;
        const rad_status_t status = service.descriptor.poll(service.descriptor.context);
        if (status != RAD_STATUS_OK) {
            service.last_status = status;
            service.state = RAD_SERVICE_FAILED;
            return status;
        }
    }
    return RAD_STATUS_OK;
}

size_t rad_service_list(rad_service_info_t *services, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        const ServiceRecord& service = g_overlay.services[i];
        if (!service.used) continue;
        if (services && count < capacity) {
            copy_string(services[count].name, sizeof(services[count].name), service.name);
            copy_string(services[count].compatible, sizeof(services[count].compatible), service.compatible);
            copy_string(services[count].capability, sizeof(services[count].capability), service.capability);
            copy_string(services[count].tree_path, sizeof(services[count].tree_path), service.tree_path);
            copy_string(services[count].backend, sizeof(services[count].backend), service.backend);
            copy_string(services[count].display, sizeof(services[count].display), service.display);
            copy_string(services[count].keyboard, sizeof(services[count].keyboard), service.keyboard);
            copy_string(services[count].pointer, sizeof(services[count].pointer), service.pointer);
            copy_string(services[count].terminal, sizeof(services[count].terminal), service.terminal);
            services[count].state = service.state;
            services[count].autostart = service.autostart;
            services[count].order = service.order;
            services[count].last_status = service.last_status;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_framebuffer_register(const char *name, const rad_framebuffer_info_t *info, const rad_framebuffer_ops_t *ops) {
    if (!name || !info) return RAD_STATUS_INVALID_ARGUMENT;
    rad_framebuffer_config_t config{};
    config.size = sizeof(config);
    config.name = name;
    config.info = *info;
    config.output_type = RAD_DISPLAY_OUTPUT_MEMORY;
    config.connector = "memory";
    config.mode_count = 1;
    config.preferred_mode = 0;
    config.primary = 0;
    config.modes[0].width = info->width;
    config.modes[0].height = info->height;
    config.modes[0].refresh_hz = 0;
    config.modes[0].stride_bytes = info->stride_bytes;
    config.modes[0].pixel_format = info->pixel_format;
    return rad_framebuffer_register_ex(&config, ops);
}

rad_status_t rad_framebuffer_register_ex(const rad_framebuffer_config_t *config, const rad_framebuffer_ops_t *ops) {
    if (!config || !config->name) return RAD_STATUS_INVALID_ARGUMENT;
    const rad_framebuffer_info_t *info = &config->info;
    if (!info->pixels || info->width == 0 || info->height == 0) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t bytes_per_pixel = info->pixel_format == RAD_PIXEL_FORMAT_RGB565 ? 2u
        : (info->pixel_format == RAD_PIXEL_FORMAT_XRGB8888 ? 4u : 0u);
    if (bytes_per_pixel == 0 || info->stride_bytes < info->width * bytes_per_pixel) return RAD_STATUS_INVALID_ARGUMENT;
    if (config->mode_count > RAD_FRAMEBUFFER_MAX_MODES) return RAD_STATUS_INVALID_ARGUMENT;
    if (config->mode_count > 0 && config->preferred_mode >= config->mode_count) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_framebuffer(config->name)) return RAD_STATUS_ALREADY_EXISTS;
    const int existing_primary = find_primary_framebuffer() != nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        if (g_overlay.framebuffers[i].used) continue;
        FramebufferRecord& fb = g_overlay.framebuffers[i];
        memset(&fb, 0, sizeof(fb));
        fb.used = 1;
        fb.handle.index = static_cast<uint32_t>(i);
        fb.info = *info;
        fb.info.size = sizeof(rad_framebuffer_info_t);
        fb.output_type = config->output_type;
        copy_string(fb.connector, sizeof(fb.connector), config->connector ? config->connector : display_output_type_name(config->output_type));
        fb.mode_count = config->mode_count;
        fb.preferred_mode = config->preferred_mode;
        fb.current_mode = config->preferred_mode;
        if (fb.mode_count == 0) {
            fb.mode_count = 1;
            fb.preferred_mode = 0;
            fb.current_mode = 0;
            fb.modes[0].width = info->width;
            fb.modes[0].height = info->height;
            fb.modes[0].refresh_hz = 0;
            fb.modes[0].stride_bytes = info->stride_bytes;
            fb.modes[0].pixel_format = info->pixel_format;
        } else {
            for (uint32_t mode = 0; mode < fb.mode_count; ++mode) fb.modes[mode] = config->modes[mode];
        }
        fb.primary = config->primary || !existing_primary;
        if (fb.primary) {
            for (size_t other = 0; other < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++other) {
                if (other != i) g_overlay.framebuffers[other].primary = 0;
            }
        }
        if (ops) fb.ops = *ops;
        copy_string(fb.name, sizeof(fb.name), config->name);
        fb.device_ops.context = &fb;
        fb.device_ops.ioctl = framebuffer_device_ioctl;
        const rad_status_t status = rad_device_register(config->name, RAD_DEVICE_FRAMEBUFFER, &fb.device_ops);
        if (status != RAD_STATUS_OK) {
            memset(&fb, 0, sizeof(fb));
            return status;
        }
        bind_framebuffer_tree();
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_framebuffer_unregister(const char *name) {
    FramebufferRecord *fb = find_framebuffer(name);
    if (!fb) return RAD_STATUS_NOT_FOUND;
    rad_device_unregister(fb->name);
    memset(fb, 0, sizeof(*fb));
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_open(const char *name, rad_framebuffer_t *framebuffer) {
    if (!framebuffer) return RAD_STATUS_INVALID_ARGUMENT;
    FramebufferRecord *fb = find_framebuffer(name);
    if (!fb) return RAD_STATUS_NOT_FOUND;
    ++fb->open_count;
    *framebuffer = &fb->handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_open_primary(rad_framebuffer_t *framebuffer) {
    if (!framebuffer) return RAD_STATUS_INVALID_ARGUMENT;
    FramebufferRecord *fb = find_primary_framebuffer();
    if (!fb) return RAD_STATUS_NOT_FOUND;
    ++fb->open_count;
    *framebuffer = &fb->handle;
    return RAD_STATUS_OK;
}

void rad_framebuffer_close(rad_framebuffer_t framebuffer) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (fb && fb->open_count > 0) --fb->open_count;
}

rad_status_t rad_framebuffer_get_info(rad_framebuffer_t framebuffer, rad_framebuffer_info_t *info) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb || !info) return RAD_STATUS_INVALID_ARGUMENT;
    *info = fb->info;
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_get_display_info(rad_framebuffer_t framebuffer, rad_framebuffer_display_info_t *info) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb || !info) return RAD_STATUS_INVALID_ARGUMENT;
    populate_framebuffer_display_info(*fb, info);
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_set_primary(const char *name) {
    FramebufferRecord *fb = find_framebuffer(name);
    if (!fb) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) g_overlay.framebuffers[i].primary = 0;
    fb->primary = 1;
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_set_mode(rad_framebuffer_t framebuffer, uint32_t mode_index) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb) return RAD_STATUS_INVALID_ARGUMENT;
    if (mode_index >= fb->mode_count) return RAD_STATUS_INVALID_ARGUMENT;
    if (mode_index == fb->current_mode) return RAD_STATUS_OK;
    if (!fb->ops.set_mode) return RAD_STATUS_NOT_SUPPORTED;
    const rad_status_t status = fb->ops.set_mode(fb->ops.context, &fb->modes[mode_index]);
    if (status == RAD_STATUS_OK) {
        fb->current_mode = mode_index;
        fb->info.width = fb->modes[mode_index].width;
        fb->info.height = fb->modes[mode_index].height;
        fb->info.stride_bytes = fb->modes[mode_index].stride_bytes;
        fb->info.pixel_format = fb->modes[mode_index].pixel_format;
    }
    return status;
}

rad_status_t rad_framebuffer_blank(rad_framebuffer_t framebuffer, int blanked) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb) return RAD_STATUS_INVALID_ARGUMENT;
    if (!fb->ops.blank) return RAD_STATUS_OK;
    return fb->ops.blank(fb->ops.context, blanked);
}

rad_status_t rad_framebuffer_get_vsync_counter(rad_framebuffer_t framebuffer, uint64_t *counter) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb || !counter) return RAD_STATUS_INVALID_ARGUMENT;
    if (!fb->ops.get_vsync_counter) return RAD_STATUS_NOT_SUPPORTED;
    *counter = fb->ops.get_vsync_counter(fb->ops.context);
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_clear(rad_framebuffer_t framebuffer, uint32_t color) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb) return RAD_STATUS_INVALID_ARGUMENT;
    framebuffer_fill_rect(fb, 0, 0, fb->info.width, fb->info.height, color);
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_draw_text(rad_framebuffer_t framebuffer, uint32_t cell_x, uint32_t cell_y, const char *text, uint32_t foreground, uint32_t background) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb || !text) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t columns = fb->info.width / 8u;
    const uint32_t rows = fb->info.height / 8u;
    if (cell_x >= columns || cell_y >= rows) return RAD_STATUS_INVALID_ARGUMENT;
    uint32_t x = cell_x;
    uint32_t y = cell_y;
    for (const char *p = text; *p && y < rows; ++p) {
        if (*p == '\n') {
            x = cell_x;
            ++y;
            continue;
        }
        framebuffer_draw_cell(fb, x, y, *p, foreground, background);
        ++x;
        if (x >= columns) {
            x = cell_x;
            ++y;
        }
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_flush(rad_framebuffer_t framebuffer, const rad_framebuffer_rect_t *rect) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb) return RAD_STATUS_INVALID_ARGUMENT;
    if (fb->ops.flush) return fb->ops.flush(fb->ops.context, rect);
    return RAD_STATUS_OK;
}

rad_status_t rad_framebuffer_present(rad_framebuffer_t framebuffer, const rad_framebuffer_present_t *present) {
    FramebufferRecord *fb = framebuffer_from_handle(framebuffer);
    if (!fb || !present || !present->pixels) return RAD_STATUS_INVALID_ARGUMENT;
    if (fb->ops.present) return fb->ops.present(fb->ops.context, present);
    if (fb->ops.flush) return fb->ops.flush(fb->ops.context, &present->rect);
    return RAD_STATUS_OK;
}

size_t rad_framebuffer_list(rad_framebuffer_info_t *framebuffers, char names[][64], size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        if (!g_overlay.framebuffers[i].used) continue;
        if (count < capacity) {
            if (framebuffers) framebuffers[count] = g_overlay.framebuffers[i].info;
            if (names) copy_string(names[count], 64, g_overlay.framebuffers[i].name);
        }
        ++count;
    }
    return count;
}

size_t rad_framebuffer_list_ex(rad_framebuffer_display_info_t *framebuffers, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_FRAMEBUFFERS; ++i) {
        if (!g_overlay.framebuffers[i].used) continue;
        if (count < capacity && framebuffers) populate_framebuffer_display_info(g_overlay.framebuffers[i], &framebuffers[count]);
        ++count;
    }
    return count;
}

rad_status_t rad_i2c_controller_register(const rad_i2c_controller_config_t *config, const rad_i2c_controller_ops_t *ops) {
    if (!config || !ops || !ops->transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_i2c(config->bus_id)) return RAD_STATUS_ALREADY_EXISTS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_CONTROLLERS; ++i) {
        if (g_overlay.i2c[i].used) continue;
        I2cController& controller = g_overlay.i2c[i];
        memset(&controller, 0, sizeof(controller));
        controller.used = 1;
        controller.bus_id = config->bus_id;
        controller.clock_hz = config->clock_hz;
        controller.sda_gpio = config->sda_gpio;
        controller.scl_gpio = config->scl_gpio;
        controller.ops = *ops;
        snprintf(controller.name, sizeof(controller.name), "%s", config->name ? config->name : "i2c");
        snprintf(controller.device_name, sizeof(controller.device_name), "/dev/i2c%u", static_cast<unsigned>(config->bus_id));
        copy_string(controller.tree_path, sizeof(controller.tree_path), config->tree_path);
        controller.device_ops.context = &controller;
        controller.device_ops.ioctl = i2c_device_ioctl;
        const rad_status_t status = rad_device_register(controller.device_name, RAD_DEVICE_I2C, &controller.device_ops);
        if (status != RAD_STATUS_OK) {
            memset(&controller, 0, sizeof(controller));
            return status;
        }
        return rad_i2c_bind_tree();
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_i2c_controller_unregister(uint32_t bus_id) {
    I2cController *controller = find_i2c(bus_id);
    if (!controller) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_DEVICES; ++i) {
        rad_i2c_device& device = g_overlay.i2c_devices[i];
        if (device.controller != controller) continue;
        if (device.bound && device.driver_index < RADIX_KERNEL_MAX_BUS_DRIVERS) {
            I2cDriver& driver = g_overlay.i2c_drivers[device.driver_index];
            if (driver.used && driver.spec.remove) driver.spec.remove(driver.spec.context, &device);
        }
        TreeNode *node = find_node_mut(device.path);
        if (node) node->bound = 0;
        memset(&device, 0, sizeof(device));
    }
    rad_device_unregister(controller->device_name);
    memset(controller, 0, sizeof(*controller));
    return RAD_STATUS_OK;
}

rad_status_t rad_i2c_bus_open(uint32_t bus_id, rad_i2c_bus_t *bus) {
    if (!bus) return RAD_STATUS_INVALID_ARGUMENT;
    I2cController *controller = find_i2c(bus_id);
    if (!controller) return RAD_STATUS_NOT_FOUND;
    *bus = reinterpret_cast<rad_i2c_bus_t>(controller);
    return RAD_STATUS_OK;
}

void rad_i2c_bus_close(rad_i2c_bus_t) {
}

rad_status_t rad_i2c_transfer(rad_i2c_bus_t bus, const rad_i2c_transfer_t *transfer) {
    auto *controller = reinterpret_cast<I2cController*>(bus);
    if (!controller || !controller->used || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    return controller->ops.transfer
        ? controller->ops.transfer(controller->ops.context, transfer)
        : RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_i2c_bind_tree(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TREE_NODES; ++i) {
        if (!g_overlay.nodes[i].used) continue;
        uint32_t bus_id = 0;
        if (!node_is_under_i2c(g_overlay.nodes[i], &bus_id)) continue;
        TreeProperty *reg = find_property_mut(&g_overlay.nodes[i], "reg");
        if (!reg || reg->type != RAD_TREE_PROP_U32) continue;
        I2cController *controller = find_i2c(bus_id);
        if (!controller) continue;
        TreeProperty *compatible = find_property_mut(&g_overlay.nodes[i], "compatible");
        if (!compatible || compatible->type != RAD_TREE_PROP_STRING) continue;
        rad_i2c_device *device = find_i2c_device_by_path(g_overlay.nodes[i].path);
        if (!device) {
            for (size_t d = 0; d < RADIX_KERNEL_MAX_I2C_DEVICES; ++d) {
                if (g_overlay.i2c_devices[d].controller) continue;
                device = &g_overlay.i2c_devices[d];
                memset(device, 0, sizeof(*device));
                break;
            }
        }
        if (!device) return RAD_STATUS_NO_MEMORY;
        if (device->bound) {
            g_overlay.nodes[i].bound = 1;
            continue;
        }
        device->bus_id = bus_id;
        device->address = static_cast<uint8_t>(reg->u32_value & 0x7fu);
        device->controller = controller;
        device->irq_count = static_cast<uint32_t>(resolve_irq_resources(&g_overlay.nodes[i], device->irqs, RAD_IRQ_MAX_RESOURCES));
        copy_string(device->path, sizeof(device->path), g_overlay.nodes[i].path);
        copy_string(device->compatible, sizeof(device->compatible), compatible->string_value);
        uint32_t driver_index = 0;
        I2cDriver *driver = find_i2c_driver_by_compatible(device->compatible, &driver_index);
        if (!driver) {
            device->bound = 0;
            device->driver[0] = '\0';
            g_overlay.nodes[i].bound = 0;
            continue;
        }
        device->driver_index = driver_index;
        copy_string(device->driver, sizeof(device->driver), driver->name);
        rad_status_t status = driver->spec.probe
            ? driver->spec.probe(driver->spec.context, device)
            : RAD_STATUS_OK;
        if (status != RAD_STATUS_OK) return status;
        device->bound = 1;
        g_overlay.nodes[i].bound = 1;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_i2c_driver_register(const rad_i2c_driver_t *driver) {
    if (!driver || !driver->name || !driver->compatible) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_i2c_driver_by_name(driver->name)) return RAD_STATUS_ALREADY_EXISTS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.i2c_drivers[i].used) continue;
        I2cDriver& slot = g_overlay.i2c_drivers[i];
        memset(&slot, 0, sizeof(slot));
        slot.used = 1;
        slot.spec = *driver;
        copy_string(slot.name, sizeof(slot.name), driver->name);
        copy_string(slot.compatible, sizeof(slot.compatible), driver->compatible);
        return rad_i2c_bind_tree();
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_i2c_driver_unregister(const char *name) {
    I2cDriver *driver = find_i2c_driver_by_name(name);
    if (!driver) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_DEVICES; ++i) {
        rad_i2c_device& device = g_overlay.i2c_devices[i];
        if (!device.controller || strcmp(device.driver, driver->name) != 0) continue;
        if (device.bound && driver->spec.remove) driver->spec.remove(driver->spec.context, &device);
        device.bound = 0;
        device.driver[0] = '\0';
        TreeNode *node = find_node_mut(device.path);
        if (node) node->bound = 0;
    }
    memset(driver, 0, sizeof(*driver));
    return RAD_STATUS_OK;
}

rad_status_t rad_i2c_device_open(uint32_t bus_id, uint8_t address, rad_i2c_device_t **device) {
    if (!device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_i2c_device *found = find_i2c_device(bus_id, address);
    if (!found || !found->controller) return RAD_STATUS_NOT_FOUND;
    ++found->open_count;
    *device = found;
    return RAD_STATUS_OK;
}

void rad_i2c_device_close(rad_i2c_device_t *device) {
    if (device && device->open_count > 0) --device->open_count;
}

rad_status_t rad_i2c_device_transfer(rad_i2c_device_t *device, const rad_i2c_transfer_t *transfer) {
    if (!device || !device->controller || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    auto *controller = static_cast<I2cController*>(device->controller);
    if (!controller->ops.transfer) return RAD_STATUS_NOT_SUPPORTED;
    rad_i2c_transfer_t routed = *transfer;
    routed.address = device->address;
    return controller->ops.transfer(controller->ops.context, &routed);
}

size_t rad_i2c_device_irq_count(const rad_i2c_device_t *device) {
    return device ? device->irq_count : 0;
}

rad_status_t rad_i2c_device_get_irq(const rad_i2c_device_t *device, size_t index, rad_irq_resource_t *resource) {
    if (!device || !resource) return RAD_STATUS_INVALID_ARGUMENT;
    if (index >= device->irq_count || index >= RAD_IRQ_MAX_RESOURCES) return RAD_STATUS_NOT_FOUND;
    *resource = device->irqs[index];
    return RAD_STATUS_OK;
}

size_t rad_i2c_list_devices(rad_i2c_device_info_t *devices, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_DEVICES; ++i) {
        const rad_i2c_device& device = g_overlay.i2c_devices[i];
        if (!device.controller) continue;
        if (devices && count < capacity) {
            devices[count].bus_id = device.bus_id;
            devices[count].address = device.address;
            devices[count].irq_count = device.irq_count;
            for (uint32_t irq = 0; irq < device.irq_count && irq < RAD_IRQ_MAX_RESOURCES; ++irq) {
                devices[count].irqs[irq] = device.irqs[irq];
            }
            copy_string(devices[count].path, sizeof(devices[count].path), device.path);
            copy_string(devices[count].compatible, sizeof(devices[count].compatible), device.compatible);
            copy_string(devices[count].driver, sizeof(devices[count].driver), device.driver);
            devices[count].bound = device.bound;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_i2c_transfer_device(rad_device_t device, const rad_i2c_transfer_t *transfer) {
    if (!transfer) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_I2C_TRANSFER, const_cast<rad_i2c_transfer_t*>(transfer));
}

rad_status_t rad_spi_controller_register(const rad_spi_controller_config_t *config, const rad_spi_controller_ops_t *ops) {
    if (!config || !ops || !ops->transfer) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_spi(config->bus_id)) return RAD_STATUS_ALREADY_EXISTS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_CONTROLLERS; ++i) {
        if (g_overlay.spi[i].used) continue;
        SpiController& controller = g_overlay.spi[i];
        memset(&controller, 0, sizeof(controller));
        controller.used = 1;
        controller.bus_id = config->bus_id;
        controller.clock_hz = config->clock_hz;
        controller.sck_gpio = config->sck_gpio;
        controller.tx_gpio = config->tx_gpio;
        controller.rx_gpio = config->rx_gpio;
        controller.cs_gpio = config->cs_gpio;
        controller.ops = *ops;
        snprintf(controller.name, sizeof(controller.name), "%s", config->name ? config->name : "spi");
        snprintf(controller.device_name, sizeof(controller.device_name), "/dev/spi%u", static_cast<unsigned>(config->bus_id));
        copy_string(controller.tree_path, sizeof(controller.tree_path), config->tree_path);
        controller.device_ops.context = &controller;
        controller.device_ops.ioctl = spi_device_ioctl;
        const rad_status_t status = rad_device_register(controller.device_name, RAD_DEVICE_SPI, &controller.device_ops);
        if (status != RAD_STATUS_OK) {
            memset(&controller, 0, sizeof(controller));
            return status;
        }
        return bind_spi_tree();
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_spi_controller_unregister(uint32_t bus_id) {
    SpiController *controller = find_spi(bus_id);
    if (!controller) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_DEVICES; ++i) {
        rad_spi_device& device = g_overlay.spi_devices[i];
        if (device.controller != controller) continue;
        if (device.bound && device.driver_index < RADIX_KERNEL_MAX_BUS_DRIVERS) {
            SpiDriver& driver = g_overlay.spi_drivers[device.driver_index];
            if (driver.used && driver.spec.remove) driver.spec.remove(driver.spec.context, &device);
        }
        TreeNode *node = find_node_mut(device.path);
        if (node) node->bound = 0;
        memset(&device, 0, sizeof(device));
    }
    rad_device_unregister(controller->device_name);
    memset(controller, 0, sizeof(*controller));
    return RAD_STATUS_OK;
}

rad_status_t rad_spi_bus_open(uint32_t bus_id, rad_spi_bus_t *bus) {
    if (!bus) return RAD_STATUS_INVALID_ARGUMENT;
    SpiController *controller = find_spi(bus_id);
    if (!controller) return RAD_STATUS_NOT_FOUND;
    *bus = reinterpret_cast<rad_spi_bus_t>(controller);
    return RAD_STATUS_OK;
}

void rad_spi_bus_close(rad_spi_bus_t) {
}

rad_status_t rad_spi_transfer(rad_spi_bus_t bus, const rad_spi_transfer_t *transfer) {
    auto *controller = reinterpret_cast<SpiController*>(bus);
    if (!controller || !controller->used || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    rad_spi_device device{};
    device.bus_id = controller->bus_id;
    device.cs = transfer->cs;
    device.clock_hz = transfer->speed_hz ? transfer->speed_hz : controller->clock_hz;
    device.mode = transfer->mode;
    device.bits_per_word = transfer->bits_per_word ? transfer->bits_per_word : 8u;
    device.transfer_mode = transfer->transfer_mode;
    device.controller = controller;
    return rad_spi_device_transfer(&device, transfer);
}

rad_status_t rad_spi_driver_register(const rad_spi_driver_t *driver) {
    if (!driver || !driver->name || !driver->compatible) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_spi_driver_by_name(driver->name)) return RAD_STATUS_ALREADY_EXISTS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.spi_drivers[i].used) continue;
        SpiDriver& slot = g_overlay.spi_drivers[i];
        memset(&slot, 0, sizeof(slot));
        slot.used = 1;
        slot.spec = *driver;
        copy_string(slot.name, sizeof(slot.name), driver->name);
        copy_string(slot.compatible, sizeof(slot.compatible), driver->compatible);
        return bind_spi_tree();
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_spi_driver_unregister(const char *name) {
    SpiDriver *driver = find_spi_driver_by_name(name);
    if (!driver) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_DEVICES; ++i) {
        rad_spi_device& device = g_overlay.spi_devices[i];
        if (!device.controller || strcmp(device.driver, driver->name) != 0) continue;
        if (device.bound && driver->spec.remove) driver->spec.remove(driver->spec.context, &device);
        device.bound = 0;
        device.driver[0] = '\0';
        TreeNode *node = find_node_mut(device.path);
        if (node) node->bound = 0;
    }
    memset(driver, 0, sizeof(*driver));
    return RAD_STATUS_OK;
}

rad_status_t rad_spi_device_open(uint32_t bus_id, uint8_t cs, rad_spi_device_t **device) {
    if (!device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_spi_device *found = find_spi_device(bus_id, cs);
    if (!found || !found->controller) return RAD_STATUS_NOT_FOUND;
    ++found->open_count;
    *device = found;
    return RAD_STATUS_OK;
}

void rad_spi_device_close(rad_spi_device_t *device) {
    if (device && device->open_count > 0) --device->open_count;
}

rad_status_t rad_spi_device_transfer(rad_spi_device_t *device, const rad_spi_transfer_t *transfer) {
    if (!device || !device->controller || !transfer) return RAD_STATUS_INVALID_ARGUMENT;
    auto *controller = static_cast<SpiController*>(device->controller);
    rad_spi_transfer_t routed = *transfer;
    if (!routed.speed_hz) routed.speed_hz = device->clock_hz ? device->clock_hz : controller->clock_hz;
    if (!routed.bits_per_word) routed.bits_per_word = device->bits_per_word ? device->bits_per_word : 8u;
    routed.cs = device->cs;
    if (routed.transfer_mode == RAD_SPI_TRANSFER_MODE_AUTO && device->transfer_mode != RAD_SPI_TRANSFER_MODE_AUTO) {
        routed.transfer_mode = device->transfer_mode;
    }

    const int wants_dma = routed.transfer_mode == RAD_SPI_TRANSFER_MODE_DMA
        || (routed.transfer_mode == RAD_SPI_TRANSFER_MODE_AUTO
            && routed.tx_data && routed.rx_data
            && routed.size >= RADIX_KERNEL_SPI_AUTO_DMA_THRESHOLD
            && dma_backend_available());
    if (wants_dma) {
        if (!controller->ops.transfer_dma) {
            if (routed.transfer_mode == RAD_SPI_TRANSFER_MODE_DMA) return RAD_STATUS_NOT_SUPPORTED;
        } else {
            rad_dma_channel_t tx = nullptr;
            rad_dma_channel_t rx = nullptr;
            rad_status_t status = rad_dma_channel_request(device->bus_id == 0 ? RAD_DMA_DREQ_SPI0_TX : RAD_DMA_DREQ_SPI1_TX, &tx);
            if (status == RAD_STATUS_OK) status = rad_dma_channel_request(device->bus_id == 0 ? RAD_DMA_DREQ_SPI0_RX : RAD_DMA_DREQ_SPI1_RX, &rx);
            if (status == RAD_STATUS_OK) {
                status = controller->ops.transfer_dma(controller->ops.context, device, &routed, tx, rx);
            } else if (routed.transfer_mode == RAD_SPI_TRANSFER_MODE_DMA) {
                if (tx) rad_dma_channel_release(tx);
                if (rx) rad_dma_channel_release(rx);
                return status;
            }
            if (tx) rad_dma_channel_release(tx);
            if (rx) rad_dma_channel_release(rx);
            if (status == RAD_STATUS_OK || routed.transfer_mode == RAD_SPI_TRANSFER_MODE_DMA) return status;
        }
    }
    return controller->ops.transfer
        ? controller->ops.transfer(controller->ops.context, device, &routed)
        : RAD_STATUS_NOT_SUPPORTED;
}

size_t rad_spi_device_irq_count(const rad_spi_device_t *device) {
    return device ? device->irq_count : 0;
}

rad_status_t rad_spi_device_get_irq(const rad_spi_device_t *device, size_t index, rad_irq_resource_t *resource) {
    if (!device || !resource) return RAD_STATUS_INVALID_ARGUMENT;
    if (index >= device->irq_count || index >= RAD_IRQ_MAX_RESOURCES) return RAD_STATUS_NOT_FOUND;
    *resource = device->irqs[index];
    return RAD_STATUS_OK;
}

size_t rad_spi_list_devices(rad_spi_device_info_t *devices, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_DEVICES; ++i) {
        const rad_spi_device& device = g_overlay.spi_devices[i];
        if (!device.controller) continue;
        if (devices && count < capacity) {
            devices[count].bus_id = device.bus_id;
            devices[count].cs = device.cs;
            devices[count].clock_hz = device.clock_hz;
            devices[count].mode = device.mode;
            devices[count].bits_per_word = device.bits_per_word;
            devices[count].transfer_mode = device.transfer_mode;
            devices[count].irq_count = device.irq_count;
            for (uint32_t irq = 0; irq < device.irq_count && irq < RAD_IRQ_MAX_RESOURCES; ++irq) {
                devices[count].irqs[irq] = device.irqs[irq];
            }
            copy_string(devices[count].path, sizeof(devices[count].path), device.path);
            copy_string(devices[count].compatible, sizeof(devices[count].compatible), device.compatible);
            copy_string(devices[count].driver, sizeof(devices[count].driver), device.driver);
            devices[count].bound = device.bound;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_dma_controller_register(const rad_dma_controller_config_t *config, const rad_dma_backend_ops_t *ops) {
    if (!config || !config->name || !ops) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++i) {
        if (g_overlay.dma[i].used && strcmp(g_overlay.dma[i].name, config->name) == 0) return RAD_STATUS_ALREADY_EXISTS;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++i) {
        if (g_overlay.dma[i].used) continue;
        DmaController& controller = g_overlay.dma[i];
        memset(&controller, 0, sizeof(controller));
        controller.used = 1;
        controller.channel_count = config->channel_count;
        controller.ops = *ops;
        copy_string(controller.name, sizeof(controller.name), config->name);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_dma_controller_unregister(const char *name) {
    if (!name) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++i) {
        DmaController& controller = g_overlay.dma[i];
        if (!controller.used || strcmp(controller.name, name) != 0) continue;
        for (size_t ch = 0; ch < RADIX_KERNEL_MAX_DMA_CHANNELS; ++ch) {
            if (!g_overlay.dma_channels[ch].used || g_overlay.dma_channels[ch].controller_index != i) continue;
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        memset(&controller, 0, sizeof(controller));
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_dma_channel_request(rad_dma_request_id_t request_id, rad_dma_channel_t *channel) {
    if (!channel) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t controller_index = 0; controller_index < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++controller_index) {
        DmaController& controller = g_overlay.dma[controller_index];
        if (!controller.used || controller.channel_count == 0) continue;
        for (uint32_t channel_index = 0; channel_index < controller.channel_count && channel_index < RADIX_KERNEL_MAX_DMA_CHANNELS; ++channel_index) {
            int in_use = 0;
            for (size_t scan = 0; scan < RADIX_KERNEL_MAX_DMA_CHANNELS; ++scan) {
                if (g_overlay.dma_channels[scan].used
                    && g_overlay.dma_channels[scan].controller_index == controller_index
                    && g_overlay.dma_channels[scan].index == channel_index) {
                    in_use = 1;
                    break;
                }
            }
            if (in_use) continue;
            for (size_t slot = 0; slot < RADIX_KERNEL_MAX_DMA_CHANNELS; ++slot) {
                rad_dma_channel_handle& handle = g_overlay.dma_channels[slot];
                if (handle.used) continue;
                memset(&handle, 0, sizeof(handle));
                handle.used = 1;
                handle.controller_index = static_cast<uint32_t>(controller_index);
                handle.index = channel_index;
                handle.request_id = request_id;
                rad_status_t status = RAD_STATUS_OK;
                if (controller.ops.request) {
                    status = controller.ops.request(controller.ops.context, request_id, &handle.backend_channel);
                }
                if (status != RAD_STATUS_OK) {
                    memset(&handle, 0, sizeof(handle));
                    return status;
                }
                *channel = &handle;
                return RAD_STATUS_OK;
            }
            return RAD_STATUS_NO_MEMORY;
        }
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

void rad_dma_channel_release(rad_dma_channel_t channel) {
    if (!channel || !channel->used || channel->controller_index >= RADIX_KERNEL_MAX_DMA_CONTROLLERS) return;
    DmaController& controller = g_overlay.dma[channel->controller_index];
    if (controller.used && controller.ops.release) controller.ops.release(controller.ops.context, channel->backend_channel);
    memset(channel, 0, sizeof(*channel));
}

rad_status_t rad_dma_submit(rad_dma_channel_t channel, const rad_dma_transfer_t *transfer) {
    if (!channel || !channel->used || !transfer || channel->controller_index >= RADIX_KERNEL_MAX_DMA_CONTROLLERS) return RAD_STATUS_INVALID_ARGUMENT;
    DmaController& controller = g_overlay.dma[channel->controller_index];
    if (!controller.used) return RAD_STATUS_NOT_SUPPORTED;
    channel->active = 1;
    if (controller.ops.submit) {
        const rad_status_t status = controller.ops.submit(controller.ops.context, channel->backend_channel, transfer);
        if (status != RAD_STATUS_OK) channel->active = 0;
        return status;
    }
    if (transfer->type == RAD_DMA_MEMORY_TO_MEMORY && transfer->source && transfer->destination) {
        memcpy(transfer->destination, transfer->source, transfer->size);
        channel->active = 0;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_dma_wait(rad_dma_channel_t channel, uint32_t timeout_ms) {
    if (!channel || !channel->used || channel->controller_index >= RADIX_KERNEL_MAX_DMA_CONTROLLERS) return RAD_STATUS_INVALID_ARGUMENT;
    DmaController& controller = g_overlay.dma[channel->controller_index];
    if (!controller.used) return RAD_STATUS_NOT_SUPPORTED;
    rad_status_t status = RAD_STATUS_OK;
    if (controller.ops.wait) status = controller.ops.wait(controller.ops.context, channel->backend_channel, timeout_ms);
    (void)timeout_ms;
    if (status == RAD_STATUS_OK) channel->active = 0;
    return status;
}

rad_status_t rad_dma_cancel(rad_dma_channel_t channel) {
    if (!channel || !channel->used || channel->controller_index >= RADIX_KERNEL_MAX_DMA_CONTROLLERS) return RAD_STATUS_INVALID_ARGUMENT;
    DmaController& controller = g_overlay.dma[channel->controller_index];
    if (!controller.used) return RAD_STATUS_NOT_SUPPORTED;
    rad_status_t status = controller.ops.cancel
        ? controller.ops.cancel(controller.ops.context, channel->backend_channel)
        : RAD_STATUS_OK;
    if (status == RAD_STATUS_OK) channel->active = 0;
    return status;
}

size_t rad_dma_list_channels(rad_dma_channel_info_t *channels, size_t capacity) {
    size_t count = 0;
    for (size_t controller_index = 0; controller_index < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++controller_index) {
        const DmaController& controller = g_overlay.dma[controller_index];
        if (!controller.used) continue;
        for (uint32_t ch = 0; ch < controller.channel_count; ++ch) {
            const rad_dma_channel_handle *allocated = nullptr;
            for (size_t slot = 0; slot < RADIX_KERNEL_MAX_DMA_CHANNELS; ++slot) {
                if (g_overlay.dma_channels[slot].used
                    && g_overlay.dma_channels[slot].controller_index == controller_index
                    && g_overlay.dma_channels[slot].index == ch) {
                    allocated = &g_overlay.dma_channels[slot];
                    break;
                }
            }
            if (channels && count < capacity) {
                copy_string(channels[count].controller, sizeof(channels[count].controller), controller.name);
                channels[count].index = ch;
                channels[count].request_id = allocated ? allocated->request_id : RAD_DMA_DREQ_NONE;
                channels[count].allocated = allocated ? 1 : 0;
                channels[count].active = allocated ? allocated->active : 0;
            }
            ++count;
        }
    }
    return count;
}

size_t rad_driver_list(rad_driver_info_t *drivers, size_t capacity) {
    size_t count = 0;
    auto append = [&](const char *name, const char *bus, const char *role, const char *compatible) {
        if (drivers && count < capacity) {
            copy_string(drivers[count].name, sizeof(drivers[count].name), name);
            copy_string(drivers[count].bus, sizeof(drivers[count].bus), bus);
            copy_string(drivers[count].role, sizeof(drivers[count].role), role);
            copy_string(drivers[count].compatible, sizeof(drivers[count].compatible), compatible);
        }
        ++count;
    };
    append("rad_i2c_core", "i2c", "bus-core", "");
    append("rad_spi_core", "spi", "bus-core", "");
    append("rad_dma_core", "dma", "bus-core", "");
    for (size_t i = 0; i < RADIX_KERNEL_MAX_I2C_CONTROLLERS; ++i) {
        if (g_overlay.i2c[i].used) append(g_overlay.i2c[i].name, "i2c", "controller", "");
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SPI_CONTROLLERS; ++i) {
        if (g_overlay.spi[i].used) append(g_overlay.spi[i].name, "spi", "controller", "");
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DMA_CONTROLLERS; ++i) {
        if (g_overlay.dma[i].used) append(g_overlay.dma[i].name, "dma", "controller", "");
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.i2c_drivers[i].used) append(g_overlay.i2c_drivers[i].name, "i2c", "child", g_overlay.i2c_drivers[i].compatible);
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_BUS_DRIVERS; ++i) {
        if (g_overlay.spi_drivers[i].used) append(g_overlay.spi_drivers[i].name, "spi", "child", g_overlay.spi_drivers[i].compatible);
    }
    return count;
}

}

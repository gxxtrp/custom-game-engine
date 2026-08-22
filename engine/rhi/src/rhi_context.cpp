#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <set>
#include <cstring>

namespace engine::rhi {

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/) {

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_ERROR("Vulkan", "{}", callback_data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("Vulkan", "{}", callback_data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOG_INFO("Vulkan", "{}", callback_data->pMessage);
    }
    return VK_FALSE;
}

RhiContext& RhiContext::instance() {
    static RhiContext s_instance;
    return s_instance;
}

RhiContext::~RhiContext() {
    shutdown();
}

bool RhiContext::init(core::Window& window, bool enable_validation) {
    if (m_device != VK_NULL_HANDLE) return true;

    LOG_INFO("RHI", "Initializing Vulkan 1.3 RHI...");

    if (!create_instance(enable_validation)) return false;
    if (!create_surface(window)) return false;
    if (!select_physical_device()) return false;
    if (!create_logical_device()) return false;
    if (!init_vma()) return false;

    LOG_INFO("RHI", "Vulkan 1.3 RHI initialized successfully on GPU: {}", m_caps.device_name);
    return true;
}

void RhiContext::shutdown() {
    if (m_device == VK_NULL_HANDLE) return;

    wait_idle();

    if (m_vma_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_vma_allocator);
        m_vma_allocator = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_debug_messenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debug_messenger, nullptr);
        m_debug_messenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    LOG_INFO("RHI", "Vulkan RHI shutdown cleanly");
}

void RhiContext::wait_idle() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }
}

bool RhiContext::create_instance(bool enable_validation) {
    m_validation_enabled = enable_validation;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Modern Game Engine";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "ModernEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    // Instance extensions from SDL3
    Uint32 sdl_ext_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);

    std::vector<const char*> extensions;
    if (sdl_extensions) {
        for (Uint32 i = 0; i < sdl_ext_count; ++i) {
            extensions.push_back(sdl_extensions[i]);
        }
    } else {
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(ENGINE_PLATFORM_WINDOWS)
        extensions.push_back("VK_KHR_win32_surface");
#endif
    }

    std::vector<const char*> layers;
    if (enable_validation) {
        // Query instance layers
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        bool validation_available = false;
        for (const auto& layer : available_layers) {
            if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validation_available = true;
                break;
            }
        }

        if (validation_available) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            LOG_INFO("RHI", "Enabled Vulkan validation layers");
        } else {
            LOG_WARN("RHI", "VK_LAYER_KHRONOS_validation requested but not available");
        }
    }

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();

    VkResult res = vkCreateInstance(&create_info, nullptr, &m_instance);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create Vulkan instance: {}", static_cast<int>(res));
        return false;
    }

    if (enable_validation && std::find(extensions.begin(), extensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != extensions.end()) {
        VkDebugUtilsMessengerCreateInfoEXT debug_info{};
        debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_info.pfnUserCallback = debug_callback;

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func) func(m_instance, &debug_info, nullptr, &m_debug_messenger);
    }

    return true;
}

bool RhiContext::create_surface(core::Window& window) {
    if (!SDL_Vulkan_CreateSurface(window.get_sdl_window(), m_instance, nullptr, &m_surface)) {
        LOG_FATAL("RHI", "Failed to create Vulkan surface: {}", SDL_GetError());
        return false;
    }
    return true;
}

bool RhiContext::select_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    if (device_count == 0) {
        LOG_FATAL("RHI", "No GPUs with Vulkan support found!");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    VkPhysicalDevice selected = VK_NULL_HANDLE;
    int best_score = -1;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        // Find queue families
        uint32_t q_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &q_count, nullptr);
        std::vector<VkQueueFamilyProperties> q_props(q_count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &q_count, q_props.data());

        QueueFamilyIndices indices;
        for (uint32_t i = 0; i < q_count; ++i) {
            if (q_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surface, &present_support);
                if (present_support) {
                    indices.graphics_family = i;
                }
            }
            if ((q_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && !(q_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.compute_family = i;
            }
            if ((q_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && !(q_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.transfer_family = i;
            }
        }

        if (indices.compute_family == UINT32_MAX) indices.compute_family = indices.graphics_family;
        if (indices.transfer_family == UINT32_MAX) indices.transfer_family = indices.graphics_family;

        if (!indices.is_complete()) continue;

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 100;

        if (score > best_score) {
            best_score = score;
            selected = dev;
            m_queue_families = indices;
        }
    }

    if (selected == VK_NULL_HANDLE) {
        LOG_FATAL("RHI", "Failed to find a suitable Vulkan physical device!");
        return false;
    }

    m_physical_device = selected;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physical_device, &props);
    m_caps.device_name = props.deviceName;
    m_caps.api_version = props.apiVersion;
    m_caps.driver_version = props.driverVersion;

    // Check extensions
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> avail_exts(ext_count);
    vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &ext_count, avail_exts.data());

    for (const auto& ext : avail_exts) {
        if (std::strcmp(ext.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) m_caps.ray_tracing_supported = true;
        if (std::strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0) m_caps.mesh_shader_supported = true;
        if (std::strcmp(ext.extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) m_caps.buffer_device_address_supported = true;
    }

    return true;
}

bool RhiContext::create_logical_device() {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {
        m_queue_families.graphics_family,
        m_queue_families.compute_family,
        m_queue_families.transfer_family
    };

    float queue_priority = 1.0f;
    for (uint32_t qf : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = qf;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_info);
    }

    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };

    if (m_caps.buffer_device_address_supported) {
        device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }

    // Vulkan 1.3 Core Features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.timelineSemaphore = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    features12.bufferDeviceAddress = m_caps.buffer_device_address_supported ? VK_TRUE : VK_FALSE;
    features12.pNext = &features13;

    VkPhysicalDeviceFeatures2 device_features2{};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.features.samplerAnisotropy = VK_TRUE;
    device_features2.features.independentBlend = VK_TRUE; // per-attachment blend (WBOIT)
    device_features2.pNext = &features12;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.pNext = &device_features2;

    VkResult res = vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create Vulkan logical device: {}", static_cast<int>(res));
        return false;
    }

    vkGetDeviceQueue(m_device, m_queue_families.graphics_family, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_queue_families.compute_family, 0, &m_compute_queue);
    vkGetDeviceQueue(m_device, m_queue_families.transfer_family, 0, &m_transfer_queue);

    return true;
}

bool RhiContext::init_vma() {
    VmaVulkanFunctions vma_functions{};
    vma_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vma_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
    allocator_info.physicalDevice = m_physical_device;
    allocator_info.device = m_device;
    allocator_info.instance = m_instance;
    allocator_info.pVulkanFunctions = &vma_functions;

    if (m_caps.buffer_device_address_supported) {
        allocator_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    VkResult res = vmaCreateAllocator(&allocator_info, &m_vma_allocator);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create VMA allocator: {}", static_cast<int>(res));
        return false;
    }

    return true;
}

} // namespace engine::rhi

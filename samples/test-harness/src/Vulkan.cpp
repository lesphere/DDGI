/*
* Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Graphics.h"
#include "VulkanExtensions.h"
#include "UI.h"
#include "ImageCapture.h"

namespace Graphics
{
    namespace Vulkan
    {

        const uint32_t maxSamplerDescriptorCount = 1024;
        const uint32_t maxUniformBufferDescriptorCount = 1024;
        const uint32_t maxAccelerationStructureDescriptorCount = 1024;
        const uint32_t maxStorageImageDescriptorCount = 2048;
        const uint32_t maxSampledImageDescriptorCount = 2048;
        const uint32_t maxCombinedImageSamplerDescriptorCount = 2048;
        const uint32_t maxStorageBufferDescriptorCount = 2048;
        const uint32_t maxDescriptorSets = 16;

        //----------------------------------------------------------------------------------------------------------
        // Private Helper Functions
        //----------------------------------------------------------------------------------------------------------

    #if _DEBUG
        VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverityFlags,
            VkDebugUtilsMessageTypeFlagsEXT messageTypeFlags,
            const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
            void* userData)
        {
        #if _WIN32
            OutputDebugString(callbackData->pMessage);
            OutputDebugString("\n");
        #elif __linux__
            // TODO: unix implementation
        #else
            #pragma_comment("Platform not supported!")
        #endif
            return VK_FALSE;
        }
    #endif

        void ConvertWideStringToNarrow(std::wstring& wide, std::string& narrow)
        {
            narrow.resize(wide.size());
        #if defined(_WIN32) || defined(WIN32)
            size_t converted = 0;
            wcstombs_s(&converted, narrow.data(), (narrow.size() + 1), wide.c_str(), wide.size());
        #else
            wcstombs(narrow.data(), wide.c_str(), narrow.size() + 1);
        #endif
        }

        bool Check(VkResult hr, std::string fileName, uint32_t lineNumber)
        {
            if(hr == VK_ERROR_OUT_OF_DATE_KHR) return false;    // window resized or destroyed
            if(hr != VK_SUCCESS)
            {
                std::string msg = "Vulkan call failed in:\n" + fileName + " at line " + std::to_string(lineNumber) + " where VkResult=" + std::to_string(hr);
                Graphics::UI::MessageBox(msg);
                return false;
            }
            return true;
        }

        /**
         * Check if the device supports required extensions.
         */
        bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char*>& requiredExtensions) {
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

            // Check if all required extensions are supported
            for (const char* extension : requiredExtensions) {
                bool found = false;
                for (const VkExtensionProperties& ext : availableExtensions) {
                    if (strcmp(extension, ext.extensionName) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    //std::cerr << "Extension " << extension << " not supported on this device." << std::endl;
                    return false;
                }
            }
            return true;
        }

        /**
         * Search a list of physical devices for one that supports a graphics queue and required extensions.
         */
        bool FindPhysicalDeviceWithGraphicsQueueAndExtensions(const std::vector<VkPhysicalDevice>& physicalDevices, VkPhysicalDevice* device, int* graphicsQueueIndex, const std::vector<const char*>& deviceExtensions)
        {
            for (uint32_t deviceIndex = 0; deviceIndex < static_cast<uint32_t>(physicalDevices.size()); deviceIndex++)
            {
                // Get the physical device
                const VkPhysicalDevice physicalDevice = physicalDevices[deviceIndex];

                // Get the number of properties
                uint32_t queueFamilyPropertyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);
                if (queueFamilyPropertyCount == 0) continue;

                // Get a list of the queue properties
                std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

                if (!checkDeviceExtensionSupport(physicalDevice, deviceExtensions)) continue;

                // Inspect the list of properties to see if the physical device supports graphics queues
                for (uint32_t propertyIndex = 0; propertyIndex < static_cast<uint32_t>(queueFamilyProperties.size()); propertyIndex++)
                {
                    const VkQueueFamilyProperties props = queueFamilyProperties[propertyIndex];
                    if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    {
                        if (device) *device = physicalDevice;
                        if (graphicsQueueIndex) *graphicsQueueIndex = propertyIndex;
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * Get the index of the memory type used for the requested memory.
         */
        uint32_t GetMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t memoryTypeBits, VkMemoryPropertyFlags memoryProperties)
        {
            // Get the physical device memory properties
            VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

            uint32_t memTypeIndex = 0;
            while (memTypeIndex < physicalDeviceMemoryProperties.memoryTypeCount)
            {
                // Check if the device has the proper memory type capabilities
                bool isRequiredType = memoryTypeBits & (1 << memTypeIndex);
                bool hasRequiredProperties = (physicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags & memoryProperties) == memoryProperties;
                if (isRequiredType && hasRequiredProperties) return memTypeIndex;
                ++memTypeIndex;
            }

            return ~0x0;
        }

        /**
         * Get the format and color space of the swap chain surfaces.
         */
        bool GetSwapChainFormatAndColorSpace(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkFormat* format, VkColorSpaceKHR* colorSpace)
        {
            // Get the number of formats the surface supports
            uint32_t surfaceFormatCount = 0;
            VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr));

            // Get the list of surface formats
            std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
            VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data()));

            if (surfaceFormatCount == 1 && surfaceFormats.front().format == VK_FORMAT_UNDEFINED)
            {
                *format = VK_FORMAT_R8G8B8A8_UNORM;
            }
            else
            {
                *format = surfaceFormats.front().format;
            }

            *colorSpace = surfaceFormats.front().colorSpace;

            return true;
        }

        /**
         * Get the device address of the given buffer.
         */
        VkDeviceAddress GetBufferDeviceAddress(VkDevice device, VkBuffer buffer)
        {
            VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo = {};
            bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddressInfo.buffer = buffer;
            return vkGetBufferDeviceAddressKHR(device, &bufferDeviceAddressInfo);
        }

        /**
         * Allocate memory.
         */
        bool AllocateMemory(Globals& vk, const AllocateMemoryDesc info, VkDeviceMemory* memory)
        {
            // Get the memory properties of the physical device
            VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
            vkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &physicalDeviceMemoryProperties);

            // Check to see if the device has the required memory
            uint32_t memTypeIndex = 0;
            while (memTypeIndex < physicalDeviceMemoryProperties.memoryTypeCount)
            {
                bool isRequiredType = info.requirements.memoryTypeBits & (1 << memTypeIndex);
                bool hasRequiredProperties = (physicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags & info.properties) == info.properties;
                if (isRequiredType && hasRequiredProperties) break;
                ++memTypeIndex;
            }

            // Early exit, memory type not found
            if (memTypeIndex == physicalDeviceMemoryProperties.memoryTypeCount) return false;

            // Describe the memory allocation
            VkMemoryAllocateFlagsInfo allocateFlagsInfo = {};
            allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocateFlagsInfo.flags = info.flags;

            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.pNext = &allocateFlagsInfo;
            memoryAllocateInfo.memoryTypeIndex = memTypeIndex;
            memoryAllocateInfo.allocationSize = info.requirements.size;

            // Allocate the device memory
            VKCHECK(vkAllocateMemory(vk.device, &memoryAllocateInfo, nullptr, memory));

            return true;
        }

        //----------------------------------------------------------------------------------------------------------
        // Private Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Create the Vulkan instance.
         */
        bool CreateInstance(Globals& vk)
        {
            // Check if Vulkan exists
            if(!glfwVulkanSupported()) return false;

            // Get the required extensions
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            // Specify all extensions
            // 0: VK_KHR_SURFACE_EXTENSION_NAME
            // 1: VK_KHR_WIN32_SURFACE_EXTENSION_NAME - Windows only
            // 1: VK_KHR_XCB_SURFACE_EXTENSION_NAME - Linux only
            // 2: VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
            // 3: VK_EXT_DEBUG_UTILS_EXTENSION_NAME
            std::vector<const char*> extensionNames(glfwExtensions, glfwExtensions + glfwExtensionCount);
            extensionNames.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        #if _DEBUG || GFX_NAME_OBJECTS
            extensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        #endif

            // Describe the instance
            VkInstanceCreateInfo instanceCreateInfo = {};
            instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instanceCreateInfo.ppEnabledExtensionNames = extensionNames.data();
            instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());

            std::vector<const char*> layerNames;

        #if _DEBUG
            // Enable the validation layer in debug
            layerNames.push_back("VK_LAYER_KHRONOS_validation");

            // Create the validation layer's message callback function
            VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
            debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            debugUtilsMessengerCreateInfo.pfnUserCallback = DebugUtilsMessengerCallback;

            instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
        #endif

            instanceCreateInfo.ppEnabledLayerNames = layerNames.data();
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(layerNames.size());

            // Describe the application
            VkApplicationInfo applicationInfo = {};
            applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            applicationInfo.apiVersion = VK_API_VERSION_1_2;
            applicationInfo.applicationVersion = VK_MAKE_VERSION(RTXGI_VERSION::major, RTXGI_VERSION::minor, RTXGI_VERSION::revision);
            applicationInfo.engineVersion = VK_MAKE_VERSION(RTXGI_VERSION::major, RTXGI_VERSION::minor, RTXGI_VERSION::revision);
            applicationInfo.pApplicationName = "RTXGI Test Harness";
            applicationInfo.pEngineName = "RTXGI Test Harness";

            instanceCreateInfo.pApplicationInfo = &applicationInfo;

            // Create the instance
            VKCHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &vk.instance));
            if (vk.instance == nullptr) return false;

            // Load the instance extensions
            LoadInstanceExtensions(vk.instance);

        #if _DEBUG
            VKCHECK(vkCreateDebugUtilsMessengerEXT(vk.instance, &debugUtilsMessengerCreateInfo, nullptr, &vk.debugUtilsMessenger));
        #endif

            return true;
        }

        /**
         * Create the Vulkan surface.
         */
        bool CreateSurface(Globals& vk)
        {
            // Create the surface with GLFW
            VKCHECK(glfwCreateWindowSurface(vk.instance, vk.window, nullptr, &vk.surface));
            return true;
        }

        /**
         * Create the Vulkan device and queue.
         */
        bool CreateDeviceInternal(Globals& vk, Configs::Config& config)
        {
            // Get the number of physical graphics devices
            uint32_t physicalDeviceCount = 0;
            VKCHECK(vkEnumeratePhysicalDevices(vk.instance, &physicalDeviceCount, nullptr));
            if (physicalDeviceCount == 0) return false;

            // Get the list of physical devices
            std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
            VKCHECK(vkEnumeratePhysicalDevices(vk.instance, &physicalDeviceCount, devices.data()));

            std::vector<const char*> deviceExtensions =
            {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                VK_KHR_RAY_QUERY_EXTENSION_NAME,
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
                VK_KHR_MAINTENANCE3_EXTENSION_NAME
            };

            // Find a physical device that supports graphics queues
            if (!FindPhysicalDeviceWithGraphicsQueueAndExtensions(devices, &vk.physicalDevice, &vk.queueFamilyIndex, deviceExtensions)) return false;

            // Describe the device queue
            VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
            deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            deviceQueueCreateInfo.queueCount = 1;
            deviceQueueCreateInfo.queueFamilyIndex = vk.queueFamilyIndex;

            static const float queuePriorities[] = { 1.f };
            deviceQueueCreateInfo.pQueuePriorities = queuePriorities;

            // Describe the device
            VkDeviceCreateInfo deviceCreateInfo = {};
            deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceCreateInfo.queueCreateInfoCount = 1;
            deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;

            std::vector<const char*> deviceLayerNames;

        #if _DEBUG
            deviceLayerNames.push_back("VK_LAYER_KHRONOS_validation");
        #endif

            deviceCreateInfo.ppEnabledLayerNames = deviceLayerNames.data();
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(deviceLayerNames.size());

            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
            deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());

            // Enable extension features
            VkPhysicalDeviceRobustness2FeaturesEXT robusness2Features = {};
            robusness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
            robusness2Features.pNext = nullptr;
            robusness2Features.nullDescriptor = VK_TRUE;    // allow null descriptors in descriptor sets

            VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAdressFeatures = {};
            bufferDeviceAdressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            bufferDeviceAdressFeatures.pNext = &robusness2Features;
            bufferDeviceAdressFeatures.bufferDeviceAddress = VK_TRUE;

            VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
            rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
            rayQueryFeatures.pNext = &bufferDeviceAdressFeatures;
            rayQueryFeatures.rayQuery = VK_TRUE;

            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.pNext = &rayQueryFeatures;
            accelerationStructureFeatures.accelerationStructure = VK_TRUE;
            accelerationStructureFeatures.accelerationStructureCaptureReplay = VK_FALSE;
            accelerationStructureFeatures.accelerationStructureIndirectBuild = VK_FALSE;
            accelerationStructureFeatures.accelerationStructureHostCommands = VK_FALSE;
            accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {};
            rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayTracingPipelineFeatures.pNext = &accelerationStructureFeatures;
            rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
            rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
            rayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
            rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
            rayTracingPipelineFeatures.rayTraversalPrimitiveCulling = VK_TRUE;

            VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
            descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            descriptorIndexingFeatures.pNext = &rayTracingPipelineFeatures;
            descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
            descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

            deviceCreateInfo.pNext = &descriptorIndexingFeatures;

            // Get the features supported by the physical device
            vkGetPhysicalDeviceFeatures(vk.physicalDevice, &vk.deviceFeatures);
            deviceCreateInfo.pEnabledFeatures = &vk.deviceFeatures;

            // Create the device
            VKCHECK(vkCreateDevice(vk.physicalDevice, &deviceCreateInfo, nullptr, &vk.device));
            if (vk.device == nullptr) return false;

            // Load the device extensions
            LoadDeviceExtensions(vk.device);

            // Create the queue
            vkGetDeviceQueue(vk.device, vk.queueFamilyIndex, 0, &vk.queue);
            if (vk.queue == nullptr) return false;

        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.device), "VKDevice", VK_OBJECT_TYPE_DEVICE);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.queue), "VKQueue", VK_OBJECT_TYPE_QUEUE);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.surface), "VKSurface", VK_OBJECT_TYPE_SURFACE_KHR);
        #endif

            // Get the properties of the device (include ray tracing properties)
            vk.deviceProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            vk.deviceASProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            vk.deviceRTPipelineProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            vk.deviceSubgroupProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

            vk.deviceASProps.pNext = &vk.deviceSubgroupProps;
            vk.deviceRTPipelineProps.pNext = &vk.deviceASProps;
            vk.deviceProps.pNext = &vk.deviceRTPipelineProps;

            vkGetPhysicalDeviceProperties2(vk.physicalDevice, &vk.deviceProps);

            vk.features.waveLaneCount = vk.deviceSubgroupProps.subgroupSize;

            // Set the graphics API name
            config.app.api = "Vulkan 1.2";

            // Save the GPU device name
            std::string name(vk.deviceProps.properties.deviceName);
            config.app.gpuName = name;

            return true;
        }

        /**
         * Create the fences.
         */
        bool CreateFences(Globals& vk)
        {
            VkFenceCreateInfo fenceCreateInfo = {};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (uint32_t fenceIndex = 0; fenceIndex < MAX_FRAMES_IN_FLIGHT; fenceIndex++)
            {
                VKCHECK(vkCreateFence(vk.device, &fenceCreateInfo, nullptr, &vk.fences[fenceIndex]));
            #ifdef GFX_NAME_OBJECTS
                std::string fenceName = "Fence " + std::to_string(fenceIndex);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.fences[fenceIndex]), fenceName.c_str(), VK_OBJECT_TYPE_FENCE);
            #endif
            }

            fenceCreateInfo.flags = 0;
            VKCHECK(vkCreateFence(vk.device, &fenceCreateInfo, nullptr, &vk.immediateFence));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.immediateFence), "Immediate Fence", VK_OBJECT_TYPE_FENCE);
        #endif

            return true;
        }

        /**
         * Create the swap chain.
         */
        bool CreateSwapChain(Globals& vk)
        {
            // Make sure the surface supports presentation
            VkBool32 presentSupported;
            VKCHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vk.physicalDevice, 0, vk.surface, &presentSupported));
            if (!presentSupported) return false;

            // Get the number of presentation modes of the surface
            uint32_t presentModeCount = 0;
            VKCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, nullptr));

            // Get the list of presentation modes of the surface
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            VKCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, presentModes.data()));

            // Get the capabilities of the surface
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physicalDevice, vk.surface, &surfaceCapabilities));

            // Describe the swap chain
            VkExtent2D swapchainSize = {};
            swapchainSize = surfaceCapabilities.currentExtent;
            if (swapchainSize.width != vk.width) return false;
            //if (swapchainSize.height != vk.height) return false;
            if (surfaceCapabilities.minImageCount > MAX_FRAMES_IN_FLIGHT) return false;

            // Note: maxImageCount of 0 means unlimited number of images
            assert((surfaceCapabilities.maxImageCount != 0) && (surfaceCapabilities.maxImageCount > MAX_FRAMES_IN_FLIGHT));

            VkSurfaceTransformFlagBitsKHR surfaceTransformFlagBits =
                surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfaceCapabilities.currentTransform;

            // Get the swap chain's format and color space
            // TODO: use B8G8R8A8Unorm and SrgbNonlinear?
            if (!GetSwapChainFormatAndColorSpace(vk.physicalDevice, vk.surface, &vk.swapChainFormat, &vk.swapChainColorSpace)) return false;

            // Describe the swap chain
            VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
            swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapchainCreateInfo.surface = vk.surface;
            swapchainCreateInfo.minImageCount = MAX_FRAMES_IN_FLIGHT;
            swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            swapchainCreateInfo.preTransform = surfaceTransformFlagBits;
            swapchainCreateInfo.imageColorSpace = vk.swapChainColorSpace;
            swapchainCreateInfo.imageFormat = vk.swapChainFormat;
            swapchainCreateInfo.pQueueFamilyIndices = nullptr;
            swapchainCreateInfo.queueFamilyIndexCount = 0;
            swapchainCreateInfo.clipped = VK_TRUE;
            swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
            swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swapchainCreateInfo.imageExtent = swapchainSize;
            swapchainCreateInfo.imageArrayLayers = 1;
            if(vk.vsync) swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            else swapchainCreateInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            {
                // Allow the back buffer to be a copy destination
                swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            }

            if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            {
                // Allow the back buffer to be a copy source
                swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }

            // Create the swap chain
            VKCHECK(vkCreateSwapchainKHR(vk.device, &swapchainCreateInfo, nullptr, &vk.swapChain));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.swapChain), "Swapchain", VK_OBJECT_TYPE_SWAPCHAIN_KHR);
        #endif

            // Get the swap chain image count
            uint32_t swapchainImageCount = 0;
            VKCHECK(vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &swapchainImageCount, nullptr));
            if (swapchainImageCount != MAX_FRAMES_IN_FLIGHT) return false;

            // Get the swap chain images
            VKCHECK(vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &swapchainImageCount, vk.swapChainImage));

            // Create views for the swap chain images
            for (uint32_t imageIndex = 0; imageIndex < MAX_FRAMES_IN_FLIGHT; imageIndex++)
            {
                // Describe the image view
                VkImageViewCreateInfo imageViewCreateInfo = {};
                imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                imageViewCreateInfo.image = vk.swapChainImage[imageIndex];
                imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                imageViewCreateInfo.format = vk.swapChainFormat;
                imageViewCreateInfo.subresourceRange.levelCount = 1;
                imageViewCreateInfo.subresourceRange.layerCount = 1;
                imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

                // Create the image view
                VKCHECK(vkCreateImageView(vk.device, &imageViewCreateInfo, nullptr, &vk.swapChainImageView[imageIndex]));

            #ifdef GFX_NAME_OBJECTS
                std::string imageName = "Back Buffer Image " + std::to_string(imageIndex);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.swapChainImage[imageIndex]), imageName.c_str(), VK_OBJECT_TYPE_IMAGE);

                std::string viewName = "Back Buffer Image View " + std::to_string(imageIndex);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.swapChainImageView[imageIndex]), viewName.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif
            }

            ImageBarrierDesc barrier =
            {
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
            };

            // Transition the back buffers to present
            for (uint32_t imageIndex = 0; imageIndex < MAX_FRAMES_IN_FLIGHT; imageIndex++)
            {
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], vk.swapChainImage[imageIndex], barrier);
            }

            return true;
        }

        /**
         * Create the render pass.
         */
        bool CreateRenderPass(Globals& vk)
        {
            // Describe the render pass
            VkAttachmentDescription attachmentDescriptions[1] = {};
            attachmentDescriptions[0].format = vk.swapChainFormat;
            attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentReference = {};
            colorAttachmentReference.attachment = 0;
            colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpassDescription = {};
            subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpassDescription.inputAttachmentCount = 0;
            subpassDescription.colorAttachmentCount = 1;
            subpassDescription.pColorAttachments = &colorAttachmentReference;

            VkRenderPassCreateInfo renderPassCreateInfo = {};
            renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassCreateInfo.attachmentCount = 1;
            renderPassCreateInfo.subpassCount = 1;
            renderPassCreateInfo.pSubpasses = &subpassDescription;
            renderPassCreateInfo.pAttachments = &attachmentDescriptions[0];

            // Create the render pass
            VKCHECK(vkCreateRenderPass(vk.device, &renderPassCreateInfo, nullptr, &vk.renderPass));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.renderPass), "Render Pass", VK_OBJECT_TYPE_RENDER_PASS);
        #endif
            return true;
        }

        /**
         * Create the frame buffers.
         */
        bool CreateFrameBuffers(Globals& vk)
        {
            for (uint32_t bufferIndex = 0; bufferIndex < MAX_FRAMES_IN_FLIGHT; bufferIndex++)
            {
                // Describe the frame buffer
                VkFramebufferCreateInfo framebufferCreateInfo = {};
                framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferCreateInfo.attachmentCount = 1;
                framebufferCreateInfo.pAttachments = vk.swapChainImageView + bufferIndex;
                framebufferCreateInfo.width = vk.width;
                framebufferCreateInfo.height = vk.height;
                framebufferCreateInfo.layers = 1;
                framebufferCreateInfo.renderPass = vk.renderPass;

                // Create the frame buffer
                VKCHECK(vkCreateFramebuffer(vk.device, &framebufferCreateInfo, nullptr, &vk.frameBuffer[bufferIndex]));
            #ifdef GFX_NAME_OBJECTS
                std::string name = "Frame Buffer " + std::to_string(bufferIndex);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.frameBuffer[bufferIndex]), name.c_str(), VK_OBJECT_TYPE_FRAMEBUFFER);
            #endif
            }

            return true;
        }

        /**
         * Create the command pool.
         */
        bool CreateCommandPool(Globals& vk)
        {
            // Describe the command pool
            VkCommandPoolCreateInfo commandPoolCreateInfo = {};
            commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolCreateInfo.queueFamilyIndex = vk.queueFamilyIndex;
            commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            // Create the command pool
            VKCHECK(vkCreateCommandPool(vk.device, &commandPoolCreateInfo, nullptr, &vk.commandPool));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.commandPool), "Command Pool", VK_OBJECT_TYPE_COMMAND_POOL);
        #endif
            return true;
        }

        /**
         * Create the command buffers.
         */
        bool CreateCommandBuffers(Globals& vk)
        {
            // Describe the command buffers
            VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
            commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
            commandBufferAllocateInfo.commandPool = vk.commandPool;
            commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            // Allocate the command buffers from the command pool
            std::vector<VkCommandBuffer> commandBuffers;
            commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            VKCHECK(vkAllocateCommandBuffers(vk.device, &commandBufferAllocateInfo, commandBuffers.data()));

            for (uint32_t index = 0; index < MAX_FRAMES_IN_FLIGHT; index++)
            {
                vk.cmdBuffer[index] = commandBuffers[index];
            #ifdef GFX_NAME_OBJECTS
                std::string name = "Command Buffer " + std::to_string(index);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.cmdBuffer[0]), name.c_str(), VK_OBJECT_TYPE_COMMAND_BUFFER);
            #endif
            }

            return true;
        }

        /**
         * Create the semaphores.
         */
        bool CreateSemaphores(Globals& vk)
        {
            VkSemaphoreCreateInfo semaphoreCreateInfo = {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreCreateInfo.flags = VK_SEMAPHORE_TYPE_BINARY;

            for (uint32_t semaphoreIndex = 0; semaphoreIndex < MAX_FRAMES_IN_FLIGHT; semaphoreIndex++)
            {
                VKCHECK(vkCreateSemaphore(vk.device, &semaphoreCreateInfo, nullptr, &vk.imageAcquiredSemaphore[semaphoreIndex]));
                VKCHECK(vkCreateSemaphore(vk.device, &semaphoreCreateInfo, nullptr, &vk.presentSemaphore[semaphoreIndex]));

            #ifdef GFX_NAME_OBJECTS
                std::string imageSemaphoreName = "Image Acquired Semaphore " + std::to_string(semaphoreIndex);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.imageAcquiredSemaphore[semaphoreIndex]), imageSemaphoreName.c_str(), VK_OBJECT_TYPE_SEMAPHORE);

                std::string presentSemaphoreName = "Present Semaphore " + std::to_string(semaphoreIndex);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(vk.presentSemaphore[semaphoreIndex]), presentSemaphoreName.c_str(), VK_OBJECT_TYPE_SEMAPHORE);
            #endif
            }
            return true;
        }

        /**
         * Create the descriptor pool.
         */
        bool CreateDescriptorPool(Globals& vk, Resources& resources)
        {
            // Describe the descriptor pool sizes
            VkDescriptorPoolSize descriptorPoolSizes[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, maxSamplerDescriptorCount },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxUniformBufferDescriptorCount },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxStorageImageDescriptorCount },
                { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, maxAccelerationStructureDescriptorCount },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxSampledImageDescriptorCount },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxCombinedImageSamplerDescriptorCount },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxStorageBufferDescriptorCount }
            };

            // Describe the descriptor pool
            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
            descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolCreateInfo.poolSizeCount = _countof(descriptorPoolSizes);
            descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
            descriptorPoolCreateInfo.maxSets = maxDescriptorSets;

            // Create the descriptor pool
            VKCHECK(vkCreateDescriptorPool(vk.device, &descriptorPoolCreateInfo, nullptr, &resources.descriptorPool));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorPool), "Descriptor Pool", VK_OBJECT_TYPE_DESCRIPTOR_POOL);
        #endif
            return true;
        }

        /**
         * Create the query pool(s).
         */
        bool CreateQueryPools(Globals& vk, Resources& resources)
        {
            // Describe the timestamp query pool
            VkQueryPoolCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            createInfo.queryCount = MAX_TIMESTAMPS * 2;

            // Create the timestamp query pool
            VKCHECK(vkCreateQueryPool(vk.device, &createInfo, nullptr, &resources.timestampPool));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.timestampPool), "Timestamp Query Pool", VK_OBJECT_TYPE_QUERY_POOL);
        #endif

            // Reset the queries in the pool
            vkCmdResetQueryPool(vk.cmdBuffer[vk.frameIndex], resources.timestampPool, 0, MAX_TIMESTAMPS * 2);

            // Create the timestamps resource (read-back)
            uint32_t size = MAX_TIMESTAMPS * sizeof(uint64_t) * 2;
            BufferDesc desc = { size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.timestamps, &resources.timestampsMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.timestamps), "Timestamp Query Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.timestampsMemory), "Timestamp Query Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            return true;
        }

        /**
         * Create the raster viewport.
         */
        bool CreateViewport(Globals& vk)
        {
            vk.viewport.x = 0;
            vk.viewport.y = 0;
            vk.viewport.width = static_cast<float>(vk.width);
            vk.viewport.height = static_cast<float>(vk.height);
            vk.viewport.minDepth = 0;
            vk.viewport.maxDepth = 1;
            return true;
        }

        /**
         * Create the raster scissor.
         */
        bool CreateScissor(Globals& vk)
        {
            vk.scissor.extent.width = vk.width;
            vk.scissor.extent.height = vk.height;
            vk.scissor.offset.x = 0;
            vk.scissor.offset.y = 0;
            return true;
        }

        /**
         * Create the samplers.
         */
        bool CreateSamplers(Globals& vk, Resources& resources)
        {
            // Describe a bilinear sampler
            VkSamplerCreateInfo samplerCreateInfo = {};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerCreateInfo.compareEnable = VK_FALSE;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerCreateInfo.minLod = 0.f;
            samplerCreateInfo.maxLod = FLT_MAX;

            // Create the bilinear sampler
            resources.samplers.push_back(VkSampler{});
            VKCHECK(vkCreateSampler(vk.device, &samplerCreateInfo, nullptr, &resources.samplers.back()));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.samplers.back()), "Bilinear Wrap Sampler", VK_OBJECT_TYPE_SAMPLER);
        #endif

            // Describe a point sampler
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
            samplerCreateInfo.minFilter = VK_FILTER_NEAREST;

            // Create the point sampler
            resources.samplers.push_back(VkSampler{});
            VKCHECK(vkCreateSampler(vk.device, &samplerCreateInfo, nullptr, &resources.samplers.back()));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.samplers.back()), "Point Clamp Sampler", VK_OBJECT_TYPE_SAMPLER);
        #endif

            // Describe an anisotropic (wrap) sampler
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerCreateInfo.anisotropyEnable = VK_TRUE;
            samplerCreateInfo.maxAnisotropy = vk.deviceProps.properties.limits.maxSamplerAnisotropy;

            // Create the aniso sampler
            resources.samplers.push_back(VkSampler{});
            VKCHECK(vkCreateSampler(vk.device, &samplerCreateInfo, nullptr, &resources.samplers.back()));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.samplers.back()), "Aniso Wrap Sampler", VK_OBJECT_TYPE_SAMPLER);
        #endif

            return true;
        }

        /**
         * Create the index buffer and device memory for a mesh.
         * Copy the index data to the upload buffer and schedule a copy to the device buffer.
         */
        bool CreateIndexBuffer(Globals& vk, const Scenes::Mesh& mesh, VkBuffer* ib, VkDeviceMemory* ibMemory, VkBuffer* ibUpload, VkDeviceMemory* ibUploadMemory)
        {
            // Create the index buffer upload resource
            uint32_t sizeInBytes = mesh.numIndices * sizeof(uint32_t);
            BufferDesc desc = { sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, ibUpload, ibUploadMemory)) return false;

            // Create the index buffer device resource
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, ib, ibMemory)) return false;

            // Copy the index data of each mesh primitive to the upload buffer
            uint8_t* pData = nullptr;
            VKCHECK(vkMapMemory(vk.device, *ibUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));

            for (uint32_t primitiveIndex = 0; primitiveIndex < static_cast<uint32_t>(mesh.primitives.size()); primitiveIndex++)
            {
                // Get the mesh primitive and copy its indices to the upload buffer
                const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                uint32_t size = static_cast<uint32_t>(primitive.indices.size()) * sizeof(uint32_t);
                memcpy(pData + primitive.indexByteOffset, primitive.indices.data(), size);
            }
            vkUnmapMemory(vk.device, *ibUploadMemory);

            // Schedule a copy of the upload buffer to the device buffer
            VkBufferCopy bufferCopy = {};
            bufferCopy.size = sizeInBytes;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], *ibUpload, *ib, 1, &bufferCopy);

            return true;
        }

        /**
         * Create the vertex buffer and device memory for a mesh primitive.
         * Copy the vertex data to the upload buffer and schedule a copy to the device buffer.
         */
        bool CreateVertexBuffer(Globals& vk, const Scenes::Mesh& mesh, VkBuffer* vb, VkDeviceMemory* vbMemory, VkBuffer* vbUpload, VkDeviceMemory* vbUploadMemory)
        {
            // Create the vertex buffer upload resource
            uint32_t stride = sizeof(Vertex);
            uint32_t sizeInBytes = mesh.numVertices * stride;
            BufferDesc desc = { sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, vbUpload, vbUploadMemory)) return false;

            // Create the vertex buffer device resource
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, vb, vbMemory)) return false;

            // Copy the vertex data of each mesh primitive to the upload
            uint8_t* pData = nullptr;
            VKCHECK(vkMapMemory(vk.device, *vbUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));

            for (uint32_t primitiveIndex = 0; primitiveIndex < static_cast<uint32_t>(mesh.primitives.size()); primitiveIndex++)
            {
                // Get the mesh primitive and copy its vertices to the upload buffer
                const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                uint32_t size = static_cast<uint32_t>(primitive.vertices.size()) * stride;
                memcpy(pData + primitive.vertexByteOffset, primitive.vertices.data(), size);
            }
            vkUnmapMemory(vk.device, *vbUploadMemory);

            // Schedule a copy of the upload buffer to the device buffer
            VkBufferCopy bufferCopy = {};
            bufferCopy.size = sizeInBytes;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], *vbUpload, *vb, 1, &bufferCopy);

            return true;
        }

        /**
         * Create a bottom level acceleration structure and device memory for a mesh primitive.
         * Allocate scratch memory and schedule a GPU BLAS build.
         */
        bool CreateBLAS(Globals& vk, Resources& resources, const Scenes::Mesh& mesh, AccelerationStructure& as)
        {
            uint32_t numPrimitives = static_cast<uint32_t>(mesh.primitives.size());

            // Describe the mesh primitives
            std::vector<VkAccelerationStructureGeometryKHR> primitives(numPrimitives);
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges(numPrimitives);
            std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos(numPrimitives);
            std::vector<uint32_t> primitiveCounts(numPrimitives);

            for (uint32_t primitiveIndex = 0; primitiveIndex < numPrimitives; primitiveIndex++)
            {
                // Get the mesh primitive
                const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                VkAccelerationStructureGeometryKHR desc = {};
                desc.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                desc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                desc.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

                desc.geometry.triangles.vertexData = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.sceneVBs[mesh.index]) + primitive.vertexByteOffset };
                desc.geometry.triangles.vertexStride = sizeof(Vertex);
                desc.geometry.triangles.maxVertex = static_cast<uint32_t>(primitive.vertices.size());
                desc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                desc.geometry.triangles.indexData = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.sceneIBs[mesh.index]) + primitive.indexByteOffset };
                desc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                desc.flags = primitive.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

                uint32_t primitiveCount = static_cast<uint32_t>(primitive.indices.size() / 3);

                // Describe the geometry for the builder
                VkAccelerationStructureBuildRangeInfoKHR buildRange = { primitiveCount, 0, 0, 0 };
                buildRanges[primitiveIndex] = buildRange;
                buildRangeInfos[primitiveIndex] = &buildRanges[primitiveIndex];

                primitives[primitiveIndex] = desc;
                primitiveCounts[primitiveIndex] = primitiveCount;
            }

            VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

            // Describe the bottom level acceleration structure inputs
            VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
            asInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            asInputs.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            asInputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            asInputs.geometryCount = static_cast<uint32_t>(primitives.size());
            asInputs.pGeometries = primitives.data();
            asInputs.flags = buildFlags;

            // Get the size requirements for the BLAS buffer
            VkAccelerationStructureBuildSizesInfoKHR asPreBuildInfo = {};
            asPreBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asInputs, primitiveCounts.data(), &asPreBuildInfo);

            // Create the BLAS scratch buffer, allocate and bind device memory
            BufferDesc blasScratchDesc = { asPreBuildInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
            if (!CreateBuffer(vk, blasScratchDesc, &as.scratch, &as.scratchMemory)) return false;
            asInputs.scratchData = VkDeviceOrHostAddressKHR{ GetBufferDeviceAddress(vk.device, as.scratch) };

            // Create the BLAS buffer, allocate and bind device memory
            BufferDesc blasDesc = { asPreBuildInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
            if (!CreateBuffer(vk, blasDesc, &as.asBuffer, &as.asMemory)) return false;

            // Describe the BLAS acceleration structure
            VkAccelerationStructureCreateInfoKHR asCreateInfo = {};
            asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            asCreateInfo.size = asPreBuildInfo.accelerationStructureSize;
            asCreateInfo.buffer = as.asBuffer;

            // Create the BLAS acceleration structure
            VKCHECK(vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &as.asKHR));

            // Set the location of the final acceleration structure
            asInputs.dstAccelerationStructure = as.asKHR;

            vkCmdBuildAccelerationStructuresKHR(vk.cmdBuffer[vk.frameIndex], 1, &asInputs, buildRangeInfos.data());

            return true;
        }

        /**
         * Create a top level acceleration structure.
         * Allocate scratch memory and schedule a GPU TLAS build.
         */
        bool CreateTLAS(Globals& vk, const std::vector<VkAccelerationStructureInstanceKHR>& instances, AccelerationStructure& as)
        {
            VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

            // Describe the TLAS geometry instances
            VkAccelerationStructureGeometryKHR geometries = {};
            geometries.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometries.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            geometries.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            geometries.geometry.instances.arrayOfPointers = VK_FALSE;
            geometries.geometry.instances.data = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, as.instances) };

            // Describe the top level acceleration structure inputs
            VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
            asInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            asInputs.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            asInputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            asInputs.geometryCount = 1;
            asInputs.pGeometries = &geometries;
            asInputs.flags = buildFlags;

            // Get the size requirements for the TLAS buffer
            uint32_t primitiveCount = static_cast<uint32_t>(instances.size());
            VkAccelerationStructureBuildSizesInfoKHR asPreBuildInfo = {};
            asPreBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asInputs, &primitiveCount, &asPreBuildInfo);

            // Create the TLAS scratch buffer, allocate and bind device memory
            BufferDesc scratchDesc = { asPreBuildInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
            if (!CreateBuffer(vk, scratchDesc, &as.scratch, &as.scratchMemory)) return false;
            asInputs.scratchData = VkDeviceOrHostAddressKHR{ GetBufferDeviceAddress(vk.device, as.scratch) };

            // Create the acceleration structure buffer, allocate and bind device memory
            BufferDesc desc = { asPreBuildInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
            if (!CreateBuffer(vk, desc, &as.asBuffer, &as.asMemory)) return false;

            // Describe the TLAS
            VkAccelerationStructureCreateInfoKHR asCreateInfo = {};
            asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            asCreateInfo.size = asPreBuildInfo.accelerationStructureSize;
            asCreateInfo.buffer = as.asBuffer;

            // Create the TLAS
            VKCHECK(vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &as.asKHR));

            // Set the location of the final acceleration structure
            asInputs.dstAccelerationStructure = as.asKHR;

            // Describe and build the BLAS
            std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos(1);
            VkAccelerationStructureBuildRangeInfoKHR buildInfo = { primitiveCount, 0, 0, 0 };
            buildRangeInfos[0] = &buildInfo;

            vkCmdBuildAccelerationStructuresKHR(vk.cmdBuffer[vk.frameIndex], 1, &asInputs, buildRangeInfos.data());

            // Wait for the TLAS build to complete
            VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            vkCmdPipelineBarrier(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

            return true;
        }

        /**
         * Create GPU heap resources, upload the texture, and schedule a copy from the GPU upload to default heap.
         */
        bool CreateAndUploadTexture(Globals& vk, Resources& resources, const Textures::Texture& texture, std::ofstream& log)
        {
            std::vector<VkImage>* textures;
            std::vector<VkDeviceMemory>* textureMemory;
            std::vector<VkImageView>* textureViews;
            std::vector<VkBuffer>* uploadBuffers;
            std::vector<VkDeviceMemory>* uploadBufferMemory;
            if (texture.type == Textures::ETextureType::SCENE)
            {
                textures = &resources.sceneTextures;
                textureMemory = &resources.sceneTextureMemory;
                textureViews = &resources.sceneTextureViews;
                uploadBuffers = &resources.sceneTextureUploadBuffer;
                uploadBufferMemory = &resources.sceneTextureUploadMemory;
            }
            else if (texture.type == Textures::ETextureType::ENGINE)
            {
                textures = &resources.textures;
                textureMemory = &resources.textureMemory;
                textureViews = &resources.textureViews;
                uploadBuffers = &resources.textureUploadBuffer;
                uploadBufferMemory = &resources.textureUploadMemory;
            }

            VkImage& resource = textures->emplace_back();
            VkDeviceMemory& resourceMemory = textureMemory->emplace_back();
            VkImageView& resourceView = textureViews->emplace_back();

            VkBuffer& upload = uploadBuffers->emplace_back();
            VkDeviceMemory& uploadMemory = uploadBufferMemory->emplace_back();

            // Create the device texture resource, memory, and view
            {
                TextureDesc desc = { texture.width, texture.height, 1, texture.mips, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
                if (texture.format == Textures::ETextureFormat::BC7) desc.format = VK_FORMAT_BC7_UNORM_BLOCK;
                CHECK(CreateTexture(vk, desc, &resource, &resourceMemory, &resourceView), "create the texture buffer, memory, and view!", log);
            #ifdef GFX_NAME_OBJECTS
                std::string name = "Texture: " + texture.name;
                std::string memory = "Texture Memory: " + texture.name;
                std::string view = "Texture View: " + texture.name;
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resource), name.c_str(), VK_OBJECT_TYPE_IMAGE);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resourceMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resourceView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif
            }

            // Create the upload heap buffer resource
            {
                BufferDesc desc = { texture.texelBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &upload, &uploadMemory), "create the texture upload buffer and memory!", log);
            #ifdef GFX_NAME_OBJECTS
                std::string name = " Texture Upload Buffer: " + texture.name;
                std::string memory = " Texture Upload Memory: " + texture.name;
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(upload), name.c_str(), VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(uploadMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif
            }

            // Copy the texel data to the upload buffer resource
            {
                uint8_t* pData = nullptr;
                VKCHECK(vkMapMemory(vk.device, uploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));

                if (texture.format == Textures::ETextureFormat::BC7)
                {
                    // Aligned, copy all the image pixels
                    memcpy(pData, texture.texels, texture.texelBytes);
                }
                else if (texture.format == Textures::ETextureFormat::UNCOMPRESSED)
                {
                    uint32_t rowSize = texture.width * texture.stride;
                    uint32_t rowPitch = ALIGN(256, rowSize);
                    if (rowSize == rowPitch)
                    {
                        // Aligned, copy the all image pixels
                        memcpy(pData, texture.texels, texture.texelBytes);
                    }
                    else
                    {
                        // RowSize is *not* aligned to 256B
                        // Copy each row of the image and add padding to match the row pitch alignment
                        uint8_t* pSource = texture.texels;
                        for (uint32_t rowIndex = 0; rowIndex < texture.height; rowIndex++)
                        {
                            memcpy(pData, texture.texels, rowSize);
                            pData += rowPitch;
                            pSource += rowSize;
                        }
                    }
                }

                vkUnmapMemory(vk.device, uploadMemory);
            }

            // Schedule a copy the of the upload resource to the device resource, then transition it to a shader resource
            {
                // Transition the device texture to be a copy destination
                VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, texture.mips, 0, 1 };
                ImageBarrierDesc before = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, range };
                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], resource, before);

                // Describe the buffer to image copy
                // Copy each texture mip level from the upload heap to default heap
                uint64_t offset = 0;
                std::vector<VkBufferImageCopy> bufferImageCopies;
                for (uint32_t mipIndex = 0; mipIndex < texture.mips; mipIndex++)
                {
                    uint32_t divisor = static_cast<uint32_t>(powf(2.f, (float)mipIndex));
                    uint32_t mipExtent = texture.width / divisor;
                    uint32_t mipDimension = std::max((uint32_t)4, mipExtent);

                    // Describe the mip level to copy
                    VkBufferImageCopy mipBufferImageCopy = {};
                    mipBufferImageCopy.bufferOffset = offset;
                    mipBufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    mipBufferImageCopy.imageSubresource.layerCount = 1;
                    mipBufferImageCopy.imageSubresource.mipLevel = mipIndex;

                    // Describe the mip image footprint
                    mipBufferImageCopy.imageExtent.width = mipExtent;
                    mipBufferImageCopy.imageExtent.height = mipExtent;
                    mipBufferImageCopy.imageExtent.depth = 1;
                    mipBufferImageCopy.bufferRowLength = std::max((uint32_t)64, mipExtent);

                    bufferImageCopies.push_back(mipBufferImageCopy);

                    if (texture.mips > 1)
                    {
                        assert(texture.format == Textures::ETextureFormat::BC7);
                        offset += Textures::GetBC7TextureSizeInBytes(mipDimension, mipDimension);
                    }
                }

                // Schedule a copy of the upload buffer to the device image buffer
                vkCmdCopyBufferToImage(vk.cmdBuffer[vk.frameIndex], upload, resource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferImageCopies.size()), bufferImageCopies.data());

                // Transition the device texture for reading in a shader
                ImageBarrierDesc after = { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, range };
                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], resource, after);
            }

            return true;
        }

        /**
         * Create the global (bindless) pipeline layout.
         */
        bool CreateGlobalPipelineLayout(Globals& vk, Resources& resources)
        {
            // Describe the global descriptor set layout bindings (aligns with Descriptors.hlsl)
            std::vector<VkDescriptorSetLayoutBinding> bindings;

            // 0: Samplers
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::SAMPLERS;
                bind.descriptorCount = maxSamplerDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                bindings.push_back(bind);
            }

            // 1: Camera Constant Buffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::CB_CAMERA;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                bindings.push_back(bind);
            }

            // 2: Lights StructuredBuffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::STB_LIGHTS;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 3: Materials StructuredBuffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::STB_MATERIALS;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                bindings.push_back(bind);
            }

            // 4: Scene TLAS Instances StructuredBuffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::STB_TLAS_INSTANCES;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 5: DDGIVolume Constants StructuredBuffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::STB_DDGI_VOLUME_CONSTS;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 6: DDGIVolume Bindless Resource Indices StructuredBuffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::STB_DDGI_VOLUME_RESOURCE_INDICES;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 7: Probe Vis TLAS Instances RWStructuredBuffer
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::UAV_STB_TLAS_INSTANCES;
                bind.descriptorCount = 1;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 8: Bindless UAVs, RWTexture2D
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::UAV_TEX2D;
                bind.descriptorCount = maxStorageImageDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 9: Bindless UAVs, RWTexture2DArray
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::UAV_TEX2DARRAY;
                bind.descriptorCount = maxStorageImageDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                bindings.push_back(bind);
            }

            // 10: Bindless SRVs, Ray Tracing Acceleration Structures (TLAS)
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::SRV_TLAS;
                bind.descriptorCount = maxAccelerationStructureDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                bind.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;    // not allowing tracing in hit shaders (i.e. recursive tracing)

                bindings.push_back(bind);
            }

            // 11: Bindless SRVs, Texture2D
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::SRV_TEX2D;
                bind.descriptorCount = maxSampledImageDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                bindings.push_back(bind);
            }

            // 12: Bindless SRVS, Texture2DArrays
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::SRV_TEX2DARRAY;
                bind.descriptorCount = maxSampledImageDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                bindings.push_back(bind);
            }

            // 13: Bindless SRVs, ByteAddressBuffers
            {
                VkDescriptorSetLayoutBinding bind = {};
                bind.binding = DescriptorLayoutBindings::SRV_BYTEADDRESS;
                bind.descriptorCount = maxStorageBufferDescriptorCount;
                bind.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                bindings.push_back(bind);
            }

            // Specify the descriptor binding flags for each binding
            VkDescriptorBindingFlags bindingFlags[] =
            {
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 0: Samplers[]
                0, // 1: Camera Constant Buffer
                0, // 2: Lights StructuredBuffer
                0, // 3: Materials StructuredBuffer
                0, // 4: TLASInstances StructuredBuffer
                0, // 5: DDGIVolume Constants StructuredBuffer
                0, // 6: DDGIVolume Resource Indices StructuredBuffer
                0, // 7: RWTLASInstances StructuredBuffer
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, //  8: RWTex2D[]
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, //  9: RWTex2DArray[]
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 10: TLAS[]
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 11: Tex2D[]
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 12: Tex2DArray[]
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 13: ByteAddrBuffer[]
            };
            assert(_countof(bindingFlags) == bindings.size()); // must have 1 binding flag per binding slot

            // Describe the descriptor bindings
            VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingsCreateInfo = {};
            descriptorSetLayoutBindingsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            descriptorSetLayoutBindingsCreateInfo.pBindingFlags = bindingFlags;
            descriptorSetLayoutBindingsCreateInfo.bindingCount = _countof(bindingFlags);

            // Describe the global descriptor set layout
            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingsCreateInfo;
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            descriptorSetLayoutCreateInfo.pBindings = bindings.data();

            // Create the descriptor set layout
            VKCHECK(vkCreateDescriptorSetLayout(vk.device, &descriptorSetLayoutCreateInfo, nullptr, &resources.descriptorSetLayout));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSetLayout), "Global Descriptor Set Layout", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
        #endif

            // Ranges in the push constants memory block
            std::vector<VkPushConstantRange> ranges;

            // Global Constants
            {
                VkPushConstantRange range = {};
                range.stageFlags = VK_SHADER_STAGE_ALL;
                range.offset = 0;
                range.size = GlobalConstants::GetAlignedSizeInBytes() + DDGIRootConstants::GetAlignedSizeInBytes();
                ranges.push_back(range);
            }

            // Describe the pipeline layout
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pSetLayouts = &resources.descriptorSetLayout;
            pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
            pipelineLayoutCreateInfo.pPushConstantRanges = ranges.data();

            // Create the pipeline layout
            VKCHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutCreateInfo, nullptr, &resources.pipelineLayout));
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.pipelineLayout), "Global Pipeline Layout", VK_OBJECT_TYPE_PIPELINE_LAYOUT);
        #endif

            return true;
        }

        /**
         * Create the shared render targets.
         */
        bool CreateRenderTargets(Globals& vk, Resources& resources)
        {
            // Create the GBufferA (R8G8B8A8_UNORM) texture resource
            TextureDesc desc = { static_cast<uint32_t>(vk.width), static_cast<uint32_t>(vk.height), 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
            if(!CreateTexture(vk, desc, &resources.rt.GBufferA, &resources.rt.GBufferAMemory, &resources.rt.GBufferAView)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferA), "GBufferA", VK_OBJECT_TYPE_IMAGE);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferAMemory), "GBufferA Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferAView), "GBufferA View", VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif

            // Create the GBufferB (R32G32B32A32_FLOAT) texture resource
            desc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            if (!CreateTexture(vk, desc, &resources.rt.GBufferB, &resources.rt.GBufferBMemory, &resources.rt.GBufferBView)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferB), "GBufferB", VK_OBJECT_TYPE_IMAGE);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferBMemory), "GBufferB Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferBView), "GBufferB View", VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif

            // Create the GBufferC (R32G32B32A32_FLOAT) texture resource
            if (!CreateTexture(vk, desc, &resources.rt.GBufferC, &resources.rt.GBufferCMemory, &resources.rt.GBufferCView)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferC), "GBufferC", VK_OBJECT_TYPE_IMAGE);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferCMemory), "GBufferC Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferCView), "GBufferC View", VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif

            // Create the GBufferD (R32G32B32A32_FLOAT) texture resource
            if (!CreateTexture(vk, desc, &resources.rt.GBufferD, &resources.rt.GBufferDMemory, &resources.rt.GBufferDView)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferD), "GBufferD", VK_OBJECT_TYPE_IMAGE);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferDMemory), "GBufferD Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rt.GBufferDView), "GBufferD View", VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif

            ImageBarrierDesc barrier =
            {
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
            };

            // Transition GBuffer resources for general use
            SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.rt.GBufferA, barrier);
            SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.rt.GBufferB, barrier);
            SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.rt.GBufferC, barrier);
            SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.rt.GBufferD, barrier);

            return true;
        }

        /**
         * Destroy the existing GBuffer resources.
         */
        void CleanupGBuffer(Globals& vk, Resources& resources)
        {
            vkDestroyImageView(vk.device, resources.rt.GBufferAView, nullptr);
            vkFreeMemory(vk.device, resources.rt.GBufferAMemory, nullptr);
            vkDestroyImage(vk.device, resources.rt.GBufferA, nullptr);

            vkDestroyImageView(vk.device, resources.rt.GBufferBView, nullptr);
            vkFreeMemory(vk.device, resources.rt.GBufferBMemory, nullptr);
            vkDestroyImage(vk.device, resources.rt.GBufferB, nullptr);

            vkDestroyImageView(vk.device, resources.rt.GBufferCView, nullptr);
            vkFreeMemory(vk.device, resources.rt.GBufferCMemory, nullptr);
            vkDestroyImage(vk.device, resources.rt.GBufferC, nullptr);

            vkDestroyImageView(vk.device, resources.rt.GBufferDView, nullptr);
            vkFreeMemory(vk.device, resources.rt.GBufferDMemory, nullptr);
            vkDestroyImage(vk.device, resources.rt.GBufferD, nullptr);
        }

        /**
         * Destroy the existing swapchain and associated resources.
         */
        void CleanupSwapchain(Globals& vk)
        {
            for (uint32_t resourceIndex = 0; resourceIndex < MAX_FRAMES_IN_FLIGHT; resourceIndex++)
            {
                vkDestroyFramebuffer(vk.device, vk.frameBuffer[resourceIndex], nullptr);
                vkDestroyImageView(vk.device, vk.swapChainImageView[resourceIndex], nullptr);
            }
            vkDestroySwapchainKHR(vk.device, vk.swapChain, nullptr);
        }

        /**
         * Release Vulkan resources.
         */
        void Cleanup(VkDevice& device, Resources& resources)
        {
            // Buffers
            if (resources.cameraCBMemory) vkUnmapMemory(device, resources.cameraCBMemory);
            if (resources.lightsSTBUploadMemory) vkUnmapMemory(device, resources.lightsSTBUploadMemory);

            vkDestroyBuffer(device, resources.cameraCB, nullptr);
            vkFreeMemory(device, resources.cameraCBMemory, nullptr);

            vkDestroyBuffer(device, resources.lightsSTB, nullptr);
            vkFreeMemory(device, resources.lightsSTBMemory, nullptr);
            vkDestroyBuffer(device, resources.lightsSTBUploadBuffer, nullptr);
            vkFreeMemory(device, resources.lightsSTBUploadMemory, nullptr);

            vkDestroyBuffer(device, resources.materialsSTB, nullptr);
            vkFreeMemory(device, resources.materialsSTBMemory, nullptr);
            vkDestroyBuffer(device, resources.meshOffsetsRB, nullptr);
            vkFreeMemory(device, resources.meshOffsetsRBMemory, nullptr);
            vkDestroyBuffer(device, resources.geometryDataRB, nullptr);
            vkFreeMemory(device, resources.geometryDataRBMemory, nullptr);
            resources.cameraCBPtr = nullptr;
            resources.lightsSTBPtr = nullptr;
            resources.materialsSTBPtr = nullptr;
            resources.meshOffsetsRBPtr = nullptr;
            resources.geometryDataRBPtr = nullptr;

            // Render Targets
            vkDestroyImageView(device, resources.rt.GBufferAView, nullptr);
            vkFreeMemory(device, resources.rt.GBufferAMemory, nullptr);
            vkDestroyImage(device, resources.rt.GBufferA, nullptr);

            vkDestroyImageView(device, resources.rt.GBufferBView, nullptr);
            vkFreeMemory(device, resources.rt.GBufferBMemory, nullptr);
            vkDestroyImage(device, resources.rt.GBufferB, nullptr);

            vkDestroyImageView(device, resources.rt.GBufferCView, nullptr);
            vkFreeMemory(device, resources.rt.GBufferCMemory, nullptr);
            vkDestroyImage(device, resources.rt.GBufferC, nullptr);

            vkDestroyImageView(device, resources.rt.GBufferDView, nullptr);
            vkFreeMemory(device, resources.rt.GBufferDMemory, nullptr);
            vkDestroyImage(device, resources.rt.GBufferD, nullptr);

            // Render Target Aliases
            resources.rt.RTAOOutputView = nullptr;

            // Release Scene geometry
            size_t resourceIndex;
            assert(resources.sceneIBs.size() == resources.sceneVBs.size());
            for (resourceIndex = 0; resourceIndex < resources.sceneIBs.size(); resourceIndex++)
            {
                vkDestroyBuffer(device, resources.sceneIBs[resourceIndex], nullptr);
                vkFreeMemory(device, resources.sceneIBMemory[resourceIndex], nullptr);
                vkDestroyBuffer(device, resources.sceneVBs[resourceIndex], nullptr);
                vkFreeMemory(device, resources.sceneVBMemory[resourceIndex], nullptr);
            }
            resources.sceneIBs.clear();
            resources.sceneIBMemory.clear();
            resources.sceneVBs.clear();
            resources.sceneVBMemory.clear();

            // Release Scene acceleration structures
            for (resourceIndex = 0; resourceIndex < resources.blas.size(); resourceIndex++)
            {
                resources.blas[resourceIndex].Release(device);
            }
            resources.tlas.Release(device);

            // Release Scene textures and related resources
            for (resourceIndex = 0; resourceIndex < resources.sceneTextures.size(); resourceIndex++)
            {
                vkDestroyImage(device, resources.sceneTextures[resourceIndex], nullptr);
                vkFreeMemory(device, resources.sceneTextureMemory[resourceIndex], nullptr);
                vkDestroyImageView(device, resources.sceneTextureViews[resourceIndex], nullptr);
            }

            // Release default textures and related resources
            for (resourceIndex = 0; resourceIndex < resources.textures.size(); resourceIndex++)
            {
                vkDestroyImage(device, resources.textures[resourceIndex], nullptr);
                vkFreeMemory(device, resources.textureMemory[resourceIndex], nullptr);
                vkDestroyImageView(device, resources.textureViews[resourceIndex], nullptr);
            }

            // Release the samplers
            for (resourceIndex = 0; resourceIndex < resources.samplers.size(); resourceIndex++)
            {
                vkDestroySampler(device, resources.samplers[resourceIndex], nullptr);
            }

            // Release the pipeline layout
            vkDestroyPipelineLayout(device, resources.pipelineLayout, nullptr);

            // Release the timestamp query resources
            vkFreeMemory(device, resources.timestampsMemory, nullptr);
            vkDestroyBuffer(device, resources.timestamps, nullptr);
            vkDestroyQueryPool(device, resources.timestampPool, nullptr);

            // Release the descriptor set layout
            vkDestroyDescriptorSetLayout(device, resources.descriptorSetLayout, nullptr);

            // Release the descriptor pool
            vkDestroyDescriptorPool(device, resources.descriptorPool, nullptr);
        }

        /**
         * Release core Vulkan resources.
         */
        void Cleanup(Globals& vk)
        {
            uint32_t resourceIndex;

            Shaders::Cleanup(vk.shaderCompiler);

            // Release core Vulkan objects
            for (resourceIndex = 0; resourceIndex < MAX_FRAMES_IN_FLIGHT; resourceIndex++)
            {
                vkDestroySemaphore(vk.device, vk.imageAcquiredSemaphore[resourceIndex], nullptr);
                vkDestroySemaphore(vk.device, vk.presentSemaphore[resourceIndex], nullptr);
                vkDestroyFramebuffer(vk.device, vk.frameBuffer[resourceIndex], nullptr);
                vkDestroyFence(vk.device, vk.fences[resourceIndex], nullptr);
                vkDestroyImageView(vk.device, vk.swapChainImageView[resourceIndex], nullptr);
            }

            vkFreeCommandBuffers(vk.device, vk.commandPool, MAX_FRAMES_IN_FLIGHT, vk.cmdBuffer);
            vkDestroyCommandPool(vk.device, vk.commandPool, nullptr);
            vkDestroyRenderPass(vk.device, vk.renderPass, nullptr);
            vkDestroyFence(vk.device, vk.immediateFence, nullptr);
            vkDestroySwapchainKHR(vk.device, vk.swapChain, nullptr);
            vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
            vkDestroyDevice(vk.device, nullptr);

        #if _DEBUG
            // Destroy validation layer messenger
            vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.debugUtilsMessenger, nullptr);
        #endif

            vkDestroyInstance(vk.instance, nullptr);
        }

        //----------------------------------------------------------------------------------------------------------
        // Private Scene Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Create the scene camera constant buffer.
         */
        bool CreateSceneCameraConstantBuffer(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            // Create the camera buffer resource and allocate device memory
            uint32_t size = ALIGN(256, Scenes::Camera::GetGPUDataSize());
            BufferDesc desc = { size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.cameraCB, &resources.cameraCBMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.cameraCB), "Camera Constant Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.cameraCBMemory), "Camera Constant Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Map the buffer for updates
            VKCHECK(vkMapMemory(vk.device, resources.cameraCBMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&resources.cameraCBPtr)));

            return true;
        }

        /**
         * Create the scene lights structured buffer.
         */
        bool CreateSceneLightsBuffer(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            uint32_t size = ALIGN(256, Scenes::Light::GetGPUDataSize() * static_cast<uint32_t>(scene.lights.size()));
            if (size == 0) return true; // scenes with no lights are valid

            // Create the lights upload buffer resource and allocate host memory
            BufferDesc desc = { size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.lightsSTBUploadBuffer, &resources.lightsSTBUploadMemory)) return false;

            // Create the lights device buffer resource and allocate device memory
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, &resources.lightsSTB, &resources.lightsSTBMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.lightsSTB), "Lights Structured Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.lightsSTBMemory), "Lights Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Copy the lights to the upload buffer. Leave the buffer mapped for updates.
            uint32_t offset = 0;
            VKCHECK(vkMapMemory(vk.device, resources.lightsSTBUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&resources.lightsSTBPtr)));
            for (uint32_t lightIndex = 0; lightIndex < static_cast<uint32_t>(scene.lights.size()); lightIndex++)
            {
                const Scenes::Light& light = scene.lights[lightIndex];
                memcpy(resources.lightsSTBPtr + offset, light.GetGPUData(), Scenes::Light::GetGPUDataSize());
                offset += Scenes::Light::GetGPUDataSize();
            }

            // Schedule a copy of the upload buffer to the device buffer
            VkBufferCopy bufferCopy = {};
            bufferCopy.size = size;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.lightsSTBUploadBuffer, resources.lightsSTB, 1, &bufferCopy);

            return true;
        }

        /**
         * Create the scene materials buffer.
         */
        bool CreateSceneMaterialsBuffer(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            // Create the materials buffer upload resource
            uint32_t sizeInBytes = ALIGN(16, Scenes::Material::GetGPUDataSize() * static_cast<uint32_t>(scene.materials.size()));
            BufferDesc desc = { sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.materialsSTBUploadBuffer, &resources.materialsSTBUploadMemory)) return false;

            // Create the materials buffer device resource
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, &resources.materialsSTB, &resources.materialsSTBMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.materialsSTB), "Materials Structured Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.materialsSTBMemory), "Materials Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Copy the materials to the upload buffer
            uint32_t offset = 0;
            VKCHECK(vkMapMemory(vk.device, resources.materialsSTBUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&resources.materialsSTBPtr)));
            for (uint32_t materialIndex = 0; materialIndex < static_cast<uint32_t>(scene.materials.size()); materialIndex++)
            {
                // Get the material
                Scenes::Material material = scene.materials[materialIndex];

                // Add the offset to the textures (in resource arrays)
                if (material.data.albedoTexIdx > -1) material.data.albedoTexIdx += Tex2DIndices::SCENE_TEXTURES;
                if (material.data.normalTexIdx > -1) material.data.normalTexIdx += Tex2DIndices::SCENE_TEXTURES;
                if (material.data.roughnessMetallicTexIdx > -1) material.data.roughnessMetallicTexIdx += Tex2DIndices::SCENE_TEXTURES;
                if (material.data.emissiveTexIdx > -1) material.data.emissiveTexIdx += Tex2DIndices::SCENE_TEXTURES;

                // Copy the material
                memcpy(resources.materialsSTBPtr + offset, material.GetGPUData(), Scenes::Material::GetGPUDataSize());

                // Move the destination pointer to the next material
                offset += Scenes::Material::GetGPUDataSize();
            }
            vkUnmapMemory(vk.device, resources.materialsSTBUploadMemory);

            // Schedule a copy of the upload buffer to the device buffer
            VkBufferCopy bufferCopy = {};
            bufferCopy.size = sizeInBytes;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.materialsSTBUploadBuffer, resources.materialsSTB, 1, &bufferCopy);

            return true;
        }

        /**
         * Create the scene material indexing buffers.
         */
        bool CreateSceneMaterialIndexingBuffers(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            // Mesh Offsets

            // Create the mesh offsets buffer upload resource
            uint32_t meshOffsetsSize = ALIGN(16, sizeof(uint32_t) * static_cast<uint32_t>(scene.meshes.size()));
            BufferDesc desc = { meshOffsetsSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.meshOffsetsRBUploadBuffer, &resources.meshOffsetsRBUploadMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.meshOffsetsRBUploadBuffer), "Mesh Offsets Upload Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.meshOffsetsRBUploadMemory), "Mesh Offsets Upload Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Create the mesh offsets buffer device resource
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, &resources.meshOffsetsRB, &resources.meshOffsetsRBMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.meshOffsetsRB), "Mesh Offsets Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.meshOffsetsRBMemory), "Mesh Offsets Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Geometry Data

            // Create the geometry (mesh primitive) data buffer upload resource
            uint32_t geometryDataSize = ALIGN(16, sizeof(GeometryData) * scene.numMeshPrimitives);
            desc = { geometryDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.geometryDataRBUploadBuffer, &resources.geometryDataRBUploadMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.geometryDataRBUploadBuffer), "Geometry Data Upload Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.geometryDataRBUploadMemory), "Geometry Data Upload Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Create the geometry data (mesh primitive) buffer device resource
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, &resources.geometryDataRB, &resources.geometryDataRBMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.geometryDataRB), "Geometry Data Buffer", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.geometryDataRBMemory), "Geometry Data Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Copy the mesh offsets and geometry data to the upload buffers
            uint32_t primitiveOffset = 0;
            VKCHECK(vkMapMemory(vk.device, resources.meshOffsetsRBUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&resources.meshOffsetsRBPtr)));
            VKCHECK(vkMapMemory(vk.device, resources.geometryDataRBUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&resources.geometryDataRBPtr)));

            uint8_t* meshOffsetsAddress = resources.meshOffsetsRBPtr;
            uint8_t* geometryDataAddress = resources.geometryDataRBPtr;
            for (uint32_t meshIndex = 0; meshIndex < static_cast<uint32_t>(scene.meshes.size()); meshIndex++)
            {
                // Get the mesh
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];

                // Copy the mesh offset to the upload buffer
                uint32_t meshOffset = primitiveOffset * sizeof(GeometryData);
                memcpy(meshOffsetsAddress, &meshOffset, sizeof(uint32_t));
                meshOffsetsAddress += sizeof(uint32_t);

                for (uint32_t primitiveIndex = 0; primitiveIndex < static_cast<uint32_t>(scene.meshes[meshIndex].primitives.size()); primitiveIndex++)
                {
                    // Get the mesh primitive and copy its material index to the upload buffer
                    const Scenes::MeshPrimitive& primitive = scene.meshes[meshIndex].primitives[primitiveIndex];

                    GeometryData data;
                    data.materialIndex = primitive.material;
                    data.indexByteAddress = primitive.indexByteOffset;
                    data.vertexByteAddress = primitive.vertexByteOffset;
                    memcpy(geometryDataAddress, &data, sizeof(GeometryData));

                    geometryDataAddress += sizeof(GeometryData);
                    primitiveOffset++;
                }
            }
            vkUnmapMemory(vk.device, resources.meshOffsetsRBUploadMemory);
            vkUnmapMemory(vk.device, resources.geometryDataRBUploadMemory);

            // Schedule a copy of the upload buffers to the device buffers
            VkBufferCopy bufferCopy = {};
            bufferCopy.size = meshOffsetsSize;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.meshOffsetsRBUploadBuffer, resources.meshOffsetsRB, 1, &bufferCopy);

            bufferCopy.size = geometryDataSize;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.geometryDataRBUploadBuffer, resources.geometryDataRB, 1, &bufferCopy);

            return true;
        }

        /**
         * Create the scene TLAS instances buffers.
         */
        bool CreateSceneInstancesBuffer(Globals& vk, Resources& resources, const std::vector<VkAccelerationStructureInstanceKHR>& instances)
        {
            // Create the TLAS instance upload buffer resource
            uint32_t size = static_cast<uint32_t>(instances.size()) * sizeof(VkAccelerationStructureInstanceKHR);
            BufferDesc desc = { size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
            if (!CreateBuffer(vk, desc, &resources.tlas.instancesUpload, &resources.tlas.instancesUploadMemory)) return false;

            // Create the TLAS instance device buffer resource
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!CreateBuffer(vk, desc, &resources.tlas.instances, &resources.tlas.instancesMemory)) return false;
        #ifdef GFX_NAME_OBJECTS
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.instances), "TLAS Instance Descriptors", VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.instancesMemory), "TLAS Instance Descriptors Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            // Copy the instance data to the upload buffer
            uint8_t* pData = nullptr;
            VKCHECK(vkMapMemory(vk.device, resources.tlas.instancesUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));
            memcpy(pData, instances.data(), desc.size);
            vkUnmapMemory(vk.device, resources.tlas.instancesUploadMemory);

            // Schedule a copy of the upload buffer to the device buffer
            VkBufferCopy bufferCopy = {};
            bufferCopy.size = size;
            vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.tlas.instancesUpload, resources.tlas.instances, 1, &bufferCopy);

            return true;
        }

        /**
         * Create the scene mesh index buffers.
         */
        bool CreateSceneIndexBuffers(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            uint32_t numMeshes = static_cast<uint32_t>(scene.meshes.size());

            resources.sceneIBs.resize(numMeshes);
            resources.sceneIBMemory.resize(numMeshes);
            resources.sceneIBUploadBuffers.resize(numMeshes);
            resources.sceneIBUploadMemory.resize(numMeshes);
            for (uint32_t meshIndex = 0; meshIndex < numMeshes; meshIndex++)
            {
                // Get the mesh
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];

                // Create the index buffer and copy the index data to the GPU
                if (!CreateIndexBuffer(vk, mesh,
                    &resources.sceneIBs[meshIndex],
                    &resources.sceneIBMemory[meshIndex],
                    &resources.sceneIBUploadBuffers[meshIndex],
                    &resources.sceneIBUploadMemory[meshIndex])) return false;
            #ifdef GFX_NAME_OBJECTS
                std::string name = "IB: " + mesh.name;
                std::string memoryName = "IB: " + mesh.name + " Memory";
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.sceneIBs[meshIndex]), name.c_str(), VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.sceneIBMemory[meshIndex]), memoryName.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif
            }
            return true;
        }

        /**
         * Create the scene mesh vertex buffers.
         */
        bool CreateSceneVertexBuffers(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            uint32_t numMeshes = static_cast<uint32_t>(scene.meshes.size());

            resources.sceneVBs.resize(numMeshes);
            resources.sceneVBMemory.resize(numMeshes);
            resources.sceneVBUploadBuffers.resize(numMeshes);
            resources.sceneVBUploadMemory.resize(numMeshes);
            for (uint32_t meshIndex = 0; meshIndex < numMeshes; meshIndex++)
            {
                // Get the mesh
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];

                // Create the vertex buffer and copy the data to the GPU
                if (!CreateVertexBuffer(vk, mesh,
                    &resources.sceneVBs[meshIndex],
                    &resources.sceneVBMemory[meshIndex],
                    &resources.sceneVBUploadBuffers[meshIndex],
                    &resources.sceneVBUploadMemory[meshIndex])) return false;
            #ifdef GFX_NAME_OBJECTS
                std::string name = "VB: " + mesh.name;
                std::string memoryName = "VB: " + mesh.name + " Memory";
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.sceneVBs[meshIndex]), name.c_str(), VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.sceneVBMemory[meshIndex]), memoryName.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif
            }

            return true;
        }

        /**
         * Create the scene's bottom level acceleration structure(s).
         */
        bool CreateSceneBLAS(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            // Build a BLAS for each mesh
            resources.blas.resize(scene.meshes.size());
            for (uint32_t meshIndex = 0; meshIndex < static_cast<uint32_t>(scene.meshes.size()); meshIndex++)
            {
                // Get the mesh and its BLAS
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];
                AccelerationStructure& as = resources.blas[meshIndex];

                // Create the BLAS and schedule a build
                if (!CreateBLAS(vk, resources, mesh, as)) return false;
            #ifdef GFX_NAME_OBJECTS
                std::string name = "BLAS: " + mesh.name;
                std::string memory = "BLAS Memory: " + mesh.name;
                std::string scratch = "BLAS Scratch: " + mesh.name;
                std::string scratchMemory = "BLAS Scratch Memory: " + mesh.name;
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(as.asKHR), name.c_str(), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(as.asBuffer), memory.c_str(), VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(as.asMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(as.scratch), scratch.c_str(), VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(as.scratchMemory), scratchMemory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif
            }

            // Wait for the BLAS builds to complete
            VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            vkCmdPipelineBarrier(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

            return true;
        }

        /**
         * Create the scene's top level acceleration structure.
         */
        bool CreateSceneTLAS(Globals& vk, Resources& resources, const Scenes::Scene& scene)
        {
            // Describe the scene TLAS instances
            std::vector<VkAccelerationStructureInstanceKHR> instances;
            for (size_t instanceIndex = 0; instanceIndex < scene.instances.size(); instanceIndex++)
            {
                // Get the mesh instance
                const Scenes::MeshInstance& instance = scene.instances[instanceIndex];

                // Get the BLAS device address
                VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {};
                asDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                asDeviceAddressInfo.accelerationStructure = resources.blas[instance.meshIndex].asKHR;
                VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(vk.device, &asDeviceAddressInfo);

                // Describe the mesh instance
                VkAccelerationStructureInstanceKHR desc = {};
                desc.instanceCustomIndex = instance.meshIndex; // quantized to 24-bits
                desc.mask = 0xFF;
                desc.instanceShaderBindingTableRecordOffset = 0; // A single hit group for all geometry
                desc.accelerationStructureReference = blasAddress;
            #if (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT) || (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP)
                desc.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
            #endif
                desc.flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

                // Write the instance transform
                memcpy(desc.transform.matrix, instance.transform, sizeof(DirectX::XMFLOAT4) * 3);

                instances.push_back(desc);
            }

            // Create the TLAS instances buffer
            if (!CreateSceneInstancesBuffer(vk, resources, instances)) return false;

            // Build the TLAS
            if (!CreateTLAS(vk, instances, resources.tlas)) return false;
        #ifdef GFX_NAME_OBJECTS
            std::string name = "TLAS";
            std::string memory = "TLAS Memory";
            std::string scratch = "TLAS Scratch";
            std::string scratchMemory = "TLAS Scratch Memory";
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.asKHR), name.c_str(), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.asBuffer), memory.c_str(), VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.asMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.scratch), scratch.c_str(), VK_OBJECT_TYPE_BUFFER);
            SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.scratchMemory), scratchMemory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            return true;
        }

        /**
         * Create the scene textures.
         */
        bool CreateSceneTextures(Globals& vk, Resources &resources, const Scenes::Scene &scene, std::ofstream& log)
        {
            // Early out if there are no scene textures
            if (scene.textures.size() == 0) return true;

            // Create the default and upload heap texture resources
            for (uint32_t textureIndex = 0; textureIndex < static_cast<uint32_t>(scene.textures.size()); textureIndex++)
            {
                // Get the texture
                const Textures::Texture texture = scene.textures[textureIndex];

                // Create the GPU texture resources, upload the texture data, and schedule a copy
                CHECK(CreateAndUploadTexture(vk, resources, texture, log), "create and upload scene texture!\n", log);
            }

            return true;
        }

        //----------------------------------------------------------------------------------------------------------
        // Private Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Load texture data, create GPU heap resources, upload the texture to the GPU heap,
         * unload the CPU side texture, and schedule a copy from the GPU upload to default heap.
         */
        bool CreateDefaultTexture(Globals& vk, Resources& resources, Textures::Texture& texture, std::ofstream& log)
        {
            // Load the texture from disk
            CHECK(Textures::Load(texture), "load the blue noise texture!", log);

            // Create and upload the texture data
            CHECK(CreateAndUploadTexture(vk, resources, texture, log), "create the blue noise texture!\n", log);

            // Free the texels on the CPU now that the texture data is copied to the upload buffer
            Textures::Unload(texture);

            return true;
        }

        /**
         * Load and create the default texture resources.
         */
        bool LoadAndCreateDefaultTextures(Globals& vk, Resources& resources, const Configs::Config& config, std::ofstream& log)
        {
            Textures::Texture blueNoise;
            blueNoise.name = "Blue Noise";
            blueNoise.filepath = config.app.root + "data/textures/blue-noise-rgb-256.png";
            blueNoise.type = Textures::ETextureType::ENGINE;

            // Load the texture data, create the texture, copy it to the upload buffer, and schedule a copy to the device texture
            CHECK(CreateDefaultTexture(vk, resources, blueNoise, log), "create the blue noise texture!", log);

            return true;
        }

        //----------------------------------------------------------------------------------------------------------
        // Debug Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Write an image (or images) to disk from the given Vulkan resource.
         */
        bool WriteResourceToDisk(Globals& vk, std::string file, VkImage image, uint32_t width, uint32_t height, uint32_t arraySize, VkFormat imageFormat, VkImageLayout originalLayout)
        {
            VkCommandPool commandPool = nullptr;
            VkCommandBuffer commandBuffer = nullptr;

            // Create a command pool
            {
                VkCommandPoolCreateInfo commandPoolCreateInfo = {};
                commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                commandPoolCreateInfo.queueFamilyIndex = vk.queueFamilyIndex;
                commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

                VKCHECK(vkCreateCommandPool(vk.device, &commandPoolCreateInfo, nullptr, &commandPool));
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(commandPool), "Image Capture Command Pool", VK_OBJECT_TYPE_COMMAND_POOL);
            #endif
            }

            // Create and begin the command buffer
            {
                VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
                commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                commandBufferAllocateInfo.commandBufferCount = 1;
                commandBufferAllocateInfo.commandPool = commandPool;
                commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

                VKCHECK(vkAllocateCommandBuffers(vk.device, &commandBufferAllocateInfo, &commandBuffer));
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(commandBuffer), "Image capture Command Buffer", VK_OBJECT_TYPE_COMMAND_BUFFER);
            #endif

                // Begin the command buffer
                VkCommandBufferBeginInfo commandBufferBeginInfo = {};
                commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                VKCHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
            }

            // Transition the source resource to a copy source
            {
                ImageBarrierDesc barrier =
                {
                    originalLayout,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, arraySize }
                };
                SetImageMemoryBarrier(commandBuffer, image, barrier);
            }

            // Staging (read-back) resources
            std::vector<VkBuffer> stagingBuffer; // linear layout
            std::vector<VkImage> stagingImage;   // optimal tiled layout
            std::vector<VkDeviceMemory> stagingBufferMemory;
            std::vector<VkDeviceMemory> stagingImageMemory;

            // Loop over the subresources (array slices), copying them from the GPU
            for(uint32_t subresourceIndex = 0; subresourceIndex < arraySize; subresourceIndex++)
            {
                // Add new resource entries
                stagingBuffer.emplace_back();
                stagingImage.emplace_back();
                stagingBufferMemory.emplace_back();
                stagingImageMemory.emplace_back();

                // Create the staging buffer and texture resources
                {
                    // Create the staging linear buffer
                    uint32_t sizeInBytes = width * height * ImageCapture::NumChannels;
                    BufferDesc bufferDesc = { sizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                    if (!CreateBuffer(vk, bufferDesc, &stagingBuffer[subresourceIndex], &stagingBufferMemory[subresourceIndex])) return false;

                    // Describe the staging resource
                    VkImageCreateInfo imageCreateInfo = {};
                    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
                    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                    imageCreateInfo.extent.width = width;
                    imageCreateInfo.extent.height = height;
                    imageCreateInfo.extent.depth = 1;
                    imageCreateInfo.mipLevels = 1;
                    imageCreateInfo.arrayLayers = 1;
                    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                    imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
                    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                    // Create the (optimal tiling) texture
                    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                    VKCHECK(vkCreateImage(vk.device, &imageCreateInfo, nullptr, &stagingImage[subresourceIndex]));

                    // Get the memory requirements for the optimal tiled texture
                    AllocateMemoryDesc desc = {};
                    vkGetImageMemoryRequirements(vk.device, stagingImage[subresourceIndex], &desc.requirements);

                    // Allocate and bind the memory for the optimal tiled texture
                    if (!AllocateMemory(vk, desc, &stagingImageMemory[subresourceIndex])) return false;
                    VKCHECK(vkBindImageMemory(vk.device, stagingImage[subresourceIndex], stagingImageMemory[subresourceIndex], 0));

                    // Transition the staging texture to a copy destination
                    ImageBarrierDesc barrier =
                    {
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                    };
                    SetImageMemoryBarrier(commandBuffer, stagingImage[subresourceIndex], barrier);
                }

                // Copy the source resource (slice) to the optimal tiled texture
                {
                    VkImageSubresourceLayers source = {};
                    source.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    source.layerCount = 1;
                    source.baseArrayLayer = subresourceIndex;

                    VkImageSubresourceLayers dest = {};
                    dest.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    dest.layerCount = 1;
                    dest.baseArrayLayer = 0;

                    VkImageBlit region = {};
                    region.srcSubresource = source;
                    region.dstSubresource = dest;
                    region.srcOffsets[0] = {};
                    region.srcOffsets[1] = { (int32_t)width, (int32_t)height, 1 };
                    region.dstOffsets[0] = {};
                    region.dstOffsets[1] = { (int32_t)width, (int32_t)height, 1 };

                    vkCmdBlitImage(
                        commandBuffer,
                        image,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        stagingImage[subresourceIndex],
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &region, VK_FILTER_NEAREST);
                }

                // Transition the optimal tiled texture to a copy source
                ImageBarrierDesc barrier =
                {
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageMemoryBarrier(commandBuffer, stagingImage[subresourceIndex], barrier);

                // Copy the optimal tiled texture to the linear buffer (for CPU copy)
                {
                    VkImageSubresourceLayers resource = {};
                    resource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    resource.layerCount = 1;
                    resource.baseArrayLayer = 0;

                    VkBufferImageCopy region = {};
                    region.bufferOffset = 0;
                    region.bufferRowLength = 0;
                    region.bufferImageHeight = 0;
                    region.imageSubresource = resource;
                    region.imageExtent = { width, height, 1 };

                    // Copy the tiled image to a linear buffer
                    vkCmdCopyImageToBuffer(
                        commandBuffer,
                        stagingImage[subresourceIndex],
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        stagingBuffer[subresourceIndex],
                        1, &region);
                }
            }

            // Transition the source resource back to its original layout
            {
                ImageBarrierDesc barrier =
                {
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    originalLayout,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, arraySize }
                };
                SetImageMemoryBarrier(commandBuffer, image, barrier);
            }

            // Execute GPU work
            VKCHECK(vkEndCommandBuffer(commandBuffer));

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, VK_NULL_HANDLE));

            WaitForGPU(vk);

            // Copy the linear buffer to memory for writing to disk
            bool result = true;
            for (uint32_t subresourceIndex = 0; subresourceIndex < arraySize; subresourceIndex++)
            {
                // Map the linear buffer's memory
                uint8_t* pData = nullptr;
                VKCHECK(vkMapMemory(vk.device, stagingBufferMemory[subresourceIndex], 0, VK_WHOLE_SIZE, 0, (void**)&pData));

                std::vector<uint8_t> converted(width * height * ImageCapture::NumChannels);
                memcpy(converted.data(), pData, converted.size());

                // Write the resource to disk as a PNG file (using STB)
                std::string filename = file;
                if (arraySize > 1) filename += "-Layer-" + std::to_string(subresourceIndex);
                filename.append(".png");
                result &= ImageCapture::CapturePng(filename, width, height, converted.data());

                // Unmap the linear buffers's memory
                vkUnmapMemory(vk.device, stagingBufferMemory[subresourceIndex]);
            }

            // Clean up
            for (uint32_t subresourceIndex = 0; subresourceIndex < arraySize; subresourceIndex++)
            {
                vkFreeMemory(vk.device, stagingBufferMemory[subresourceIndex], nullptr);
                vkDestroyBuffer(vk.device, stagingBuffer[subresourceIndex], nullptr);
                vkFreeMemory(vk.device, stagingImageMemory[subresourceIndex], nullptr);
                vkDestroyImage(vk.device, stagingImage[subresourceIndex], nullptr);
            }
            vkFreeCommandBuffers(vk.device, commandPool, 1, &commandBuffer);
            vkDestroyCommandPool(vk.device, commandPool, nullptr);

            return result;
        }

    #ifdef GFX_NAME_OBJECTS
        /**
         * Sets a debug name for an object.
         */
        void SetObjectName(VkDevice device, uint64_t handle, const char* name, VkObjectType type)
        {
            VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
            objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            objectNameInfo.pNext = nullptr;
            objectNameInfo.objectType = type;
            objectNameInfo.objectHandle = handle;
            objectNameInfo.pObjectName = name;

            vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
        }
    #endif

    #ifdef GFX_PERF_MARKERS
        /**
         * Add a performance marker to the command buffer.
         */
        void AddPerfMarker(Globals& vk, uint8_t r, uint8_t g, uint8_t b, std::string name)
        {
            VkDebugUtilsLabelEXT label = {};
            label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label.pLabelName = name.c_str();
            label.color[0] = (float)r / 255.f;
            label.color[1] = (float)g / 255.f;
            label.color[2] = (float)b / 255.f;
            label.color[3] = 1.f;
            vkCmdBeginDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex], &label);
        }
    #endif

        //----------------------------------------------------------------------------------------------------------
        // Public Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Toggle between windowed and fullscreen borderless modes.
         */
        bool ToggleFullscreen(Globals& vk)
        {
            GLFWmonitor* monitor = NULL;
            if (!vk.fullscreen)
            {
                glfwGetWindowPos(vk.window, &vk.x, &vk.y);
                glfwGetWindowSize(vk.window, &vk.windowWidth, &vk.windowHeight);
                monitor = glfwGetPrimaryMonitor();

                // "Borderless" fullscreen mode
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(vk.window, monitor, vk.x, vk.y, mode->width, mode->height, mode->refreshRate);
            }
            else
            {
                glfwSetWindowMonitor(vk.window, monitor, vk.x, vk.y, vk.windowWidth, vk.windowHeight, vk.vsync ? 60 : GLFW_DONT_CARE);
            }

            vk.fullscreen = ~vk.fullscreen;
            vk.fullscreenChanged = false;
            return true;
        }

        /**
         * Create a Vulkan device.
         */
        bool CreateDevice(Globals& vk, Configs::Config& config)
        {
            if(!CreateInstance(vk)) return false;
            if(!CreateSurface(vk)) return false;
            if(!CreateDeviceInternal(vk, config)) return false;
            return true;
        }

        /**
         * Add an image memory barrier on the given command buffer.
         */
        void SetImageMemoryBarrier(VkCommandBuffer cmdBuffer, VkImage image, const ImageBarrierDesc info)
        {
            VkImageMemoryBarrier imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.pNext = nullptr;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = 0;
            imageMemoryBarrier.oldLayout = info.oldLayout;
            imageMemoryBarrier.newLayout = info.newLayout;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image = image;
            imageMemoryBarrier.subresourceRange = info.subresourceRange;

            switch (info.oldLayout)
            {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            default: break;
            }

            switch (info.newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            default: break;
            }

            vkCmdPipelineBarrier(cmdBuffer, info.srcMask, info.dstMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        /**
         * Add an image layout barrier on the given command buffer.
         */
        void SetImageLayoutBarrier(VkCommandBuffer cmdBuffer, VkImage image, const ImageBarrierDesc info)
        {
            VkImageMemoryBarrier imageMemoryBarrier = {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.oldLayout = info.oldLayout;
            imageMemoryBarrier.newLayout = info.newLayout;
            imageMemoryBarrier.image = image;
            imageMemoryBarrier.subresourceRange = info.subresourceRange;

            switch (info.oldLayout)
            {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                imageMemoryBarrier.srcAccessMask = 0;
                break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default: break;
            }

            switch (info.newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                if (imageMemoryBarrier.srcAccessMask == 0)
                {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default: break;
            }

            vkCmdPipelineBarrier(cmdBuffer, info.srcMask, info.dstMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        /**
         * Create a buffer, allocate and bind device memory to the buffer.
         */
        bool CreateBuffer(Globals& vk, const BufferDesc& info, VkBuffer* buffer, VkDeviceMemory* memory)
        {
            // Describe the buffer
            VkBufferCreateInfo bufferCreateInfo = {};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size = info.size;
            bufferCreateInfo.usage = info.usage;

            // Create the buffer
            VKCHECK(vkCreateBuffer(vk.device, &bufferCreateInfo, nullptr, buffer));

            // Describe the memory allocation
            AllocateMemoryDesc desc = {};
            vkGetBufferMemoryRequirements(vk.device, *buffer, &desc.requirements);
            desc.properties = info.memoryPropertyFlags;
            desc.flags = info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0;

            // Allocate and bind memory to the buffer
            if (!AllocateMemory(vk, desc, memory)) return false;
            VKCHECK(vkBindBufferMemory(vk.device, *buffer, *memory, 0));

            return true;
        }

        /**
         * Create a texture, allocate and bind device memory, and create the texture's image view.
         */
        bool CreateTexture(Globals& vk, const TextureDesc& info, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView)
        {
            // Describe the texture
            VkImageCreateInfo imageCreateInfo = {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.format = info.format;
            imageCreateInfo.extent.width = info.width;
            imageCreateInfo.extent.height = info.height;
            imageCreateInfo.extent.depth = 1;
            imageCreateInfo.mipLevels = info.mips;
            imageCreateInfo.arrayLayers = info.arraySize;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCreateInfo.usage = info.usage;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // Create the texture
            VKCHECK(vkCreateImage(vk.device, &imageCreateInfo, nullptr, image));

            // Describe the memory allocation
            AllocateMemoryDesc desc = {};
            vkGetImageMemoryRequirements(vk.device, *image, &desc.requirements);
            desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            desc.flags = 0;

            // Allocate the texture memory and bind it to the texture
            if (!AllocateMemory(vk, desc, imageMemory)) return false;
            VKCHECK(vkBindImageMemory(vk.device, *image, *imageMemory, 0));

            // Describe the texture's image view
            VkImageViewCreateInfo imageViewCreateInfo = {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.format = imageCreateInfo.format;
            imageViewCreateInfo.image = *image;
            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.levelCount = info.mips;
            imageViewCreateInfo.subresourceRange.layerCount = info.arraySize;
            if(info.arraySize > 1) imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            else imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

            // Create the texture's image view
            VKCHECK(vkCreateImageView(vk.device, &imageViewCreateInfo, nullptr, imageView));

            return true;
        }

        /**
         * Create a shader module from compiled DXIL bytecode.
         */
        bool CreateShaderModule(VkDevice device, const Shaders::ShaderProgram& shader, VkShaderModule* result)
        {
            VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(shader.bytecode->GetBufferPointer());
            shaderModuleCreateInfo.codeSize = shader.bytecode->GetBufferSize();

            // Create the shader module
            VKCHECK(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, result));
            return true;
        }

        /**
         * Create the shader modules for a rasterization pipeline (vertex and pixel only).
         */
        bool CreateRasterShaderModules(VkDevice device, const Shaders::ShaderPipeline& shaders, ShaderModules& modules)
        {
            // Create the vertex shader module
            if (!CreateShaderModule(device, shaders.vs, &modules.vs)) return false;
            modules.numGroups++;

            // Create the pixel shader module
            if (!CreateShaderModule(device, shaders.ps, &modules.ps)) return false;
            modules.numGroups++;

            return true;
        }

        /**
         * Create the shader modules for a ray tracing pipeline.
         */
        bool CreateRayTracingShaderModules(VkDevice device, const Shaders::ShaderRTPipeline& shaders, RTShaderModules& modules)
        {
            // Create the ray generation shader module
            if (!CreateShaderModule(device, shaders.rgs, &modules.rgs)) return false;
            modules.numGroups++;

            // Create the miss shader module
            if (!CreateShaderModule(device, shaders.miss, &modules.miss)) return false;
            modules.numGroups++;

            // Create the hit group shader modules
            std::vector<VkShaderModule> hitGroups;
            for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(shaders.hitGroups.size()); hitGroupIndex++)
            {
                const Shaders::ShaderRTHitGroup& hitGroup = shaders.hitGroups[hitGroupIndex];
                modules.hitGroups.emplace_back();
                modules.numGroups++;

                if (hitGroup.hasCHS())
                {
                    if (!CreateShaderModule(device, shaders.hitGroups[hitGroupIndex].chs, &modules.hitGroups.back().chs)) return false;
                }

                if (hitGroup.hasAHS())
                {
                    if (!CreateShaderModule(device, shaders.hitGroups[hitGroupIndex].ahs, &modules.hitGroups.back().ahs)) return false;
                }

                if (hitGroup.hasIS())
                {
                    if (!CreateShaderModule(device, shaders.hitGroups[hitGroupIndex].is, &modules.hitGroups.back().is)) return false;
                }
            }

            return true;
        }

        /**
         * Create a rasterization pipeline.
         */
        bool CreateRasterPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, const Shaders::ShaderPipeline& shaders, const ShaderModules& modules, const RasterDesc& desc, VkPipeline* pipeline)
        {
            uint32_t numStages = shaders.numStages();
            uint32_t stageIndex = 0;

            // Get the stage names
            std::wstring name;
            std::vector<std::string> entryPoints;

            // Vertex shader
            name = std::wstring(shaders.vs.entryPoint);
            entryPoints.emplace_back();
            ConvertWideStringToNarrow(name, entryPoints.back());

            // Pixel shader
            name = std::wstring(shaders.ps.entryPoint);
            entryPoints.emplace_back();
            ConvertWideStringToNarrow(name, entryPoints.back());

            // Describe the raster pipeline stages
            VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo[2] = {};

            // Describe the vertex shader stage
            pipelineShaderStageCreateInfo[stageIndex].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipelineShaderStageCreateInfo[stageIndex].module = modules.vs;
            pipelineShaderStageCreateInfo[stageIndex].pName = entryPoints[stageIndex].c_str();
            pipelineShaderStageCreateInfo[stageIndex].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stageIndex++;

            // Describe the pixel shader stage
            pipelineShaderStageCreateInfo[stageIndex].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            pipelineShaderStageCreateInfo[stageIndex].module = modules.ps;
            pipelineShaderStageCreateInfo[stageIndex].pName = entryPoints[stageIndex].c_str();
            pipelineShaderStageCreateInfo[stageIndex].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stageIndex++;

            // Describe the raster pipeline
            VkGraphicsPipelineCreateInfo rasterPipelineCreateInfo = {};
            rasterPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            rasterPipelineCreateInfo.layout = pipelineLayout;
            rasterPipelineCreateInfo.pVertexInputState = &desc.vertexInputStateCreateInfo;
            rasterPipelineCreateInfo.pInputAssemblyState = &desc.inputAssemblyStateCreateInfo;
            rasterPipelineCreateInfo.renderPass = renderPass;
            rasterPipelineCreateInfo.pViewportState = &desc.viewportStateCreateInfo;
            rasterPipelineCreateInfo.pColorBlendState = &desc.colorBlendStateCreateInfo;
            rasterPipelineCreateInfo.pRasterizationState = &desc.rasterizationStateCreateInfo;
            rasterPipelineCreateInfo.pDepthStencilState = &desc.depthStencilStateCreateInfo;
            rasterPipelineCreateInfo.pMultisampleState = &desc.multisampleStateCreateInfo;
            rasterPipelineCreateInfo.pDynamicState = &desc.dynamicStateCreateInfo;
            rasterPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo;
            rasterPipelineCreateInfo.stageCount = stageIndex;

            // Create the raster pipeline
            VKCHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &rasterPipelineCreateInfo, nullptr, pipeline));

            return true;
        }

        /**
         * Create a compute pipeline.
         */
        bool CreateComputePipeline(VkDevice device, VkPipelineLayout pipelineLayout, const Shaders::ShaderProgram& shader, const VkShaderModule& module, VkPipeline* pipeline)
        {
            std::string entryPoint;
            std::wstring name = std::wstring(shader.entryPoint);
            entryPoint.resize(name.size());
            ConvertWideStringToNarrow(name, entryPoint);

            // Describe the pipeline
            VkComputePipelineCreateInfo computePipelineCreateInfo = {};
            computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            computePipelineCreateInfo.stage.module = module;
            computePipelineCreateInfo.stage.pName = entryPoint.c_str();
            computePipelineCreateInfo.layout = pipelineLayout;

            // Create the pipeline
            VKCHECK(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, pipeline));

            return true;
        }

        /**
         * Create a ray tracing pipeline.
         */
        bool CreateRayTracingPipeline(VkDevice device, VkPipelineLayout pipelineLayout, const Shaders::ShaderRTPipeline& shaders, const RTShaderModules& modules, VkPipeline* pipeline)
        {
            uint32_t numStages = 2;     // rgs + miss + (chs + ahs + is)
            uint32_t numGroups = 2;     // rgs + miss + hitGroups

            // Find the number of pipeline stages, groups, and their names
            std::wstring name;
            std::vector<std::string> entryPoints;

            // Ray generation shader
            name = std::wstring(shaders.rgs.entryPoint);
            entryPoints.emplace_back();
            ConvertWideStringToNarrow(name, entryPoints.back());

            // Miss shader
            name = std::wstring(shaders.miss.entryPoint);
            entryPoints.emplace_back();
            ConvertWideStringToNarrow(name, entryPoints.back());

            for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(shaders.hitGroups.size()); hitGroupIndex++)
            {
                const Shaders::ShaderRTHitGroup& hitGroup = shaders.hitGroups[hitGroupIndex];
                if (hitGroup.hasCHS())
                {
                    // Closest Hit Shader
                    name = std::wstring(hitGroup.chs.entryPoint);
                    entryPoints.emplace_back();
                    ConvertWideStringToNarrow(name, entryPoints.back());
                }

                if (hitGroup.hasAHS())
                {
                    // Any Hit Shader
                    name = std::wstring(hitGroup.ahs.entryPoint);
                    entryPoints.emplace_back();
                    ConvertWideStringToNarrow(name, entryPoints.back());
                }

                if (hitGroup.hasIS())
                {
                    // Intersection Shader
                    name = std::wstring(hitGroup.is.entryPoint);
                    entryPoints.emplace_back();
                    ConvertWideStringToNarrow(name, entryPoints.back());
                }

                numStages += hitGroup.numStages();
                numGroups++;
            }

            // Describe the shader stages
            std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfo;
            pipelineShaderStageCreateInfo.resize(numStages);

            // Describe the shader groups
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> rayTracingShaderGroupCreateInfo;
            rayTracingShaderGroupCreateInfo.resize(numGroups);

            // Add a stage for the ray generation shader
            uint32_t stageIndex = 0;
            uint32_t groupIndex = 0;

            VkPipelineShaderStageCreateInfo rgsStage = {};
            rgsStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            rgsStage.module = modules.rgs;
            rgsStage.pName = entryPoints[stageIndex].c_str();
            rgsStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            pipelineShaderStageCreateInfo[stageIndex] = rgsStage;

            // Add a group for the ray generation shader
            VkRayTracingShaderGroupCreateInfoKHR rgsGroup = {};
            rgsGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            rgsGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            rgsGroup.generalShader = stageIndex;
            rgsGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            rgsGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            rgsGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

            rayTracingShaderGroupCreateInfo[groupIndex++] = rgsGroup;
            stageIndex++;

            // Add a stage for the miss shader
            VkPipelineShaderStageCreateInfo missStage = {};
            missStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            missStage.module = modules.miss;
            missStage.pName = entryPoints[stageIndex].c_str();
            missStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;

            pipelineShaderStageCreateInfo[stageIndex] = missStage;

            // Add a group for the miss shader
            VkRayTracingShaderGroupCreateInfoKHR missGroup = {};
            missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            missGroup.generalShader = stageIndex;
            missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
            missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
            missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

            rayTracingShaderGroupCreateInfo[groupIndex++] = missGroup;
            stageIndex++;

            // Add the hit group shaders
            std::vector<VkPipelineShaderStageCreateInfo> hitGroupStages;
            for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(shaders.hitGroups.size()); hitGroupIndex++)
            {
                const Shaders::ShaderRTHitGroup& hitGroup = shaders.hitGroups[hitGroupIndex];
                const HitGroupShaderModules& hitGroupModules = modules.hitGroups[hitGroupIndex];

                // Describe the group for the shader hit group
                VkRayTracingShaderGroupCreateInfoKHR group = {};
                group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                group.generalShader = VK_SHADER_UNUSED_KHR;
                group.closestHitShader = VK_SHADER_UNUSED_KHR;
                group.anyHitShader = VK_SHADER_UNUSED_KHR;
                group.intersectionShader = VK_SHADER_UNUSED_KHR;

                // Add a stage for the closest hit shader, if it exists
                if(hitGroup.hasCHS())
                {
                    VkPipelineShaderStageCreateInfo chs = {};
                    chs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    chs.module = hitGroupModules.chs;
                    chs.pName = entryPoints[stageIndex].c_str();
                    chs.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                    pipelineShaderStageCreateInfo[stageIndex] = chs;

                    // Set the group index to the pipeline stage
                    group.closestHitShader = stageIndex;

                    stageIndex++;
                }

                // Add a stage for the any hit shader, if it exists
                if (hitGroup.hasAHS())
                {
                    VkPipelineShaderStageCreateInfo ahs = {};
                    ahs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    ahs.module = hitGroupModules.ahs;
                    ahs.pName = entryPoints[stageIndex].c_str();
                    ahs.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

                    pipelineShaderStageCreateInfo[stageIndex] = ahs;

                    // Set the group index to the pipeline stage
                    group.anyHitShader = stageIndex;

                    stageIndex++;
                }

                // Add a stage for the intersection shader, if it exists
                if (hitGroup.hasIS())
                {
                    VkPipelineShaderStageCreateInfo is = {};
                    is.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    is.module = hitGroupModules.is;
                    is.pName = entryPoints[stageIndex].c_str();
                    is.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

                    pipelineShaderStageCreateInfo[stageIndex] = is;

                    // Set the group index to the pipeline stage
                    group.intersectionShader = stageIndex;

                    stageIndex++;
                }

                rayTracingShaderGroupCreateInfo[groupIndex++] = group;
            }

            // Describe the pipeline
            VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo = {};
            rayTracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
            rayTracingPipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineShaderStageCreateInfo.size());
            rayTracingPipelineCreateInfo.pStages = pipelineShaderStageCreateInfo.data();
            rayTracingPipelineCreateInfo.groupCount = static_cast<uint32_t>(rayTracingShaderGroupCreateInfo.size());
            rayTracingPipelineCreateInfo.pGroups = rayTracingShaderGroupCreateInfo.data();
            rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
            rayTracingPipelineCreateInfo.layout = pipelineLayout;
            rayTracingPipelineCreateInfo.flags = VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR;

            // Create the pipeline
            VKCHECK(vkCreateRayTracingPipelinesKHR(device, 0, 0, 1, &rayTracingPipelineCreateInfo, nullptr, pipeline));
            return true;
        }

        /**
         * Helper function to start a rasterizer render pass.
         */
        void BeginRenderPass(Globals& vk)
        {
            VkClearValue clearValue = { 0.f, 0.f, 0.f, 1.f };

            VkRenderPassBeginInfo renderPassBeginInfo = {};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.framebuffer = vk.frameBuffer[vk.frameIndex];
            renderPassBeginInfo.renderArea.extent.width = vk.width;
            renderPassBeginInfo.renderArea.extent.height = vk.height;
            renderPassBeginInfo.renderPass = vk.renderPass;
            renderPassBeginInfo.pClearValues = &clearValue;
            renderPassBeginInfo.clearValueCount = 1;

            vkCmdBeginRenderPass(vk.cmdBuffer[vk.frameIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }

        /**
         * Initialize Vulkan.
         */
        bool Initialize(const Configs::Config& config, Scenes::Scene& scene, Globals& vk, Resources& resources, std::ofstream& log)
        {
            // Set config variables
            vk.width = config.app.width;
            vk.height = config.app.height;
            vk.vsync = config.app.vsync;

            // Lighting constants
            resources.constants.lights.hasDirectionalLight = scene.hasDirectionalLight;
            resources.constants.lights.numSpotLights = scene.numSpotLights;
            resources.constants.lights.numPointLights = scene.numPointLights;

            // Initialize the shader compiler
            CHECK(Shaders::Initialize(config, vk.shaderCompiler), "initialize the shader compiler!", log);

            // Create Vulkan device objects
            CHECK(CreateCommandPool(vk), "create command pool!", log);
            CHECK(CreateCommandBuffers(vk), "create command buffers!", log);
            CHECK(CreateFences(vk), "create fences!", log);
            CHECK(CreateSemaphores(vk), "create semaphores!", log);
            CHECK(CreateDescriptorPool(vk, resources), "create descriptor pool!", log);
            CHECK(CreateGlobalPipelineLayout(vk, resources), "create global pipeline layout!", log);
            CHECK(CreateSamplers(vk, resources), "create samplers!", log);
            CHECK(CreateViewport(vk), "create viewport!", log);
            CHECK(CreateScissor(vk), "create scissor!", log);

            // Create Vulkan device objects that require command buffer operations (e.g. transitions)
            CHECK(ResetCmdList(vk), "reset command buffer!", log);
            CHECK(CreateSwapChain(vk), "create swap chain!", log);
            CHECK(CreateRenderPass(vk), "create render pass!", log);
            CHECK(CreateFrameBuffers(vk), "create frame buffers!", log);
            CHECK(CreateRenderTargets(vk, resources), "create render targets!", log);
            CHECK(CreateQueryPools(vk, resources), "create query pools!", log);

            // Create default graphics resources
            CHECK(LoadAndCreateDefaultTextures(vk, resources, config, log), "load and create default textures!", log);

            // Create scene specific resources
            CHECK(CreateSceneCameraConstantBuffer(vk, resources, scene), "create scene camera constant buffer!", log);
            CHECK(CreateSceneLightsBuffer(vk, resources, scene), "create scene lights structured buffer!", log);
            CHECK(CreateSceneMaterialsBuffer(vk, resources, scene), "create scene materials buffer!", log);
            CHECK(CreateSceneMaterialIndexingBuffers(vk, resources, scene), "create scene material indexing buffers!", log);
            CHECK(CreateSceneIndexBuffers(vk, resources, scene), "create scene index buffers!", log);
            CHECK(CreateSceneVertexBuffers(vk, resources, scene), "create scene vertex buffers!", log);
            CHECK(CreateSceneBLAS(vk, resources, scene), "create scene bottom level acceleration structures!", log);
            CHECK(CreateSceneTLAS(vk, resources, scene), "create scene top level acceleration structure!", log);
            CHECK(CreateSceneTextures(vk, resources, scene, log), "create scene textures!", log);

            // Execute GPU work to finish initialization
            VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

            // Submit command buffer and block until GPU work finishes
            VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, vk.immediateFence));
            VKCHECK(vkWaitForFences(vk.device, 1, &vk.immediateFence, VK_TRUE, UINT64_MAX));
            VKCHECK(vkResetFences(vk.device, 1, &vk.immediateFence));

            CHECK(ResetCmdList(vk), "reset command buffer!", log);

            // Release upload buffers
            vkDestroyBuffer(vk.device, resources.materialsSTBUploadBuffer, nullptr);
            vkFreeMemory(vk.device, resources.materialsSTBUploadMemory, nullptr);
            vkDestroyBuffer(vk.device, resources.meshOffsetsRBUploadBuffer, nullptr);
            vkFreeMemory(vk.device, resources.meshOffsetsRBUploadMemory, nullptr);
            vkDestroyBuffer(vk.device, resources.geometryDataRBUploadBuffer, nullptr);
            vkFreeMemory(vk.device, resources.geometryDataRBUploadMemory, nullptr);
            vkDestroyBuffer(vk.device, resources.tlas.instancesUpload, nullptr);
            vkFreeMemory(vk.device, resources.tlas.instancesUploadMemory, nullptr);
            resources.tlas.instancesUpload = nullptr;
            resources.tlas.instancesUploadMemory = nullptr;

            // Release scene geometry upload buffers
            uint32_t resourceIndex;
            assert(resources.sceneIBs.size() == resources.sceneVBs.size());
            for (resourceIndex = 0; resourceIndex < static_cast<uint32_t>(resources.sceneIBs.size()); resourceIndex++)
            {
                vkDestroyBuffer(vk.device, resources.sceneIBUploadBuffers[resourceIndex], nullptr);
                vkFreeMemory(vk.device, resources.sceneIBUploadMemory[resourceIndex], nullptr);
                vkDestroyBuffer(vk.device, resources.sceneVBUploadBuffers[resourceIndex], nullptr);
                vkFreeMemory(vk.device, resources.sceneVBUploadMemory[resourceIndex], nullptr);
            }
            resources.sceneIBUploadBuffers.clear();
            resources.sceneIBUploadMemory.clear();
            resources.sceneVBUploadBuffers.clear();
            resources.sceneVBUploadMemory.clear();

            // Release scene texture upload buffers
            for (resourceIndex = 0; resourceIndex < static_cast<uint32_t>(resources.sceneTextures.size()); resourceIndex++)
            {
                vkDestroyBuffer(vk.device, resources.sceneTextureUploadBuffer[resourceIndex], nullptr);
                vkFreeMemory(vk.device, resources.sceneTextureUploadMemory[resourceIndex], nullptr);
            }
            resources.sceneTextureUploadBuffer.clear();
            resources.sceneTextureUploadMemory.clear();

            // Release default texture upload buffers
            for (resourceIndex = 0; resourceIndex < static_cast<uint32_t>(resources.textures.size()); resourceIndex++)
            {
                vkDestroyBuffer(vk.device, resources.textureUploadBuffer[resourceIndex], nullptr);
                vkFreeMemory(vk.device, resources.textureUploadMemory[resourceIndex], nullptr);
            }
            resources.textureUploadBuffer.clear();
            resources.textureUploadMemory.clear();

            // Unload the CPU-side textures
            Scenes::Cleanup(scene);

            return true;
        }

        /**
         * Post initialization tasks.
         */
        bool PostInitialize(Globals& vk, std::ofstream& log)
        {
            VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

            VkSubmitInfo endInfo = {};
            endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            endInfo.commandBufferCount = 1;
            endInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

            // Submit command buffer and block until GPU work finishes
            VKCHECK(vkQueueSubmit(vk.queue, 1, &endInfo, vk.immediateFence));
            VKCHECK(vkWaitForFences(vk.device, 1, &vk.immediateFence, VK_TRUE, UINT64_MAX));
            VKCHECK(vkResetFences(vk.device, 1, &vk.immediateFence));

            return true;
        }

        /**
         * Update constant buffers.
         */
        void Update(Globals& vk, Resources& resources, const Configs::Config& config, Scenes::Scene& scene)
        {
            // Update application constants
            resources.constants.app.frameNumber = vk.frameNumber;
            resources.constants.app.skyRadiance = { config.scene.skyColor.x * config.scene.skyIntensity, config.scene.skyColor.y  * config.scene.skyIntensity, config.scene.skyColor.z  * config.scene.skyIntensity };

            // Update the camera constant buffer
            Scenes::Camera& camera = scene.GetActiveCamera();
            camera.data.resolution.x = (float)vk.width;
            camera.data.resolution.y = (float)vk.height;
            camera.data.aspect = camera.data.resolution.x / camera.data.resolution.y;
            memcpy(resources.cameraCBPtr, camera.GetGPUData(), camera.GetGPUDataSize());

            // Update the lights buffer for lights that have been modified
            uint32_t lastDirtyLight = 0;
            for (uint32_t lightIndex = 0; lightIndex < static_cast<uint32_t>(scene.lights.size()); lightIndex++)
            {
                Scenes::Light& light = scene.lights[lightIndex];
                if (light.dirty)
                {
                    uint32_t offset = lightIndex * Scenes::Light::GetGPUDataSize();
                    memcpy(resources.lightsSTBPtr + offset, light.GetGPUData(), Scenes::Light::GetGPUDataSize());
                    light.dirty = false;
                    lastDirtyLight = lightIndex + 1;
                }
            }

            if (lastDirtyLight > 0)
            {
                // Schedule a copy of the upload buffer to the device buffer
                VkBufferCopy bufferCopy = {};
                bufferCopy.size = Scenes::Light::GetGPUDataSize() * lastDirtyLight;
                vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.lightsSTBUploadBuffer, resources.lightsSTB, 1, &bufferCopy);
            }
        }

        /**
         * Resize the swap chain and open a command buffer for other resize operations (resource transitions).
         */
        bool ResizeBegin(Globals& vk, GlobalResources& resources, int width, int height, std::ofstream& log)
        {
            vk.width = width;
            vk.height = height;

            vk.viewport.width = static_cast<float>(vk.width);
            vk.viewport.height = static_cast<float>(vk.height);
            vk.scissor.extent.width = vk.width;
            vk.scissor.extent.height = vk.height;

            // Wait for the GPU to finish up any work
            VKCHECK(vkDeviceWaitIdle(vk.device));

            // Release the Swapchain and GBuffer resources
            CleanupSwapchain(vk);
            CleanupGBuffer(vk, resources);

            // Reset the command buffer
            vkResetCommandBuffer(vk.cmdBuffer[vk.frameIndex], 0);

            // Start recording
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            VKCHECK(vkBeginCommandBuffer(vk.cmdBuffer[vk.frameIndex], &beginInfo));

            // Recreate the Swapchain and GBuffer resources
            if (!CreateSwapChain(vk)) return false;
            if (!CreateFrameBuffers(vk)) return false;
            if (!CreateRenderTargets(vk, resources)) return false;

            log << "Back buffer resize, " << vk.width << "x" << vk.height << "\n";
            log << "GBuffer resize, " << vk.width << "x" << vk.height << "\n";
            std::flush(log);

            return true;
        }

        /**
         * Close and submit the resize command buffer. Wait on the CPU for the GPU to complete.
         */
        bool ResizeEnd(Globals& vk)
        {
            // Execute GPU work to finish initialization
            VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

            // Submit command buffer and block until GPU work finishes
            VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, vk.immediateFence));
            VKCHECK(vkWaitForFences(vk.device, 1, &vk.immediateFence, VK_TRUE, UINT64_MAX));
            VKCHECK(vkResetFences(vk.device, 1, &vk.immediateFence));

            return true;
        }

        /**
         * Reset the current frame's command list and begin recording.
         */
        bool ResetCmdList(Globals& vk)
        {
            // Reset the command buffer
            vkResetCommandBuffer(vk.cmdBuffer[vk.frameIndex], 0);

            // Start recording
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            VKCHECK(vkBeginCommandBuffer(vk.cmdBuffer[vk.frameIndex], &beginInfo));

            return true;
        }

        /**
         * Close and Submit the current frame's command list.
         */
        bool SubmitCmdList(Globals& vk)
        {
            // Close the command buffer
            VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

            const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &vk.imageAcquiredSemaphore[vk.frameIndex];
            submitInfo.pWaitDstStageMask = &waitDstStageMask;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &vk.presentSemaphore[vk.frameIndex];

            // Submit the command buffer to the graphics queue
            VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, vk.fences[vk.frameIndex]));

            return true;
        }

        /**
         * Swap the back buffers.
         */
        bool Present(Globals& vk)
        {
            // Present
            VkPresentInfoKHR presentInfo = {};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &vk.presentSemaphore[vk.frameIndex];
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &vk.swapChain;
            presentInfo.pImageIndices = &vk.imageIndex;

            VkResult result = vkQueuePresentKHR(vk.queue, &presentInfo);

            if (vk.vsyncChanged)
            {
                // Wait for the GPU to finish all work
                VKCHECK(vkDeviceWaitIdle(vk.device));

                CleanupSwapchain(vk);
                vkResetCommandBuffer(vk.cmdBuffer[vk.frameIndex], 0);

                // Start recording
                VkCommandBufferBeginInfo beginInfo = {};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                VKCHECK(vkBeginCommandBuffer(vk.cmdBuffer[vk.frameIndex], &beginInfo));

                // Recreate the Swapchain
                if (!CreateSwapChain(vk)) return false;
                if (!CreateFrameBuffers(vk)) return false;

                // Execute GPU work to finish initialization
                VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

                // Submit command buffer and block until GPU work finishes
                VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, vk.immediateFence));
                VKCHECK(vkWaitForFences(vk.device, 1, &vk.immediateFence, VK_TRUE, UINT64_MAX));
                VKCHECK(vkResetFences(vk.device, 1, &vk.immediateFence));

                vk.vsyncChanged = false;
            }

            vk.frameNumber++;
            vk.frameIndex = (vk.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

            return true;
        }

        /**
         * Wait for the previous frame's graphics commands to complete on the GPU.
         */
        bool WaitForPrevGPUFrame(Globals& vk)
        {
            VKCHECK(vkWaitForFences(vk.device, 1, &vk.fences[vk.frameIndex], VK_TRUE, UINT64_MAX));
            VKCHECK(vkResetFences(vk.device, 1, &vk.fences[vk.frameIndex]));
            return true;
        }

        /**
         * Wait (right now) for all GPU work to complete.
         */
        bool WaitForGPU(Globals& vk)
        {
            return (vkDeviceWaitIdle(vk.device) == VK_SUCCESS);
        }

        /**
         * Prepare to render the next frame.
         */
        bool MoveToNextFrame(Globals& vk)
        {
            // Get the next available image from the swapchain
            VKCHECK(vkAcquireNextImageKHR(vk.device, vk.swapChain, UINT64_MAX, vk.imageAcquiredSemaphore[vk.frameIndex], VK_NULL_HANDLE, &vk.imageIndex));
            return true;
        }

        /**
         * Resolve the timestamp queries.
         */
    #ifdef GFX_PERF_INSTRUMENTATION
        void BeginFrame(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            vkCmdResetQueryPool(vk.cmdBuffer[vk.frameIndex], resources.timestampPool, 0, performance.GetNumTotalGPUQueries());
            vkCmdWriteTimestamp(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, resources.timestampPool, performance.gpuTimes[0]->GetGPUQueryBeginIndex());
        }

        void EndFrame(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            vkCmdWriteTimestamp(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, resources.timestampPool, performance.gpuTimes[0]->GetGPUQueryEndIndex());
        }

        void ResolveTimestamps(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            // nothing to do here in Vulkan
        }

        bool UpdateTimestamps(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            std::vector<Timestamp> queries;
            queries.resize(performance.GetNumActiveGPUQueries());

            // Schedule a copy of the query results to the CPU read-back buffer
            vkCmdCopyQueryPoolResults(vk.cmdBuffer[vk.frameIndex], resources.timestampPool, 0, performance.GetNumActiveGPUQueries(), resources.timestamps, 0, sizeof(Timestamp), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

            // Copy the (previous frame's) timestamps from the read-back buffer
            uint8_t* pData = nullptr;
            VKCHECK(vkMapMemory(vk.device, resources.timestampsMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));
            memcpy(queries.data(), pData, sizeof(Timestamp) * performance.GetNumActiveGPUQueries());
            vkUnmapMemory(vk.device, resources.timestampsMemory);

            // Update the GPU performance stats for the active GPU timestamp queries
            uint64_t elapsedTicks;
            for (uint32_t timestampIndex = 0; timestampIndex < static_cast<uint32_t>(performance.gpuTimes.size()); timestampIndex++)
            {
                // Get the stat
                Instrumentation::Stat*& s = performance.gpuTimes[timestampIndex];

                // Skip the stat if it wasn't active this frame
                if(s->gpuQueryStartIndex == -1) continue;

                // Compute the elapsed GPU time in milliseconds
                Timestamp start = queries[s->gpuQueryStartIndex];
                Timestamp end = queries[s->gpuQueryEndIndex];
                if(start.availability != 0 && end.availability != 0 && start.timestamp != 0)
                {
                    elapsedTicks = end.timestamp - start.timestamp;
                    s->elapsed = std::max(static_cast<double>(elapsedTicks) / 1000000, (double)0);
                    if (s->elapsed < 10000000) // sometimes timestamps are invalid, don't include those
                    {
                        Instrumentation::Resolve(s);
                    }
                }

                // Reset the GPU query indices for a new frame
                s->ResetGPUQueryIndices();
            }
            Instrumentation::Stat::ResetGPUQueryCount();

            return true;
        }
    #endif

        /**
         * Release Vulkan resources.
         */
        void Cleanup(Globals& vk, GlobalResources& resources)
        {
            Cleanup(vk.device, resources);
            Cleanup(vk);
        }

        /**
         * Write the back buffer texture resources to disk.
         */
        bool WriteBackBufferToDisk(Globals& vk, std::string directory)
        {
            return WriteResourceToDisk(vk, directory + "/R-BackBuffer", vk.swapChainImage[vk.frameIndex], vk.width, vk.height, 1, vk.swapChainFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

    }

    /**
     * Create a graphics device.
     */
    bool CreateDevice(Globals& gfx, Configs::Config& config)
    {
        return Graphics::Vulkan::CreateDevice(gfx, config);
    }

    /**
     * Initialize Vulkan.
     */
    bool Initialize(const Configs::Config& config, Scenes::Scene& scene, Globals& gfx, GlobalResources& resources, std::ofstream& log)
    {
        return Graphics::Vulkan::Initialize(config, scene, gfx, resources, log);
    }

    /**
     * Post initializationm tasks.
     */
    bool PostInitialize(Globals& gfx, std::ofstream& log)
    {
        return Graphics::Vulkan::PostInitialize(gfx, log);
    }

    /**
     * Update root constants and constant buffers.
     */
    void Update(Globals& gfx, GlobalResources& gfxResources, const Configs::Config& config, Scenes::Scene& scene)
    {
        Graphics::Vulkan::Update(gfx, gfxResources, config, scene);
    }

    /**
     * Resize the swapchain and open a command buffer for other resize operations.
     */
    bool ResizeBegin(Globals& gfx, GlobalResources& gfxResources, int width, int height, std::ofstream& log)
    {
        return Graphics::Vulkan::ResizeBegin(gfx, gfxResources, width, height, log);
    }

    /**
     * Close and submit the resize command buffer. Wait on the CPU for the GPU to complete.
     */
    bool ResizeEnd(Globals& gfx)
    {
        return Graphics::Vulkan::ResizeEnd(gfx);
    }

    /**
     * Toggle between windowed and fullscreen borderless modes.
     */
    bool ToggleFullscreen(Globals& gfx)
    {
        return Graphics::Vulkan::ToggleFullscreen(gfx);
    }

    /**
     * Reset the current frame's command list.
     */
    bool ResetCmdList(Globals& gfx)
    {
        return Graphics::Vulkan::ResetCmdList(gfx);
    }

    /**
     * Submit the current frame's command list.
     */
    bool SubmitCmdList(Globals& gfx)
    {
        return Graphics::Vulkan::SubmitCmdList(gfx);
    }

    /**
     * Present the current frame.
     */
    bool Present(Globals& gfx)
    {
        return Graphics::Vulkan::Present(gfx);
    }

    /**
     * Wait (right now) for the graphics device to idle.
     */
    bool WaitForGPU(Globals& gfx)
    {
        return Graphics::Vulkan::WaitForGPU(gfx);
    }

    /**
     * Wait for the previous frame's graphics commands to complete on the GPU.
     */
    bool WaitForPrevGPUFrame(Globals& gfx)
    {
        return Graphics::Vulkan::WaitForPrevGPUFrame(gfx);
    }

    /**
     * Move to the next the next frame.
     */
    bool MoveToNextFrame(Globals& gfx)
    {
        return Graphics::Vulkan::MoveToNextFrame(gfx);
    }

#ifdef GFX_PERF_INSTRUMENTATION
    void BeginFrame(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
    {
        return Graphics::Vulkan::BeginFrame(vk, resources, performance);
    }

    void EndFrame(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
    {
        return Graphics::Vulkan::EndFrame(vk, resources, performance);
    }

    void ResolveTimestamps(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
    {
        return Graphics::Vulkan::ResolveTimestamps(vk, resources, performance);
    }

    bool UpdateTimestamps(Globals& vk, GlobalResources& resources, Instrumentation::Performance& performance)
    {
        return Graphics::Vulkan::UpdateTimestamps(vk, resources, performance);
    }
#endif

    /**
     * Cleanup global graphics resources.
     */
    void Cleanup(Globals& gfx, GlobalResources& gfxResources)
    {
        Graphics::Vulkan::Cleanup(gfx, gfxResources);
    }

    /**
     * Write the back buffer texture resources to disk.
     */
    bool WriteBackBufferToDisk(Globals& vk, std::string directory)
    {
        return Graphics::Vulkan::WriteBackBufferToDisk(vk, directory);
    }

    // Convert std::wstring to std::string (UTF-16 to UTF-8)
    std::string WStringToString(const std::wstring& wideString)
    {
        if (wideString.empty()) return {};
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideString.data(), (int)wideString.size(), nullptr, 0, nullptr, nullptr);
        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, wideString.data(), (int)wideString.size(), &result[0], sizeNeeded, nullptr, nullptr);
        return result;
    }

    void SaveSPIRVToFile(IDxcBlob* bytecode, const std::wstring& filePath, std::ofstream& log)
    {
        // Open the file in binary mode
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            log << "Failed to open file for writing SPIR-V: " << WStringToString(filePath) << std::endl;
            return;
        }

        // Write bytecode data to file
        file.write(reinterpret_cast<const char*>(bytecode->GetBufferPointer()), bytecode->GetBufferSize());
        if (file.fail())
        {
            log << "Failed to write SPIR-V to file: " << WStringToString(filePath) << std::endl;
        }
        else
        {
            log << "SPIR-V saved to: " << WStringToString(filePath) << std::endl;
        }

        file.close();
    }


}

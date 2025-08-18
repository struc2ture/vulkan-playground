#include <cstdio>
#include <cstdint>
#include <cstring>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "helpers.hpp"

static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;

static VkAllocationCallbacks *g_Allocator = nullptr;
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkDevice g_Device = VK_NULL_HANDLE;
static VkQueue g_Queue = VK_NULL_HANDLE;
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t g_MinImageCount = 2;
static bool g_SwapChainRebuild = false;
static bool g_VSyncEnabled = true;

static ImVector<VkPhysicalDeviceProperties> g_GpuProperties;
static int g_SelectedGpuIndex;

static bool is_extension_available(const ImVector<VkExtensionProperties>& properties, const char *extension)
{
    for (const VkExtensionProperties& p: properties)
    {
        if (strcmp(p.extensionName, extension) == 0)
        {
            return true;
        }
    }
    return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}

static void set_present_mode(bool vsync, ImGui_ImplVulkanH_Window *wd)
{
    static VkPresentModeKHR present_modes_free[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    static VkPresentModeKHR present_modes_vsync[] = { VK_PRESENT_MODE_FIFO_KHR };

    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice,
                                                          wd->Surface,
                                                          vsync ? present_modes_vsync : present_modes_free,
                                                          vsync ? IM_ARRAYSIZE(present_modes_vsync) : IM_ARRAYSIZE(present_modes_free));
}

VkPhysicalDevice select_physical_device(VkInstance instance)
{
    uint32_t gpu_count;
    VkResult err = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
    check_vk_result(err);

    ImVector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    g_GpuProperties.resize(gpu_count);
    err = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.Data);
    check_vk_result(err);

    int i = 0;
    for (VkPhysicalDevice& device : gpus)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        g_GpuProperties.Data[i] = properties;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            g_SelectedGpuIndex = i;
            return device;
        }
        i++;
    }

    if (gpu_count > 0)
    {
        g_SelectedGpuIndex = 0;
        return gpus[0];
    }

    return VK_NULL_HANDLE;
}

static const char *get_vk_device_type_str(VkPhysicalDeviceType type)
{
    switch (type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "VK_PHYSICAL_DEVICE_TYPE_OTHER";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "VK_PHYSICAL_DEVICE_TYPE_CPU";
        default:
            return "Unknown device type";
    }
}

static const char *get_vk_sample_count_flag_names(VkSampleCountFlags f)
{
    static char buf[1024];
    buf[0] = '\0';
    bool added_one = false;
    if (f & VK_SAMPLE_COUNT_1_BIT) strcat(buf, "VK_SAMPLE_COUNT_1_BIT | ");
    if (f & VK_SAMPLE_COUNT_2_BIT) strcat(buf, "VK_SAMPLE_COUNT_2_BIT | ");
    if (f & VK_SAMPLE_COUNT_4_BIT) strcat(buf, "VK_SAMPLE_COUNT_4_BIT | ");
    if (f & VK_SAMPLE_COUNT_8_BIT) strcat(buf, "VK_SAMPLE_COUNT_8_BIT | ");
    if (f & VK_SAMPLE_COUNT_16_BIT) strcat(buf, "VK_SAMPLE_COUNT_16_BIT | ");
    if (f & VK_SAMPLE_COUNT_32_BIT) strcat(buf, "VK_SAMPLE_COUNT_32_BIT | ");
    if (f & VK_SAMPLE_COUNT_64_BIT) strcat(buf, "VK_SAMPLE_COUNT_64_BIT | ");
    size_t len = strlen(buf);
    if (len > 0) buf[len - 3] = '\0';
    return buf;
}

static void setup_vulkan(ImVector<const char *> instance_extensions)
{
    VkResult err;

    // Create vulkan instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(err);

        // Enable required extensions
        if (is_extension_available(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

        if (is_extension_available(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }

        // Enable validation layers
        const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layers;
        instance_extensions.push_back("VK_EXT_debug_report");

        // Create vulkan instance
        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);

        // Set up the debug report callback
        auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
        IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
        VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
        debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
        debug_report_ci.pfnCallback = debug_report;
        debug_report_ci.pUserData = nullptr;
        err = f_vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
        check_vk_result(err);
    }

    // Select GPU
    g_PhysicalDevice = select_physical_device(g_Instance);
    IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);

    // Select graphics queue family
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);

    // Create logical device (with 1 queue)
    {
        ImVector<const char *> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");
        device_extensions.push_back("VK_KHR_portability_subset");

        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, properties.Data);
        if (is_extension_available(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            device_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        }

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Create descriptor set
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size: pool_sizes)
        {
            pool_info.maxSets = pool_size.descriptorCount;
        }
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void setup_vulkan_window(ImGui_ImplVulkanH_Window *wd, VkSurfaceKHR surface, int width, int height)
{
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE)
    {
        fatal("No WSI support on physical device 0\n");
    }

    // Select surface format
    const VkFormat request_surface_image_format[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice,
                                                              wd->Surface, 
                                                              request_surface_image_format,
                                                              (size_t)IM_ARRAYSIZE(request_surface_image_format),
                                                              request_surface_color_space);

    set_present_mode(g_VSyncEnabled, wd);

    // Create Swapchain, Render pass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void cleanup_vulkan()
{
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

    auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
    f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);

    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void cleanup_vulkan_window()
{
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void frame_render(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data)
{
    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (err != VK_SUBOPTIMAL_KHR)
    {
        check_vk_result(err);
    }

    ImGui_ImplVulkanH_Frame *fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX); // Wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(g_Device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void frame_present(ImGui_ImplVulkanH_Window *wd)
{
    if (g_SwapChainRebuild)
    {
        return;
    }

    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (err != VK_SUBOPTIMAL_KHR)
    {
        check_vk_result(err);
    }
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

static void rebuild_swapchain_if_needed(ImGui_ImplVulkanH_Window *wd, GLFWwindow *window)
{
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
    {
        set_present_mode(g_VSyncEnabled, wd);

        ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
        ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount);
        g_MainWindowData.FrameIndex = 0;
        g_SwapChainRebuild = false;
    }
}

void window_device_info(VkPhysicalDeviceProperties p, int device_index)
{
    const char *arrow = " <-";
    const char *no_arrow = "";
    if (ImGui::TreeNode(p.deviceName, "%d: %s%s", device_index, p.deviceName, device_index == g_SelectedGpuIndex ? arrow : no_arrow))
    {
        const char * device_type = get_vk_device_type_str(p.deviceType);
        ImGui::BulletText("Type: %s", device_type);

        if (ImGui::TreeNode("Limits"))
        {
            ImGui::BulletText("maxImageDimension1D = %u", p.limits.maxImageDimension1D);
            ImGui::BulletText("maxImageDimension2D = %u", p.limits.maxImageDimension2D);
            ImGui::BulletText("maxImageDimension3D = %u", p.limits.maxImageDimension3D);
            ImGui::BulletText("maxImageDimensionCube = %u", p.limits.maxImageDimensionCube);
            ImGui::BulletText("maxImageArrayLayers = %u", p.limits.maxImageArrayLayers);
            ImGui::BulletText("maxTexelBufferElements = %u", p.limits.maxTexelBufferElements);
            ImGui::BulletText("maxUniformBufferRange = %u", p.limits.maxUniformBufferRange);
            ImGui::BulletText("maxStorageBufferRange = %u", p.limits.maxStorageBufferRange);
            ImGui::BulletText("maxPushConstantsSize = %u", p.limits.maxPushConstantsSize);
            ImGui::BulletText("maxMemoryAllocationCount = %u", p.limits.maxMemoryAllocationCount);
            ImGui::BulletText("maxSamplerAllocationCount = %u", p.limits.maxSamplerAllocationCount);
            ImGui::BulletText("bufferImageGranularity = %llu", p.limits.bufferImageGranularity);
            ImGui::BulletText("sparseAddressSpaceSize = %llu", p.limits.sparseAddressSpaceSize);
            ImGui::BulletText("maxBoundDescriptorSets = %u", p.limits.maxBoundDescriptorSets);
            ImGui::BulletText("maxPerStageDescriptorSamplers = %u", p.limits.maxPerStageDescriptorSamplers);
            ImGui::BulletText("maxPerStageDescriptorUniformBuffers = %u", p.limits.maxPerStageDescriptorUniformBuffers);
            ImGui::BulletText("maxPerStageDescriptorStorageBuffers = %u", p.limits.maxPerStageDescriptorStorageBuffers);
            ImGui::BulletText("maxPerStageDescriptorSampledImages = %u", p.limits.maxPerStageDescriptorSampledImages);
            ImGui::BulletText("maxPerStageDescriptorStorageImages = %u", p.limits.maxPerStageDescriptorStorageImages);
            ImGui::BulletText("maxPerStageDescriptorInputAttachments = %u", p.limits.maxPerStageDescriptorInputAttachments);
            ImGui::BulletText("maxPerStageResources = %u", p.limits.maxPerStageResources);
            ImGui::BulletText("maxDescriptorSetSamplers = %u", p.limits.maxDescriptorSetSamplers);
            ImGui::BulletText("maxDescriptorSetUniformBuffers = %u", p.limits.maxDescriptorSetUniformBuffers);
            ImGui::BulletText("maxDescriptorSetUniformBuffersDynamic = %u", p.limits.maxDescriptorSetUniformBuffersDynamic);
            ImGui::BulletText("maxDescriptorSetStorageBuffers = %u", p.limits.maxDescriptorSetStorageBuffers);
            ImGui::BulletText("maxDescriptorSetStorageBuffersDynamic = %u", p.limits.maxDescriptorSetStorageBuffersDynamic);
            ImGui::BulletText("maxDescriptorSetSampledImages = %u", p.limits.maxDescriptorSetSampledImages);
            ImGui::BulletText("maxDescriptorSetStorageImages = %u", p.limits.maxDescriptorSetStorageImages);
            ImGui::BulletText("maxDescriptorSetInputAttachments = %u", p.limits.maxDescriptorSetInputAttachments);
            ImGui::BulletText("maxVertexInputAttributes = %u", p.limits.maxVertexInputAttributes);
            ImGui::BulletText("maxVertexInputBindings = %u", p.limits.maxVertexInputBindings);
            ImGui::BulletText("maxVertexInputAttributeOffset = %u", p.limits.maxVertexInputAttributeOffset);
            ImGui::BulletText("maxVertexInputBindingStride = %u", p.limits.maxVertexInputBindingStride);
            ImGui::BulletText("maxVertexOutputComponents = %u", p.limits.maxVertexOutputComponents);
            ImGui::BulletText("maxTessellationGenerationLevel = %u", p.limits.maxTessellationGenerationLevel);
            ImGui::BulletText("maxTessellationPatchSize = %u", p.limits.maxTessellationPatchSize);
            ImGui::BulletText("maxTessellationControlPerVertexInputComponents = %u", p.limits.maxTessellationControlPerVertexInputComponents);
            ImGui::BulletText("maxTessellationControlPerVertexOutputComponents = %u", p.limits.maxTessellationControlPerVertexOutputComponents);
            ImGui::BulletText("maxTessellationControlPerPatchOutputComponents = %u", p.limits.maxTessellationControlPerPatchOutputComponents);
            ImGui::BulletText("maxTessellationControlTotalOutputComponents = %u", p.limits.maxTessellationControlTotalOutputComponents);
            ImGui::BulletText("maxTessellationEvaluationInputComponents = %u", p.limits.maxTessellationEvaluationInputComponents);
            ImGui::BulletText("maxTessellationEvaluationOutputComponents = %u", p.limits.maxTessellationEvaluationOutputComponents);
            ImGui::BulletText("maxGeometryShaderInvocations = %u", p.limits.maxGeometryShaderInvocations);
            ImGui::BulletText("maxGeometryInputComponents = %u", p.limits.maxGeometryInputComponents);
            ImGui::BulletText("maxGeometryOutputComponents = %u", p.limits.maxGeometryOutputComponents);
            ImGui::BulletText("maxGeometryOutputVertices = %u", p.limits.maxGeometryOutputVertices);
            ImGui::BulletText("maxGeometryTotalOutputComponents = %u", p.limits.maxGeometryTotalOutputComponents);
            ImGui::BulletText("maxFragmentInputComponents = %u", p.limits.maxFragmentInputComponents);
            ImGui::BulletText("maxFragmentOutputAttachments = %u", p.limits.maxFragmentOutputAttachments);
            ImGui::BulletText("maxFragmentDualSrcAttachments = %u", p.limits.maxFragmentDualSrcAttachments);
            ImGui::BulletText("maxFragmentCombinedOutputResources = %u", p.limits.maxFragmentCombinedOutputResources);
            ImGui::BulletText("maxComputeSharedMemorySize = %u", p.limits.maxComputeSharedMemorySize);
            ImGui::BulletText("maxComputeWorkGroupCount = [%u, %u, %u]", p.limits.maxComputeWorkGroupCount[0], p.limits.maxComputeWorkGroupCount[1], p.limits.maxComputeWorkGroupCount[2]);
            ImGui::BulletText("maxComputeWorkGroupInvocations = %u", p.limits.maxComputeWorkGroupInvocations);
            ImGui::BulletText("maxComputeWorkGroupSize = [%u, %u, %u]", p.limits.maxComputeWorkGroupSize[0], p.limits.maxComputeWorkGroupSize[1], p.limits.maxComputeWorkGroupSize[2]);
            ImGui::BulletText("subPixelPrecisionBits = %u", p.limits.subPixelPrecisionBits);
            ImGui::BulletText("subTexelPrecisionBits = %u", p.limits.subTexelPrecisionBits);
            ImGui::BulletText("mipmapPrecisionBits = %u", p.limits.mipmapPrecisionBits);
            ImGui::BulletText("maxDrawIndexedIndexValue = %u", p.limits.maxDrawIndexedIndexValue);
            ImGui::BulletText("maxDrawIndirectCount = %u", p.limits.maxDrawIndirectCount);
            ImGui::BulletText("maxSamplerLodBias = %0.3f", p.limits.maxSamplerLodBias);
            ImGui::BulletText("maxSamplerAnisotropy = %0.3f", p.limits.maxSamplerAnisotropy);
            ImGui::BulletText("maxViewports = %u", p.limits.maxViewports);
            ImGui::BulletText("maxViewportDimensions = [%u, %u]", p.limits.maxViewportDimensions[0], p.limits.maxViewportDimensions[1]);
            ImGui::BulletText("viewportBoundsRange = [%0.3f, %0.3f]", p.limits.viewportBoundsRange[0], p.limits.viewportBoundsRange[1]);
            ImGui::BulletText("viewportSubPixelBits = %u", p.limits.viewportSubPixelBits);
            ImGui::BulletText("minTexelBufferOffsetAlignment = %llu", p.limits.minTexelBufferOffsetAlignment);
            ImGui::BulletText("minUniformBufferOffsetAlignment = %llu", p.limits.minUniformBufferOffsetAlignment);
            ImGui::BulletText("minStorageBufferOffsetAlignment = %llu", p.limits.minStorageBufferOffsetAlignment);
            ImGui::BulletText("minTexelOffset = %d", p.limits.minTexelOffset);
            ImGui::BulletText("maxTexelOffset = %u", p.limits.maxTexelOffset);
            ImGui::BulletText("minTexelGatherOffset = %d", p.limits.minTexelGatherOffset);
            ImGui::BulletText("maxTexelGatherOffset = %u", p.limits.maxTexelGatherOffset);
            ImGui::BulletText("minInterpolationOffset = %0.3f", p.limits.minInterpolationOffset);
            ImGui::BulletText("maxInterpolationOffset = %0.3f", p.limits.maxInterpolationOffset);
            ImGui::BulletText("subPixelInterpolationOffsetBits = %u", p.limits.subPixelInterpolationOffsetBits);
            ImGui::BulletText("maxFramebufferWidth = %u", p.limits.maxFramebufferWidth);
            ImGui::BulletText("maxFramebufferHeight = %u", p.limits.maxFramebufferHeight);
            ImGui::BulletText("maxFramebufferLayers = %u", p.limits.maxFramebufferLayers);
            ImGui::BulletText("framebufferColorSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.framebufferColorSampleCounts));
            ImGui::BulletText("framebufferDepthSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.framebufferDepthSampleCounts));
            ImGui::BulletText("framebufferStencilSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.framebufferStencilSampleCounts));
            ImGui::BulletText("framebufferNoAttachmentsSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.framebufferNoAttachmentsSampleCounts));
            ImGui::BulletText("maxColorAttachments = %u", p.limits.maxColorAttachments);
            ImGui::BulletText("sampledImageColorSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.sampledImageColorSampleCounts));
            ImGui::BulletText("sampledImageIntegerSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.sampledImageIntegerSampleCounts));
            ImGui::BulletText("sampledImageDepthSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.sampledImageDepthSampleCounts));
            ImGui::BulletText("sampledImageStencilSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.sampledImageStencilSampleCounts));
            ImGui::BulletText("storageImageSampleCounts = %s", get_vk_sample_count_flag_names(p.limits.storageImageSampleCounts));
            ImGui::BulletText("maxSampleMaskWords = %u", p.limits.maxSampleMaskWords);
            ImGui::BulletText("timestampComputeAndGraphics = %s", p.limits.timestampComputeAndGraphics ? "true" : "false");
            ImGui::BulletText("timestampPeriod = %0.3f", p.limits.timestampPeriod);
            ImGui::BulletText("maxClipDistances = %u", p.limits.maxClipDistances);
            ImGui::BulletText("maxCullDistances = %u", p.limits.maxCullDistances);
            ImGui::BulletText("maxCombinedClipAndCullDistances = %u", p.limits.maxCombinedClipAndCullDistances);
            ImGui::BulletText("discreteQueuePriorities = %u", p.limits.discreteQueuePriorities);
            ImGui::BulletText("pointSizeRange = [%0.3f, %0.3f]", p.limits.pointSizeRange[0], p.limits.pointSizeRange[1]);
            ImGui::BulletText("lineWidthRange = [%0.3f, %0.3f]", p.limits.lineWidthRange[0], p.limits.lineWidthRange[1]);
            ImGui::BulletText("pointSizeGranularity = %0.3f", p.limits.pointSizeGranularity);
            ImGui::BulletText("lineWidthGranularity = %0.3f", p.limits.lineWidthGranularity);
            ImGui::BulletText("strictLines = %s", p.limits.strictLines ? "true" : "false");
            ImGui::BulletText("standardSampleLocations = %s", p.limits.standardSampleLocations ? "true" : "false");
            ImGui::BulletText("optimalBufferCopyOffsetAlignment = %llu", p.limits.optimalBufferCopyOffsetAlignment);
            ImGui::BulletText("optimalBufferCopyRowPitchAlignment = %llu", p.limits.optimalBufferCopyRowPitchAlignment);
            ImGui::BulletText("nonCoherentAtomSize = %llu", p.limits.nonCoherentAtomSize);

            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
}

void window_info(bool *show_info_window)
{
    if (*show_info_window)
    {
        ImGui::Begin("Info", show_info_window);

        if (ImGui::TreeNode("Available GPUs"))
        {
            int i = 0;
            for (VkPhysicalDeviceProperties& gpu_properties : g_GpuProperties)
            {
                window_device_info(gpu_properties, i);
                i++;
            }
            ImGui::TreePop();
        }
        ImGui::End();
    }


}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1000, 900, "Vulkan Playground", NULL, NULL);

    ImVector<const char *> extensions;
    uint32_t extensions_count;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
    {
        extensions.push_back(glfw_extensions[i]);
    }
    setup_vulkan(extensions);

    // Create window surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    // Create framebuffers
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ImGui_ImplVulkanH_Window *wd = &g_MainWindowData;
    setup_vulkan_window(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Dear ImGui Style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result_fn;
    ImGui_ImplVulkan_Init(&init_info);

    bool show_demo_window = true;
    bool show_options_window = true;
    bool show_info_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool enable_vsync = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        rebuild_swapchain_if_needed(wd, window);

        // Sleep if minimized
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED))
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        bool prev_vsync = g_VSyncEnabled;

        if (show_options_window)
        {
            ImGui::Begin("Options", &show_options_window);

            ImGui::Checkbox("VSync", &g_VSyncEnabled);

            ImGui::End();
        }

        window_info(&show_info_window);

        if (g_VSyncEnabled != prev_vsync)
        {
            g_SwapChainRebuild = true;
        }

        ImGui::Render();
        ImDrawData *draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            frame_render(wd, draw_data);
            frame_present(wd);
        }
    }

    // Cleanup
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    cleanup_vulkan_window();
    cleanup_vulkan();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

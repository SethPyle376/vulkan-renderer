#include "Engine/Renderer/Vulkan/VulkanRenderer.h"

VulkanRenderer::VulkanRenderer(const RendererParams &params) {
    this->params = params;
    resourceManager = ResourceManager::getInstance();
}

VulkanRenderer::~VulkanRenderer() {
    spdlog::debug("destroying vulkan renderer");

    vkDestroyCommandPool(device->getDevice(), commandPool, nullptr);

    framebuffers.erase(framebuffers.begin(), framebuffers.end());

    vkDestroyRenderPass(device->getDevice(), renderPass, nullptr);

    delete swapchain;
    delete device;
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
}

bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            spdlog::debug("layer: " + std::string(layerProperties.layerName));
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }
    
    return true;
}

void VulkanRenderer::init() {
    spdlog::set_level(spdlog::level::debug);
    initSDL();
    initVolk();
    initWindow();
    createInstance();
    swapchain = new VulkanSwapchain(instance);
    swapchain->initSurface(window);
    volkLoadInstance(instance);
    device = new VulkanDevice(pickPhysicalDevice());
    device->createLogicalDevice(deviceFeatures, deviceExtensions, nullptr);
    swapchain->connect(device->getPhysicalDevice(), device->getDevice());
    swapchain->create(params.x, params.y);
    initRenderPass();
    initFramebuffers();
    initCommandPool();
    initCommandBuffers();
}

void VulkanRenderer::buildCommandbuffers() {
    for (int i = 0; i < commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            spdlog::error("failed to begin vulkan command buffer");
        } else {
            spdlog::debug("began vulkan command buffer");
        }

        std::shared_ptr<VulkanPipelineResource> renderPipeline = std::static_pointer_cast<VulkanPipelineResource>(resourceManager->getResource("assets/shaders/test_vk_resource.json"));
        VkPipeline pipeline = renderPipeline->getPipeline();

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffers[i].framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {params.x, params.y};

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            spdlog::error("failed to record vulkan command buffer");
        } else {
            spdlog::debug("recorded vulkan command buffer");
        }
    }
}

void VulkanRenderer::beginFrame() {
    buildCommandbuffers();
    return;
}

void VulkanRenderer::endFrame() {
    return;
}

void VulkanRenderer::initSDL() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Vulkan_LoadLibrary(NULL);
}

void VulkanRenderer::initVolk() {
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());
}

void VulkanRenderer::initWindow() {
        spdlog::debug("initializing window");
        Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;
        window = SDL_CreateWindow("Titus v0.1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, params.x, params.y, windowFlags);
}

void VulkanRenderer::createInstance() {
    spdlog::debug("initializing vulkan instance");
    bool validationLayerSupport = checkValidationLayerSupport();
    if (enableValidationLayers && !validationLayerSupport) {
        spdlog::error("validation layers not supported");
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Titus";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Titus";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    unsigned int extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL)) {
        spdlog::error("failed to get required vulkan extension count from sdl2");
    }

    size_t additional_count = extensions.size();
    extensions.resize(additional_count + extensionCount);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data() + additional_count)) {
        spdlog::error("failed to get required vulkan extensions from sdl2");
    };

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        spdlog::error("failed to initialize instance");
    } else {
        spdlog::debug("instance initialized");
    }
}

void VulkanRenderer::initSwapchain() {
    swapchain->initSurface(window);
}

VkPhysicalDevice VulkanRenderer::pickPhysicalDevice() {
    spdlog::debug("selecting vulkan physical device");

    VkPhysicalDevice physicalDevice;

    uint32_t deviceCount = 0;

    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        spdlog::error("no physical devices with vulkan support found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device, swapchain->getSurface(), deviceExtensions)) {
            physicalDevice = device;
        }
        break;
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        spdlog::error("failed to pick Vulkan physical device");
        assert(physicalDevice != VK_NULL_HANDLE);
    } else {
        spdlog::debug("selected Vulkan physical device");
        logDeviceProperties(physicalDevice);
    }
    return physicalDevice;
}

VkDevice VulkanRenderer::getLogicalDevice() {
    return device->getDevice();
}

VkRenderPass VulkanRenderer::getRenderPass() {
    return renderPass;
}

void VulkanRenderer::initRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchain->getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        spdlog::error("failed to create render pass");
    } else {
        spdlog::debug("created render pass");
    }
}

void VulkanRenderer::initFramebuffers() {
    std::vector<SwapChainBuffer>* buffers = swapchain->getSwapChainBuffers();
    this->framebuffers.resize(buffers->size(), VulkanFramebuffer(device->getDevice()));

    spdlog::debug("creating {0} framebuffers", buffers->size());
    for (int i = 0; i < buffers->size(); i++) {
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = this->renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &(buffers->at(i).view);
        framebufferInfo.width = params.x;
        framebufferInfo.height = params.y;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device->getDevice(), &framebufferInfo, nullptr, &(framebuffers.at(i).framebuffer)) != VK_SUCCESS) {
            spdlog::error("failed to create framebuffer");
        } else {
            spdlog::debug("created vulkan framebuffer");
        }
    }
}

void VulkanRenderer::initCommandPool() {
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = device->queueFamilyIndices.graphics;
    commandPoolInfo.flags = 0;

    if (vkCreateCommandPool(device->getDevice(), &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        spdlog::error("failed to create command pool");
    } else {
        spdlog::debug("vulkan command pool created successfully");
    }
}

void VulkanRenderer::initCommandBuffers() {
    commandBuffers.resize(framebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = commandBuffers.size();

    if (vkAllocateCommandBuffers(device->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        spdlog::error("failed to allocate command buffers");
    } else {
        spdlog::debug("allocated vulkan command buffers");
    }
}
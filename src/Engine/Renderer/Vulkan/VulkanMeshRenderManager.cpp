#include "Engine/Renderer/Vulkan/VulkanMeshRenderManager.h"

VulkanMeshRenderManager::VulkanMeshRenderManager(VulkanDevice *device, int maxObjects, int frameCount) {
  std::shared_ptr<Resource> resource = ResourceManager::getInstance()->getResource("assets/shaders/test_vk_resource.json");
  this->pipeline = std::static_pointer_cast<VulkanPipelineResource>(resource);
  this->device = device;
  this->maxObjects = maxObjects;
  this->frameCount = frameCount;
  initBuffers();
  initDescriptors();
}

VulkanMeshRenderManager::~VulkanMeshRenderManager() {
  for (int i = 0; i < uniformBuffers.size(); i++) {
    delete uniformBuffers[i];
  }
  uniformBuffers.clear();

  vkDestroyDescriptorPool(device->getDevice(), descriptorPools[0], nullptr);
}

void VulkanMeshRenderManager::initBuffers() {
  uniformBuffers.resize(frameCount);

  for (int i = 0; i < uniformBuffers.size(); i++) {
    uniformBuffers[i] = new VulkanDynamicBuffer<glm::mat4>(device, maxObjects);
  }
}

void VulkanMeshRenderManager::initDescriptors() {
  VkDescriptorPoolSize poolSize = {};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  poolSize.descriptorCount = static_cast<uint32_t>(frameCount);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = frameCount;

  VkDescriptorPool descriptorPool;

  if (vkCreateDescriptorPool(device->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
    spdlog::error("failed to create vulkan descriptor pool");
  } else {
    spdlog::debug("created vulkan descriptor pool");
  }

  descriptorPools.push_back(descriptorPool);

  std::vector<VkDescriptorSetLayout> setLayouts(frameCount, pipeline->getDescriptorSetLayout());

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPools[0];
  allocInfo.descriptorSetCount = static_cast<uint32_t>(frameCount);
  allocInfo.pSetLayouts = setLayouts.data();

  descriptorSets.resize(frameCount);

  if (vkAllocateDescriptorSets(device->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
    spdlog::error("failed to create vulkan descriptor sets");
  } else {
    spdlog::debug("created vulkan descriptor sets");
  }

  for (int i = 0; i < frameCount; i++) {
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = uniformBuffers[i]->getBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSets[i];
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device->getDevice(), 1, &descriptorWrite, 0, nullptr);
  }

}

void VulkanMeshRenderManager::draw(const VulkanRenderFrame& frame, const std::vector<Node*>& drawList) {
  std::vector<std::shared_ptr<Resource>> meshes = ResourceManager::getInstance()->getResources(RESOURCE_VULKAN_MESH_INSTANCE);

  vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());

  for (int i = 0; i < drawList.size(); i++) {
    Actor* actor = (Actor*)drawList[i];

    VkBuffer vertexBuffers[] = {actor->getMeshInstance()->getMesh()->getVertexBuffer()};
    VkDeviceSize offsets[] = {0};

    glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1920.0f / 1080.0f, 0.1f, 10.0f);
    proj[1][1] *= -1;

    glm::mat4 mvp = proj * view * actor->getTransform();

    uniformBuffers[frame.currentFrameIndex]->update(actor->getMeshInstance()->getDescriptorIndex(), mvp);

    uint32_t dynamicOffset = static_cast<uint32_t>(uniformBuffers[frame.currentFrameIndex]->getDynamicAlignment() * actor->getMeshInstance()->getDescriptorIndex());

    vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &(descriptorSets[frame.currentFrameIndex]), 1, &dynamicOffset);
    vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(frame.commandBuffer, actor->getMeshInstance()->getMesh()->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

    vkCmdDrawIndexed(frame.commandBuffer, static_cast<uint32_t>(actor->getMeshInstance()->getMesh()->getIndexCount()), 1, 0, 0, 0);
  }
}
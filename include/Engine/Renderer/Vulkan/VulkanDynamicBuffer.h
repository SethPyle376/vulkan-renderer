#pragma once

#include "volk.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Engine/Renderer/Vulkan/VulkanDevice.h"

template <class T>
class VulkanDynamicBuffer
{
private:
  struct 
  {
    T *data = nullptr;
  } dataDynamic;

  glm::mat4 mvp = glm::mat4(1.0);

  VulkanDevice* device;

  VulkanBuffer* buffer;

  uint32_t maxInstances;
  size_t dynamicAlignment;

  void findDynamicAlignment();

public:
  VulkanDynamicBuffer(VulkanDevice* device, uint32_t maxInstances);
  ~VulkanDynamicBuffer();

  void update(uint32_t index, T data);

  VkBuffer getBuffer();
  size_t getDynamicAlignment();
};

template <class T>
VulkanDynamicBuffer<T>::VulkanDynamicBuffer(VulkanDevice* device, uint32_t maxInstances)
{
  this->device = device;
  this->maxInstances = maxInstances;
  findDynamicAlignment();

  buffer = new VulkanBuffer(device, dynamicAlignment * maxInstances, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
}

template <class T>
VulkanDynamicBuffer<T>::~VulkanDynamicBuffer()
{
  delete buffer;
}

template <class T>
void VulkanDynamicBuffer<T>::update(uint32_t index, T data)
{
  buffer->update(&data, sizeof(T), dynamicAlignment * index);
}

template <class T>
void VulkanDynamicBuffer<T>::findDynamicAlignment()
{
  VkPhysicalDeviceProperties deviceProperties = {};
  vkGetPhysicalDeviceProperties(device->getPhysicalDevice(), &deviceProperties);

  size_t minimum = deviceProperties.limits.minUniformBufferOffsetAlignment;

  dynamicAlignment = sizeof(T);

  if (minimum > 0)
  {
    dynamicAlignment = (dynamicAlignment + minimum - 1) & ~(minimum - 1);
  }

  spdlog::debug("Dynamic buffer dynamic aligment set to {0} bytes", dynamicAlignment);
}

template <class T>
VkBuffer VulkanDynamicBuffer<T>::getBuffer()
{
  return buffer->getBuffer();
}

template <class T>
size_t VulkanDynamicBuffer<T>::getDynamicAlignment()
{
  return dynamicAlignment;
}
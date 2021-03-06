#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "SDL.h"
#include "document.h"
#include "spdlog/spdlog.h"

#include "Engine/Resources/Resource.h"
#include "Engine/Resources/ResourceFactory.h"

class ResourceManager {
private:
  static ResourceManager *instance;
  std::unordered_map<std::string, std::weak_ptr<Resource>> resourceMap;
  std::unordered_map<std::string, std::unique_ptr<ResourceFactory>> factoryMap;

  std::shared_ptr<Resource> loadResource(std::string filename);

public:
  ResourceManager();

  void registerFactory(ResourceFactory *factory);

  std::shared_ptr<Resource> getResource(std::string filepath);
  std::vector<std::shared_ptr<Resource>> getResources(RESOURCE_TYPE type);

  static ResourceManager *getInstance();
};

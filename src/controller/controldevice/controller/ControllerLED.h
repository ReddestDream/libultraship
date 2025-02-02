#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>

#include "controller/controldevice/controller/mapping/ControllerLEDMapping.h"

namespace Ship {
class ControllerLED {
  public:
    ControllerLED(uint8_t portIndex);
    ~ControllerLED();

    std::unordered_map<std::string, std::shared_ptr<ControllerLEDMapping>> GetAllLEDMappings();
    void AddLEDMapping(std::shared_ptr<ControllerLEDMapping> mapping);
    void ClearLEDMappingId(std::string id);
    void ClearLEDMapping(std::string id);
    void SaveLEDMappingIdsToConfig();
    void ClearAllMappings();
    void ClearAllMappingsForDeviceType(PhysicalDeviceType physicalDeviceType);
    void LoadLEDMappingFromConfig(std::string id);
    void ReloadAllMappingsFromConfig();

    bool AddLEDMappingFromRawPress();

    void SetLEDColor(Color_RGB8 color);

    bool HasMappingsForPhysicalDeviceType(PhysicalDeviceType physicalDeviceType);

  private:
    uint8_t mPortIndex;
    std::unordered_map<std::string, std::shared_ptr<ControllerLEDMapping>> mLEDMappings;
};
} // namespace Ship

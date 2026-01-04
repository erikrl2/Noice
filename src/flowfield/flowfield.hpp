#pragma once

#define _USE_MATH_DEFINES

#include <string>
#include <vector>

struct FlowfieldSettings {
  char axis = 'V';
  float creaseThresholdAngle = 0.0;
};

bool ComputeUvFlowfieldFromOBJ(
    const std::string& objPath,
    std::vector<float>& outVert,
    std::vector<unsigned int>& outInd,
    const FlowfieldSettings& settings
);

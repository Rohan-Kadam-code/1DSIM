#pragma once

#include "../../../thirdparty/imgui/imgui.h"
#include <vector>
#include <memory>
#include "../../L2_domain/sim_elements.h"

struct PortPos {
    char name;
    ImVec2 pos;
};

std::vector<PortPos> GetPortPositions(std::shared_ptr<DesktopNode> node);
PortPos GetClosestPort(std::shared_ptr<DesktopNode> nodeThis, std::shared_ptr<DesktopNode> nodeOther);
std::vector<ImVec2> ComputeOrthogonalRoute(ImVec2 portA, char faceA, ImVec2 portB, char faceB);

std::shared_ptr<DesktopNode> getNodeAt(ImVec2 pos);
std::shared_ptr<DesktopLink> getLinkAt(ImVec2 pos);
float getDistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b);

void DrawSchematicCanvas(ImDrawList* dl, ImVec2 canvas_origin, ImVec2 canvas_sz);

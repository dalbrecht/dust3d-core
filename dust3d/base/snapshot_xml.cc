/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <dust3d/base/debug.h>
#include <dust3d/base/snapshot_xml.h>
#include <dust3d/base/string.h>
#include <dust3d/base/uuid.h>
#include <iostream>
#include <rapidxml.hpp>
#include <stack>

namespace dust3d {

static std::string loadComponentReturnChildrenList(Snapshot* snapshot, rapidxml::xml_node<>* parentNode)
{
    std::vector<std::string> children;
    for (rapidxml::xml_node<>* node = parentNode->first_node(); nullptr != node; node = node->next_sibling()) {
        rapidxml::xml_attribute<>* idAttribute = node->first_attribute("id");
        if (nullptr != idAttribute) {
            std::string componentIdString = idAttribute->value();
            children.push_back(componentIdString);
            std::map<std::string, std::string>* componentMap = &snapshot->components[componentIdString];
            for (rapidxml::xml_attribute<>* attribute = node->first_attribute();
                attribute; attribute = attribute->next_attribute()) {
                (*componentMap)[attribute->name()] = attribute->value();
            }
            snapshot->components[componentIdString]["children"] = loadComponentReturnChildrenList(snapshot, node);
        }
    }
    return String::join(children, ",");
}

void loadSnapshotFromXmlString(Snapshot* snapshot, char* xmlString)
{
    if (nullptr == xmlString)
        return;
    try {
        rapidxml::xml_document<> xml;
        xml.parse<0>(xmlString);
        rapidxml::xml_node<>* canvas = xml.first_node("canvas");
        if (nullptr != canvas) {
            for (rapidxml::xml_attribute<>* attribute = canvas->first_attribute();
                attribute; attribute = attribute->next_attribute()) {
                snapshot->canvas[attribute->name()] = attribute->value();
            }

            rapidxml::xml_node<>* nodes = canvas->first_node("nodes");
            if (nullptr != nodes) {
                for (rapidxml::xml_node<>* node = nodes->first_node(); nullptr != node; node = node->next_sibling()) {
                    rapidxml::xml_attribute<>* idAttribute = node->first_attribute("id");
                    if (nullptr != idAttribute) {
                        std::map<std::string, std::string>* nodeMap = &snapshot->nodes[idAttribute->value()];
                        for (rapidxml::xml_attribute<>* attribute = node->first_attribute();
                            attribute; attribute = attribute->next_attribute()) {
                            (*nodeMap)[attribute->name()] = attribute->value();
                        }
                    }
                }
            }

            rapidxml::xml_node<>* edges = canvas->first_node("edges");
            if (nullptr != edges) {
                for (rapidxml::xml_node<>* node = edges->first_node(); nullptr != node; node = node->next_sibling()) {
                    rapidxml::xml_attribute<>* idAttribute = node->first_attribute("id");
                    if (nullptr != idAttribute) {
                        std::map<std::string, std::string>* edgeMap = &snapshot->edges[idAttribute->value()];
                        for (rapidxml::xml_attribute<>* attribute = node->first_attribute();
                            attribute; attribute = attribute->next_attribute()) {
                            (*edgeMap)[attribute->name()] = attribute->value();
                        }
                    }
                }
            }

            rapidxml::xml_node<>* parts = canvas->first_node("parts");
            if (nullptr != parts) {
                for (rapidxml::xml_node<>* node = parts->first_node(); nullptr != node; node = node->next_sibling()) {
                    rapidxml::xml_attribute<>* idAttribute = node->first_attribute("id");
                    if (nullptr != idAttribute) {
                        std::map<std::string, std::string>* partMap = &snapshot->parts[idAttribute->value()];
                        for (rapidxml::xml_attribute<>* attribute = node->first_attribute();
                            attribute; attribute = attribute->next_attribute()) {
                            (*partMap)[attribute->name()] = attribute->value();
                        }
                    }
                }
            }

            rapidxml::xml_node<>* partIdList = canvas->first_node("partIdList");
            if (nullptr != partIdList) {
                for (rapidxml::xml_node<>* partId = partIdList->first_node(); nullptr != partId; partId = partId->next_sibling()) {
                    rapidxml::xml_attribute<>* idAttribute = partId->first_attribute("id");
                    if (nullptr != idAttribute) {
                        std::string componentId = to_string(Uuid::createUuid());
                        auto& component = snapshot->components[componentId];
                        component["id"] = componentId;
                        component["linkData"] = idAttribute->value();
                        component["linkDataType"] = "partId";
                        auto& childrenIds = snapshot->rootComponent["children"];
                        if (!childrenIds.empty())
                            childrenIds += ",";
                        childrenIds += componentId;
                    }
                }
            }

            rapidxml::xml_node<>* components = canvas->first_node("components");
            if (nullptr != components) {
                snapshot->rootComponent["children"] = loadComponentReturnChildrenList(snapshot, components);
            }

            rapidxml::xml_node<>* animations = canvas->first_node("animations");
            if (nullptr != animations) {
                for (rapidxml::xml_node<>* node = animations->first_node(); nullptr != node; node = node->next_sibling()) {
                    rapidxml::xml_attribute<>* idAttribute = node->first_attribute("id");
                    if (nullptr != idAttribute) {
                        std::map<std::string, std::string>* animationMap = &snapshot->animations[idAttribute->value()];
                        for (rapidxml::xml_attribute<>* attribute = node->first_attribute();
                            attribute; attribute = attribute->next_attribute()) {
                            (*animationMap)[attribute->name()] = attribute->value();
                        }
                    }
                }
            }
        }
    } catch (const std::runtime_error& e) {
        dust3dDebug << "Runtime error was: " << e.what();
    } catch (const rapidxml::parse_error& e) {
        dust3dDebug << "Parse error was: " << e.what();
    } catch (const std::exception& e) {
        dust3dDebug << "Error was: " << e.what();
    } catch (...) {
        dust3dDebug << "An unknown error occurred.";
    }
}

}

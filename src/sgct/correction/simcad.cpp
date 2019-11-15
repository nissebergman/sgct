/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#include <sgct/correction/simcad.h>

#include <sgct/error.h>
#include <sgct/messagehandler.h>
#include <sgct/viewport.h>
#include <sgct/helpers/stringfunctions.h>
#include <tinyxml2.h>

#define Error(code, msg) Error(Error::Component::SimCAD, code, msg)

namespace sgct::core::correction {

Buffer generateSimCADMesh(const std::string& path, const sgct::core::Viewport& parent) {
    // During projector alignment, a 33x33 matrix is used. This means 33x33 points can be
    // set to define geometry correction. So(x, y) coordinates are defined by the 33x33
    // matrix and the resolution used, defined by the tag. And the corrections to be
    // applied for every point in that 33x33 matrix, are stored in the warp file. This 
    // explains why this file only contains zero’s when no warp is applied.

    Buffer buf;

    MessageHandler::printInfo("Reading simcad warp data from '%s'", path.c_str());

    tinyxml2::XMLDocument xmlDoc;
    if (xmlDoc.LoadFile(path.c_str()) != tinyxml2::XML_NO_ERROR) {
        std::string str = "Parsing failed after: ";
        if (xmlDoc.GetErrorStr1() && xmlDoc.GetErrorStr2()) {
            str += xmlDoc.GetErrorStr1();
            str += ' ';
            str += xmlDoc.GetErrorStr2();
        }
        else if (xmlDoc.GetErrorStr1()) {
            str += xmlDoc.GetErrorStr1();
        }
        else if (xmlDoc.GetErrorStr2()) {
            str += xmlDoc.GetErrorStr2();
        }
        else {
            str = "File not found";
        }

        throw Error(2060, "Error parsing XML file " + path + ". " + str);
    }

    tinyxml2::XMLElement* XMLroot = xmlDoc.FirstChildElement("GeometryFile");
    if (XMLroot == nullptr) {
        throw Error(2061, "Error reading XML file " + path + ". Missing 'GeometryFile'");
    }

    using namespace tinyxml2;
    XMLElement* element = XMLroot->FirstChildElement("GeometryDefinition");
    if (element == nullptr) {
        throw Error(
            2062,
            "Error reading XML file " + path + ". Missing 'GeometryDefinition'"
        );
    }

    float xrange = 1.f;
    float yrange = 1.f;
    std::vector<float> xcorrections, ycorrections;
    XMLElement* child = element->FirstChildElement();
    while (child) {
        std::string_view childVal = child->Value();

        if (childVal == "X-FlatParameters") {
            if (child->QueryFloatAttribute("range", &xrange) == XML_NO_ERROR) {
                std::string xcoordstr(child->GetText());
                std::vector<std::string> xcoords = helpers::split(xcoordstr, ' ');
                for (const std::string& x : xcoords) {
                    xcorrections.push_back(std::stof(x) / xrange);
                } 
            }
        }
        else if (childVal == "Y-FlatParameters") {
            if (child->QueryFloatAttribute("range", &yrange) == XML_NO_ERROR) {
                std::string ycoordstr(child->GetText());
                std::vector<std::string> ycoords = helpers::split(ycoordstr, ' ');
                for (const std::string& y : ycoords) {
                    ycorrections.push_back(std::stof(y) / yrange);
                }
            }
        }

        child = child->NextSiblingElement();
    }

    if (xcorrections.size() != ycorrections.size()) {
        throw Error(2063, "Not the same x coords as y coords");
    }

    const float numberOfColsf = sqrt(static_cast<float>(xcorrections.size()));
    const float numberOfRowsf = sqrt(static_cast<float>(ycorrections.size()));

    if (ceil(numberOfColsf) != numberOfColsf || ceil(numberOfRowsf) != numberOfRowsf) {
        throw Error(2064, "Not a valid squared matrix read from SimCAD file");
    }

    const unsigned int nCols = static_cast<unsigned int>(numberOfColsf);
    const unsigned int nRows = static_cast<unsigned int>(numberOfRowsf);

    // init to max intensity (opaque white)
    CorrectionMeshVertex vertex;
    vertex.r = 1.f;
    vertex.g = 1.f;
    vertex.b = 1.f;
    vertex.a = 1.f;

    size_t i = 0;
    for (unsigned int r = 0; r < nRows; r++) {
        for (unsigned int c = 0; c < nCols; c++) {
            // vertex-mapping
            const float u = (static_cast<float>(c) / (static_cast<float>(nCols) - 1.f));

            // @TODO (abock, 2019-09-01);  Not sure why we are inverting the y coordinate
            // for this, but we do it multiple times in this file and I'm starting to get
            // the impression that there might be a flipping issue going on somewhere
            // deeper
            const float v = 1.f - (static_cast<float>(r) /
                                  (static_cast<float>(nRows) - 1.f));

            const float x = u + xcorrections[i];
            const float y = v - ycorrections[i];

            // convert to [-1, 1]
            vertex.x = 2.f * (x * parent.getSize().x + parent.getPosition().x) - 1.f;
            vertex.y = 2.f * (y * parent.getSize().y + parent.getPosition().y) - 1.f;

            // scale to viewport coordinates
            vertex.s = u * parent.getSize().x + parent.getPosition().x;
            vertex.t = v * parent.getSize().y + parent.getPosition().y;

            buf.vertices.push_back(vertex);
            i++;
        }
    }

    // Make a triangle strip index list
    buf.indices.reserve(4 * nRows * nCols);
    for (unsigned int r = 0; r < nRows - 1; r++) {
        if ((r % 2) == 0) {
            // even rows
            for (unsigned int c = 0; c < nCols; c++) {
                buf.indices.push_back(c + r * nCols);
                buf.indices.push_back(c + (r + 1) * nCols);
            }
        }
        else {
            // odd rows
            for (unsigned int c = nCols - 1; c > 0; c--) {
                buf.indices.push_back(c + (r + 1) * nCols);
                buf.indices.push_back(c - 1 + r * nCols);
            }
        }
    }

    buf.geometryType = GL_TRIANGLE_STRIP;
    return buf;
}

} // namespace sgct::core::correction

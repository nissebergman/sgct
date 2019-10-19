/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#include <sgct/postfx.h>

#include <sgct/clustermanager.h>
#include <sgct/engine.h>
#include <sgct/messagehandler.h>
#include <sgct/offscreenbuffer.h>

namespace sgct {

bool PostFX::init(std::string name, const std::string& vertShaderSrc,
                  const std::string& fragShaderSrc)
{
    _name = std::move(name);
    _shaderProgram.setName(_name);

    if (!_shaderProgram.addShaderSrc(vertShaderSrc, GL_VERTEX_SHADER)) {
        MessageHandler::printError(
            "PostFX: Pass '%s' failed to load or set vertex shader", _name.c_str()
        );
        return false;
    }

    if (!_shaderProgram.addShaderSrc(fragShaderSrc, GL_FRAGMENT_SHADER)) {
        MessageHandler::printError(
            "PostFX: Pass '%s' failed to load or set fragment shader", _name.c_str()
        );
        return false;
    }

    if (!_shaderProgram.createAndLinkProgram()) {
        MessageHandler::printError(
            "PostFX: Pass '%s' failed to link shader", _name.c_str()
        );
        return false;
    }

    _renderFn = &PostFX::internalRender;

    return true;
}

void PostFX::destroy() {
    MessageHandler::printInfo(
        "PostFX: Pass '%s' destroying shader and texture", _name.c_str()
    );

    _renderFn = nullptr;
    _updateFn = nullptr;

    if (!_deleted) {
        _shaderProgram.deleteProgram();
        _deleted = true;
    }
}

void PostFX::render() {
    if (_renderFn) {
        (this->*_renderFn)();
    }
}

void PostFX::setUpdateUniformsFunction(void(*fnPtr)()) {
    _updateFn = fnPtr;
}

void PostFX::setInputTexture(unsigned int inputTex) {
    _inputTexture = inputTex;
}

void PostFX::setOutputTexture(unsigned int outputTex) {
    _outputTexture = outputTex;
}

unsigned int PostFX::getOutputTexture() const {
    return _outputTexture;
}

unsigned int PostFX::getInputTexture() const {
    return _inputTexture;
}

ShaderProgram& PostFX::getShaderProgram() {
    return _shaderProgram;
}

const ShaderProgram& PostFX::getShaderProgram() const {
    return _shaderProgram;
}

const std::string& PostFX::getName() const {
    return _name;
}

void PostFX::internalRender() {
    Window& win = core::ClusterManager::instance()->getThisNode().getCurrentWindow();

    // bind target FBO
    win.getFBO()->attachColorTexture(_outputTexture);

    _size = win.getFramebufferResolution();

    glViewport(0, 0, _size.x, _size.y);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _inputTexture);

    _shaderProgram.bind();

    if (_updateFn) {
        _updateFn();
    }

    win.bindVAO();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    win.unbindVAO();

    ShaderProgram::unbind();
}

} // namespace sgct

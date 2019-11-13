#include <sgct/clustermanager.h>
#include <sgct/commandline.h>
#include <sgct/engine.h>
#include <sgct/shadermanager.h>
#include <sgct/shareddata.h>
#include <sgct/texturemanager.h>
#include <sgct/trackingmanager.h>
#include <sgct/user.h>
#include <sgct/window.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    constexpr const int GridSize = 256;

    // shader data
    GLint currTimeLoc = -1;
    GLint mvpLoc = -1;
    GLint mvLoc = -1;
    GLint mvLightLoc = -1;
    GLint nmLoc = -1;

    unsigned int heightTextureId = 0;
    unsigned int normalTextureId = 0;

    GLuint vertexArray = 0;
    GLuint vertexPositionBuffer = 0;

    bool mPause = false;

    // variables to share across cluster
    sgct::SharedDouble currentTime(0.0);
    sgct::SharedBool info(false);
    sgct::SharedBool stats(false);
    sgct::SharedBool takeScreenshot(false);
    sgct::SharedBool useTracking(false);
    sgct::SharedObject<sgct::Window::StereoMode> stereoMode;

    struct Vertex {
        float x, y, z;
        float s, t;
    };
    using Geometry = std::vector<Vertex>;
    // struct Geometry {
    //     std::vector<float> vertPos;
    //     std::vector<float> texCoord;
    //     GLsizei numberOfVerts = 0;
    // };

    constexpr const char* vertexShader = R"(
  #version 330 core

  layout(location = 0) in vec3 vertPositions;
  layout(location = 1) in vec2 texCoords;

  out vec2 uv;
  out float vScale; // Height scaling
  out vec3 lightDir;
  out vec3 v;

  uniform sampler2D hTex;
  uniform float currTime;
  uniform mat4 mvp;
  uniform mat4 mv;
  uniform mat4 mvLight;
  uniform vec4 lightPos;

  void main() {
    uv = texCoords;

    vScale = 0.2 + 0.10 * sin(currTime);
    float hVal = texture(hTex, uv).r;
    vec4 transformedVertex = vec4(vertPositions + vec3(0.0, hVal * vScale, 0.0), 1.0);

    // Transform a vertex to model space
    v = vec3(mv * transformedVertex);
    vec3 l = vec3(mvLight * lightPos);
    lightDir = normalize(l - v);
  
    // Output position of the vertex, in clip space : MVP * position
    gl_Position =  mvp * transformedVertex;
  })";

    constexpr const char* fragmentShader = R"(
  #version 330 core

  in vec2 uv;
  in float vScale;
  in vec3 lightDir;
  in vec3 v;

  out vec4 color;

  uniform sampler2D hTex;
  uniform sampler2D nTex;
  uniform vec4 lightAmbient;
  uniform vec4 lightDiffuse;
  uniform vec4 lightSpecular;
  uniform mat3 normalMatrix;

  const float Pi = 3.14159265358979323846264;

  // Computes the diffues shading by using the normal for
  // the fragment and direction from fragment to the light
  vec4 calcShading(vec3 N, vec3 L) {
    // Ambient contribution
    vec4 iamb = lightAmbient;

    // Diffuse contribution
    vec4 idiff = lightDiffuse * max(dot(N, L), 0.0);
    idiff = clamp(idiff, 0.0, 1.0);

    // Specular contribution
    vec3 E = normalize(-v);
    vec3 R = normalize(reflect(-L, N));
    const float specExp = 32.0;
    vec4 ispec = lightSpecular * pow(max(dot(R, E), 0.0), specExp);
    ispec = clamp(ispec, 0.0, 1.0);

    return iamb + idiff + ispec;
  }

  void main() {
    vec3 pixelVals = texture(nTex, uv).rgb;
    vec3 normal = vec3(
      (pixelVals.r * 2.0 - 1.0),
      (pixelVals.b * 2.0 - 1.0) / vScale,
      (pixelVals.g * 2.0 - 1.0)
    );
    if (vScale < 0) {
      normal = -normal;
    }

    // Set fragment color
    // This will result in a non-linear color temperature scale based on height value
    float hVal = texture(hTex, uv).x;
    color.rgb = vec3(1.0 - cos(Pi * hVal), sin(Pi * hVal), cos(Pi * hVal));

    // multiply color with shading
    color.rgb *= calcShading(normalize(normalMatrix * normal), lightDir).rgb;
    color.a = 1.0;
  })";
} // namespace

using namespace sgct;

/**
 *
 * Will draw a flat surface that can be used for the heightmapped terrain.
 *
 * \param width Width of the surface
 * \param depth Depth of the surface
 * \param wRes Width resolution of the surface
 * \param dRes Depth resolution of the surface
 */
Geometry generateTerrainGrid(float width, float depth, int wRes, int dRes) {
    const float wStart = -width * 0.5f;
    const float dStart = -depth * 0.5f;

    const float dW = width / static_cast<float>(wRes);
    const float dD = depth / static_cast<float>(dRes);

    Geometry res;

    for (int depthIndex = 0; depthIndex < dRes; ++depthIndex) {
        const float dPosLow = dStart + dD * static_cast<float>(depthIndex);
        const float dPosHigh = dStart + dD * static_cast<float>(depthIndex + 1);
        const float dTexCoordLow = depthIndex / static_cast<float>(dRes);
        const float dTexCoordHigh = (depthIndex + 1) / static_cast<float>(dRes);

        for (int widthIndex = 0; widthIndex < wRes; ++widthIndex) {
            const float wPos = wStart + dW * static_cast<float>(widthIndex);
            const float wTexCoord = widthIndex / static_cast<float>(wRes);

            Vertex p0;
            p0.x = wPos;
            p0.y = 0.f;
            p0.z = dPosLow;
            p0.s = wTexCoord;
            p0.t = dTexCoordLow;
            res.push_back(p0);

            Vertex p1;
            p1.x = wPos;
            p1.y = 0.f;
            p1.z = dPosHigh;
            p1.s = wTexCoord;
            p1.t = dTexCoordHigh;
            res.push_back(p1);
        }
    }

    return res;
}

void drawFun() {
    glLineWidth(1.0);

    constexpr double Speed = 0.14;

    // create scene transform (animation)
    glm::mat4 scene = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -0.15f, 2.5f));
    scene = glm::rotate(
        scene,
        static_cast<float>(currentTime.getVal() * Speed),
        glm::vec3(0.f, 1.f, 0.f)
    );

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heightTextureId);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTextureId);

    const ShaderProgram& prog = ShaderManager::instance()->getShaderProgram("xform");
    prog.bind();

    const glm::mat4 mvp = Engine::instance()->getCurrentModelViewProjectionMatrix() *
                          scene;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    const glm::mat4 mv = Engine::instance()->getCurrentModelViewMatrix() * scene;
    glUniformMatrix4fv(mvLoc, 1, GL_FALSE, glm::value_ptr(mv));
    const glm::mat4 mvLight = Engine::instance()->getCurrentModelViewMatrix();
    glUniformMatrix4fv(mvLightLoc, 1, GL_FALSE, glm::value_ptr(mvLight));
    const glm::mat3 normal = glm::inverseTranspose(glm::mat3(mv));
    glUniformMatrix3fv(nmLoc, 1, GL_FALSE, glm::value_ptr(normal));
    glUniform1f(currTimeLoc, static_cast<float>(currentTime.getVal()));

    glBindVertexArray(vertexArray);

    // Draw the triangles
    for (unsigned int i = 0; i < GridSize; i++) {
        glDrawArrays(GL_TRIANGLE_STRIP, i * GridSize * 2, GridSize * 2);
    }

    glBindVertexArray(0);

    prog.unbind();
}

void preSyncFun() {
    if (Engine::instance()->isMaster() && !mPause) {
        currentTime.setVal(currentTime.getVal() + Engine::instance()->getAvgDt());
    }
}

void postSyncPreDrawFun() {
    Engine::instance()->setDisplayInfoVisibility(info.getVal());
    Engine::instance()->setStatsGraphVisibility(stats.getVal());
    Engine::instance()->getTrackingManager().setEnabled(useTracking.getVal());

    if (takeScreenshot.getVal()) {
        Engine::instance()->takeScreenshot();
        takeScreenshot.setVal(false);
    }
}

void initOGLFun() {
    stereoMode.setVal(Engine::instance()->getWindow(0).getStereoMode());

    heightTextureId = TextureManager::instance()->loadTexture("heightmap.png", true, 0);
    normalTextureId = TextureManager::instance()->loadTexture("normalmap.png", true, 0);

    // setup shader
    ShaderManager::instance()->addShaderProgram("xform", vertexShader, fragmentShader);
    const ShaderProgram& prog = ShaderManager::instance()->getShaderProgram("xform");

    prog.bind();
    currTimeLoc = prog.getUniformLocation("currTime");
    mvpLoc = prog.getUniformLocation("mvp");
    mvLoc = prog.getUniformLocation("mv");
    mvLightLoc = prog.getUniformLocation("mvLight");
    nmLoc = prog.getUniformLocation("normalMatrix");
    glUniform1i(prog.getUniformLocation("hTex"), 0);
    glUniform1i(prog.getUniformLocation("nTex"), 1);

    // light data
    const glm::vec4 position(-2.f, 5.f, 5.f, 1.f);
    const glm::vec4 ambient(0.1f, 0.1f, 0.1f, 1.f);
    const glm::vec4 diffuse(0.8f, 0.8f, 0.8f, 1.f);
    const glm::vec4 specular(1.f, 1.f, 1.f, 1.f);

    glUniform4fv(prog.getUniformLocation("lightPos"), 1, glm::value_ptr(position));
    glUniform4fv(prog.getUniformLocation("lightAmbient"), 1, glm::value_ptr(ambient));
    glUniform4fv(prog.getUniformLocation("lightDiffuse"), 1, glm::value_ptr(diffuse));
    glUniform4fv(prog.getUniformLocation("lightSpecular"), 1, glm::value_ptr(specular));
    prog.unbind();

    Geometry geometry = generateTerrainGrid(1.f, 1.f, GridSize, GridSize);

    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    glGenBuffers(1, &vertexPositionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexPositionBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(Vertex) * geometry.size(),
        geometry.data(),
        GL_STATIC_DRAW
    );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(3 * sizeof(float))
    );

    glBindVertexArray(0);
}

void encodeFun() {
    SharedData::instance()->writeDouble(currentTime);
    SharedData::instance()->writeBool(info);
    SharedData::instance()->writeBool(stats);
    SharedData::instance()->writeBool(takeScreenshot);
    SharedData::instance()->writeBool(useTracking);
    SharedData::instance()->writeObj(stereoMode);
}

void decodeFun() {
    SharedData::instance()->readDouble(currentTime);
    SharedData::instance()->readBool(info);
    SharedData::instance()->readBool(stats);
    SharedData::instance()->readBool(takeScreenshot);
    SharedData::instance()->readBool(useTracking);
    SharedData::instance()->readObj(stereoMode);
}

void keyCallback(int key, int, int action, int) {
    if (Engine::instance()->isMaster() && action == action::Press) {
        switch (key) {
            case key::S:
                stats.setVal(!stats.getVal());
                break;
            case key::I:
                info.setVal(!info.getVal());
                break;
            case key::Q:
                Engine::instance()->terminate();
                break;
            case key::T:
                useTracking.setVal(!useTracking.getVal());
                break;
            case key::E:
                sgct::core::ClusterManager::instance()->getDefaultUser().setTransform(
                    glm::translate(glm::dmat4(1.0), glm::dvec3(0.0, 0.0, 4.0))
                );
                break;
            case key::Space:
                mPause = !mPause;
                break;
            case key::F:
                for (int i = 0; i < Engine::instance()->getNumberOfWindows(); i++) {
                    Engine::instance()->getWindow(i).setUseFXAA(
                        !Engine::instance()->getWindow(i).useFXAA()
                    );
                }
                break;
            case key::P:
            case key::F10:
                takeScreenshot.setVal(true);
                break;
            case key::Left:
                if (static_cast<int>(stereoMode.getVal()) > 0) {
                    const int v = static_cast<int>(stereoMode.getVal()) - 1;
                    Window::StereoMode m = static_cast<Window::StereoMode>(v);
                    stereoMode.setVal(m);
                }
                break;
            case key::Right:
                const int v = static_cast<int>(stereoMode.getVal()) + 1;
                Window::StereoMode m = static_cast<Window::StereoMode>(v);
                stereoMode.setVal(m);
                break;
        }
    }
}

void cleanUpFun() {
    glDeleteBuffers(1, &vertexPositionBuffer);
    glDeleteVertexArrays(1, &vertexArray);
}

int main(int argc, char* argv[]) {
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);
    Engine::create(config);

    Engine::instance()->setInitOGLFunction(initOGLFun);
    Engine::instance()->setDrawFunction(drawFun);
    Engine::instance()->setPreSyncFunction(preSyncFun);
    Engine::instance()->setPostSyncPreDrawFunction(postSyncPreDrawFun);
    Engine::instance()->setCleanUpFunction(cleanUpFun);
    Engine::instance()->setKeyboardCallbackFunction(keyCallback);
    Engine::instance()->setEncodeFunction(encodeFun);
    Engine::instance()->setDecodeFunction(decodeFun);

    try {
        Engine::instance()->init(Engine::RunMode::Default_Mode, cluster);
    }
    catch (const std::runtime_error&) {
        Engine::destroy();
        return EXIT_FAILURE;
    }

    Engine::instance()->render();
    Engine::destroy();
    exit(EXIT_SUCCESS);
}
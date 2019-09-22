/*************************************************************************
Copyright (c) 2012-2015 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#ifndef __SGCT__WINDOW__H__
#define __SGCT__WINDOW__H__

#include <sgct/offscreenbuffer.h>
#include <sgct/postfx.h>
#include <sgct/screencapture.h>
#include <sgct/viewport.h>
#include <glm/glm.hpp>
#include <vector>

#define NUMBER_OF_TEXTURES 8

struct GLFWmonitor;
struct GLFWwindow;

namespace sgct::core {
    class BaseViewport;
    class ScreenCapture;
} // namespace sgct::core

namespace sgct {

/**
 * Helper class for window data. 
 */
class Window {
public:    
    /// Different stereo modes used for rendering
    enum class StereoMode {
        NoStereo = 0,
        Active,
        AnaglyphRedCyan,
        AnaglyphAmberBlue,
        AnaglyphRedCyanWimmer,
        Checkerboard,
        CheckerboardInverted,
        VerticalInterlaced,
        VerticalInterlacedInverted,
        Dummy,
        SideBySide,
        SideBySideInverted,
        TopBottom,
        TopBottomInverted
    };

    enum class Context { Shared = 0, Window, Unset };
    
    enum class ColorBitDepth {
        Depth8,
        Depth16,
        Depth16Float,
        Depth32Float,
        Depth16Int,
        Depth32Int,
        Depth16UInt,
        Depth32UInt
    };

    enum class Eye { MonoOrLeft, Right };

    /**
     * Init Nvidia swap groups if they are supported by hardware. Supported hardware is
     * Nvidia Quadro graphics card + sync card or AMD/ATI FireGL graphics card + sync
     * card.
     */
    static void initNvidiaSwapGroups();

    /// Force a restore of the shared OpenGL context
    static void restoreSharedContext();
    static void resetSwapGroupFrameNumber();
    static void setBarrier(bool state);
    static bool isBarrierActive();
    static bool isUsingSwapGroups();
    static bool isSwapGroupMaster();
    static unsigned int getSwapGroupFrameNumber();

    explicit Window(int id);

    void close();
    void init();

    /// Init window buffers such as textures, FBOs, VAOs, VBOs and PBOs.
    void initOGL();

    /// Init context specific data such as viewport corrections/warping meshes
    void initContextSpecificOGL();

    /**
     * Don't use this function if you want to set the window resolution. Use
     * setWindowResolution(const int x, const int y) instead. This function is called
     * within SGCT when the window is created.
     */
    void initWindowResolution(glm::ivec2 resolution);

    /// Swap previous data and current data. This is done at the end of the render loop.
    void swap(bool takeScreenshot);
    void updateResolutions();

    /// \returns true if frame buffer is resized and window is visible.
    bool update();

    /**
     * This function is used internally within sgct to open the window.
     *
     * /returns True if window was created successfully.
     */
    bool openWindow(GLFWwindow* share, int lastWindowIdx);

    /**
     * Set this window's OpenGL context or the shared context as current. This function
     * keeps track of which context is in use and only set the context to current if it's
     * not.
     */
    void makeOpenGLContextCurrent(Context context);

    /// Name this window
    void setName(std::string name);

    /**
     * Tag this window. Tags are seperated by comma
     */
    void setTags(std::vector<std::string> tags);

    /**
     * Set the visibility state of this window. If a window is hidden the rendering for
     * that window will be paused unless it's forced to render while hidden by using
     * setRenderWhileHidden().
     */
    void setVisibility(bool state);

    /**
     * Set if window should render while hidden. Normally a window pauses the rendering if
     * it's hidden.
     */
    void setRenderWhileHidden(bool state);
    
    /// Set the focued flag for this window (should not be done by user)
    void setFocused(bool state);

    /// Set the iconified flag for this window (should not be done by user)
    void setIconified(bool state);

    /**
     * Set the window title.
     * \param title The title of the window.
     */
    void setWindowTitle(const char* title);

    /**
     * Sets the window resolution.
     *
     * \param x The width of the window in pixels.
     * \param y The height of the window in pixels.
     */
    void setWindowResolution(glm::ivec2 resolution);

    /**
     * Sets the framebuffer resolution. Theese parameters will only be used if a fixed
     * resolution is used that is different from the window resolution.
     * This might be useful in fullscreen mode on Apples retina displays to force 1080p
     * resolution or similar.
     *
     * \param x The width of the frame buffer in pixels.
     * \param y The height of the frame buffer in pixels.
     */
    void setFramebufferResolution(glm::ivec2 resolution);

    /**
     * Set this window's position in screen coordinates
     *
     * \param x horizontal position in pixels
     * \param y vertical position in pixels
     */
    void setWindowPosition(glm::ivec2 positions);

    /// Set if fullscreen mode should be used
    void setWindowMode(bool fullscreen);

    /// Set if the window should float (be on top / topmost)
    void setFloating(bool floating);

    /**
     * Set if the window should be double buffered (can only be set before window is
     * created)
     */
    void setDoubleBuffered(bool doubleBuffered);

    /// Set if window borders should be visible
    void setWindowDecoration(bool state);

    /// Set which monitor that should be used for fullscreen mode
    void setFullScreenMonitorIndex(int index);

    /**
     * Force the frame buffer to have a fixed size which may be different from the window
     * size.
     */
    void setFixResolution(bool state);
    void setHorizFieldOfView(float hFovDeg);

    /// Set if post effects should be used.
    void setUsePostFX(bool state);

    /// Set if FXAA should be used.
    void setUseFXAA(bool state);

    /**
     * Use quad buffer (hardware stereoscopic rendering). This function can only be used
     * before the window is created. The quad buffer feature is only supported on
     * professional CAD graphics cards such as Nvidia Quadro or AMD/ATI FireGL.
     */
    void setUseQuadbuffer(bool state);

    /// Set if the specifed Draw2D function pointer should be called for this window.
    void setCallDraw2DFunction(bool state);

    /// Set if the specifed Draw3D function pointer should be called for this window.
    void setCallDraw3DFunction(bool state);

    /// Set if the specifed Draw2D function pointer should be called for this window.
    void setCopyPreviousWindowToCurrentWindow(bool state);

    /// Set the number of samples used in multisampled anti-aliasing
    void setNumberOfAASamples(int samples);

    /**
     * Set the stereo mode. Set this mode in your init callback or during runtime in the
     * post-sync-pre-draw callback. GLSL shaders will be recompliled if needed.
     */
    void setStereoMode(StereoMode sm);

    /**
     * Set the which viewport that is the current. This is done from the Engine and end
     * users shouldn't change this
     */
    void setCurrentViewport(size_t index);

    /**
     * Set the which viewport that is the current. This is done internally from SGCT and
     * end users shouldn't change this
     */
    void setCurrentViewport(core::BaseViewport* vp);

    /**
     * Set if fisheye alpha state. Should only be set using XML config of before calling
     * Engine::init.
     */
    void setAlpha(bool state);

    /// Set monitor gamma (works only if fullscreen)
    void setGamma(float gamma);

    /// Set monitor contrast in range [0.5, 1.5] (works only if fullscreen)
    void setContrast(float contrast);

    /// Set monitor brightness in range [0.5, 1.5] (works only if fullscreen)
    void setBrightness(float brightness);

    /// Set the color bit depth of the FBO and Screencapture.
    void setColorBitDepth(ColorBitDepth cbd);

    /**
     * Set if BGR(A) or RGB(A) rendering should be used. Default is BGR(A), which is
     * usually the native order on GPU hardware. This setting affects the screencapture
     * which will return the prefered color order.
     */
    void setPreferBGR(bool state);

    /// Set if screen capturing is allowed.
    void setAllowCapture(bool state);

    /// \returns true if full screen rendering is enabled
    bool isFullScreen() const;

    /// \return true if window is floating/allways on top/topmost
    bool isFloating() const;

    /// \return true if window is double-buffered
    bool isDoubleBuffered() const;

    /// \returns this window's focused flag
    bool isFocused() const;

    /// \returns this window's inconify flag 
    bool isIconified() const;

    /// \returns if the window is visible or not
    bool isVisible() const;

    /// \returns true if the window is set to render while hidden
    bool isRenderingWhileHidden() const;

    /// \returns If the frame buffer has a fix resolution this function returns true.
    bool isFixResolution() const;

    /**
     * \returns If the window resolution was set in configuration file this function
     *          returns true.
     */
    bool isWindowResolutionSet() const;

    /// \returns true if any kind of stereo is enabled
    bool isStereo() const;

    /// \returns true if this window is resized
    bool isWindowResized() const;

    /// Get if buffer is rendered using BGR(A) or RGB(A).
    bool isBGRPreferred() const;

    /// Get if (screen) capturing is allowed.
    bool isCapturingAllowed() const;
        
    /// \returns the name of this window
    const std::string& getName() const;

    /// \returns the tags of this window
    const std::vector<std::string>& getTags() const;

    /// \returns true if a specific tag exists
    bool hasTag(const std::string& tag) const;

    /// \returns this window's id
    int getId() const;

    /**
     * Get a frame buffer texture. If the texture doesn't exists then it will be created.
     *
     * \param index Index or Engine::TextureIndexes enum
     * \returns texture index of selected frame buffer texture
     */
    unsigned int getFrameBufferTexture(unsigned int index);

    /**
     * This function returns the screen capture pointer if it's set otherwise nullptr.
     *
     * \param eye can either be 0 (left) or 1 (right)
     *
     * \returns pointer to screen capture ptr
     */
    core::ScreenCapture* getScreenCapturePointer(Eye eye) const;

    /// \returns the number of samples used in multisampled anti-aliasing
    int getNumberOfAASamples() const;

    /**
     * Returns the stereo mode. The value can be compared to the
     * sgct::core::ClusterManager::StereoMode enum
     */
    StereoMode getStereoMode() const;

    /**
     * Get the dimensions of the final FBO. Regular viewport rendering renders directly to
     * this FBO but a fisheye renders first a cubemap and then to the final FBO. Post
     * effects are rendered using these dimensions.
     */
    glm::ivec2 getFinalFBODimensions() const;

    /// Returns pointer to FBO container
    core::OffScreenBuffer* getFBO() const;

    /// \returns pointer to GLFW monitor
    GLFWmonitor* getMonitor() const;

    /// \returns pointer to GLFW window
    GLFWwindow* getWindowHandle() const;

    /// \returns a pointer to the viewport that is beeing rendered to at the moment
    core::BaseViewport* getCurrentViewport() const;

    /// \returns a pointer to a specific viewport
    core::Viewport& getViewport(size_t index);

    /// \returns a pointer to a specific viewport
    const core::Viewport& getViewport(size_t index) const;

    /// Get the current viewport data in pixels.
    glm::ivec4 getCurrentViewportPixelCoords() const;

    /// \returns the viewport count for this window
    int getNumberOfViewports() const;
    std::string getStereoModeStr() const;

    /// Enable alpha clear color and 4-component screenshots
    bool getAlpha() const;

    /// Get monitor gamma value (works only if fullscreen)
    float getGamma() const;

    /// Get monitor contrast value (works only if fullscreen)
    float getContrast() const;

    /// Get monitor brightness value (works only if fullscreen)
    float getBrightness() const;

    /// Get the color bit depth of the FBO and Screencapture.
    ColorBitDepth getColorBitDepth() const;

    /// Get FOV of viewport[0]
    float getHorizFieldOfViewDegrees() const;
    
    /// \returns the pointer to a specific post effect
    PostFX& getPostFX(size_t index);

    /// \returns the number of post effects
    size_t getNumberOfPostFXs() const;

    /// \returns Get the window resolution.
    glm::ivec2 getResolution() const;

    /// \returns Get the frame buffer resolution.
    glm::ivec2 getFramebufferResolution() const;

    /// \returns Get the initial window resolution.
    glm::ivec2 getInitialResolution() const;

    /**
     * \returns Get the scale value (relation between pixel and point size). Normally this
     *          value is 1.0f but 2.0f on retina computers.
     */
    glm::vec2 getScale() const;

    /// \returns the aspect ratio of the window 
    float getAspectRatio() const;

    /// \returns Get the frame buffer bytes per color component (BPCC) count.
    int getFramebufferBPCC() const;

    void bindVAO() const;
    void bindVBO() const;
    void unbindVBO() const;
    void unbindVAO() const;

    /// Add a post effect for this window
    void addPostFX(PostFX fx);
    void addViewport(std::unique_ptr<core::Viewport> vpPtr);

    /// \return true if any masks are used
    bool hasAnyMasks() const;
    /// \returns true if FXAA should be used
    bool useFXAA() const;
    /// \returns true if PostFX pass should be used
    bool usePostFX() const;

    void bindStereoShaderProgram() const;
    int getStereoShaderMVPLoc() const;
    int getStereoShaderLeftTexLoc() const;
    int getStereoShaderRightTexLoc() const;

    bool getCallDraw2DFunction() const;
    bool getCallDraw3DFunction() const;
    bool getCopyPreviousWindowToCurrentWindow() const;

private:
    enum class TextureType { Color, Depth, Normal, Position };

    void initScreenCapture();
    /// This function creates textures that will act as FBO targets.
    void createTextures();
    void generateTexture(unsigned int& id, TextureType type);

    /**
     * This function creates FBOs if they are supported. This is done in the initOGL
     * function.
     */
    void createFBOs();

    /**
     * This function resizes the FBOs when the window is resized to achive 1:1 pixel-texel
     * mapping.
     */
    void resizeFBOs();

    void destroyFBOs();

    /// Create vertex buffer objects used to render framebuffer quad
    void createVBOs();
    void loadShaders();
    void updateTransferCurve();
    void updateColorBufferData();
    bool useRightEyeTexture() const;

    std::string mName;
    std::vector<std::string> mTags;

    bool mVisible = true;
    bool mRenderWhileHidden = false;
    bool mFocused = false;
    bool mIconified = false;
    bool mUseFixResolution = false;
    bool mIsWindowResSet = false;
    bool mAllowCapture = true;
    bool mCallDraw2DFunction = true;
    bool mCallDraw3DFunction = true;
    bool mCopyPreviousWindowToCurrentWindow = false;
    bool mUseQuadBuffer = false;
    bool mFullScreen = false;
    bool mFloating = false;
    bool mDoubleBuffered = true;
    bool mSetWindowPos = false;
    bool mDecorated = true;
    bool mAlpha = false;
    glm::ivec2 mFramebufferRes = glm::ivec2(512, 256);
    glm::ivec2 mWindowInitialRes = glm::ivec2(640, 480);
    bool mHasPendingWindowRes = false;
    glm::ivec2 mPendingWindowRes = glm::ivec2(0, 0);
    bool mHasPendingFramebufferRes = false;
    glm::ivec2 mPendingFramebufferRes = glm::ivec2(0, 0);
    glm::ivec2 mWindowRes = glm::ivec2(640, 480);
    glm::ivec2 mWindowPos = glm::ivec2(0, 0);
    glm::ivec2 mWindowResOld = glm::ivec2(640, 480);
    int mMonitorIndex = 0;
    GLFWmonitor* mMonitor = nullptr;
    GLFWwindow* mWindowHandle = nullptr;
    float mAspectRatio = 1.f;
    float mGamma = 1.f;
    float mContrast = 1.f;
    float mBrightness = 1.f;
    glm::vec2 mScale = glm::vec2(0.f, 0.f);

    bool mUseFXAA;
    bool mUsePostFX = false;

    ColorBitDepth mBufferColorBitDepth = ColorBitDepth::Depth8;
    int mInternalColorFormat;
    unsigned int mColorFormat;
    unsigned int mColorDataType;
    bool mPreferBGR = true;
    int mBytesPerColor;

    struct {
        unsigned int leftEye = 0;
        unsigned int rightEye = 0;
        unsigned int depth = 0;
        unsigned int fx1 = 0;
        unsigned int fx2 = 0;
        unsigned int intermediate = 0;
        unsigned int normals = 0;
        unsigned int positions = 0;
    } mFrameBufferTextures;

    std::unique_ptr<core::ScreenCapture> mScreenCaptureLeftOrMono;
    std::unique_ptr<core::ScreenCapture> mScreenCaptureRight;

    StereoMode mStereoMode = StereoMode::NoStereo;
    int mNumberOfAASamples;
    int mId;

    unsigned int mVAO = 0;
    unsigned int mVBO = 0;

    //Shaders
    struct {
        ShaderProgram shader;
        int mvpLoc = -1;
        int leftTexLoc = -1;
        int rightTexLoc = -1;
    } stereo;

    bool mHasAnyMasks = false;

    core::BaseViewport* mCurrentViewport = nullptr;
    std::vector<std::unique_ptr<core::Viewport>> mViewports;
    std::vector<PostFX> mPostFXPasses;
    std::unique_ptr<core::OffScreenBuffer> mFinalFBO;

    static GLFWwindow* mSharedHandle;
    static GLFWwindow* mCurrentContextOwner;
    static bool mUseSwapGroups;
    static bool mBarrier;
    static bool mSwapGroupMaster;
};

} // namespace sgct

#endif // __SGCT__WINDOW__H__
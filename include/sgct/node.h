/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2019                                                               *
 * For conditions of distribution and use, see copyright notice in sgct.h                *
 ****************************************************************************************/

#ifndef __SGCT__NODE__H__
#define __SGCT__NODE__H__

#include <sgct/window.h>
#include <string>
#include <vector>

namespace sgct::core {

class Node {
public:
    void applyNode(const config::Node& node);

    /**
     * Add a window to the window vector. Note that a window must be opened to become
     * visible.
     */
    void addWindow(Window window);

    /// Set which window that will render the draw calls.
    void setCurrentWindowIndex(int index);

    /**
     * Set to true if this node's windows should belong to a nvida swap group. Only valid
     * before window opens.
     */
    void setUseSwapGroups(bool state);

    /// Check if all windows are set to close and close them.
    bool closeAllWindows();

    /// Is this node using nvidia swap groups for it's windows?
    bool isUsingSwapGroups() const;

    /// Show all hidden windows.
    void showAllWindows();

    /// Hide all windows.
    void hideAllWindows();

    /// Check if a key is pressed for all windows.
    bool getKeyPressed(int key);

    /// Get the number of windows in the window vector
    int getNumberOfWindows() const;

    /// Get the window pointer at index in window vector.
    Window& getWindow(int index);

    /// Get the active window pointer.
    Window& getCurrentWindow();

    /// Get the current window index
    int getCurrentWindowIndex() const;

    /// \param address is the hostname, DNS-name or ip
    void setAddress(std::string address);

    /**
     * \param sync port is the number of the tcp port used for communication with this
     * node
     */
    void setSyncPort(int port);

    /**
     * \param data transfer port is the number of the tcp port used for data transfers to 
     *        this node
     */
    void setDataTransferPort(int port);

    /// \param name the name identification string of this node
    void setName(std::string name);

    /// \return the address of this node
    const std::string& getAddress() const;

    /// \return the sync port of this node
    int getSyncPort() const;

    /// \return the data transfer port of this node
    int getDataTransferPort() const;

    /// \return the name if this node
    const std::string& getName() const;

private:
    std::string _name;
    std::string _address;
    int _syncPort = 0;
    int _dataTransferPort = 0;

    int _currentWindowIndex = 0;
    std::vector<Window> _windows;
    bool _useSwapGroups = false;
};

} // namespace sgct::core

#endif // __SGCT__NODE__H__

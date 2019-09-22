/*************************************************************************
Copyright (c) 2012-2015 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h
*************************************************************************/

#include <sgct/networkmanager.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#endif

#include <sgct/clustermanager.h>
#include <sgct/engine.h>
#include <sgct/messagehandler.h>
#include <sgct/mutexmanager.h>
#include <sgct/shareddata.h>
#include <sgct/statistics.h>
#include <algorithm>
#include <numeric>

#include <zlib.h>

#ifdef WIN32
    #include <ws2tcpip.h>
#else //Use BSD sockets
    #ifdef _XCODE
        #include <unistd.h>
    #endif
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #define SOCKET_ERROR (-1)
#endif

//missing function on mingw
#if defined(__MINGW32__) || defined(__MINGW64__)
const char* inet_ntop(int af, const void* src, char* dst, int cnt) {
    sockaddr_in srcaddr;

    memset(&srcaddr, 0, sizeof(sockaddr_in));
    memcpy(&(srcaddr.sin_addr), src, sizeof(srcaddr.sin_addr));

    srcaddr.sin_family = af;
    int res = WSAAddressToString(
        reinterpret_cast<sockaddr*>(&srcaddr),
        sizeof(sockaddr_in),
        0,
        dst,
        reinterpret_cast<LPDWORD>(&cnt)
    );
    if (res != 0) {
        DWORD rv = WSAGetLastError();
        printf("WSAAddressToString() : %d\n",rv);
        return nullptr;
    }
    return dst;
}
#endif // defined(__MINGW32__) || defined(__MINGW64__)

//#define __SGCT_NETWORK_DEBUG__

namespace sgct::core {

std::condition_variable NetworkManager::gCond;
NetworkManager* NetworkManager::mInstance = nullptr;

NetworkManager* NetworkManager::instance() {
    return mInstance;
}

NetworkManager::NetworkManager(NetworkMode nm) 
    : mCompressionLevel(Z_BEST_SPEED)
    , mMode(nm)
{
    mInstance = this;

    MessageHandler::instance()->print(
        MessageHandler::Level::Debug,
        "NetworkManager: Initiating network API...\n"
    );

    initAPI();

    MessageHandler::instance()->print(
        MessageHandler::Level::Debug,
        "NetworkManager: Getting host info...\n"
    );
    getHostInfo();

    if (mMode == NetworkMode::Remote) {
        mIsServer = matchAddress(ClusterManager::instance()->getMasterAddress());
    }
    else if (mMode == NetworkMode::LocalServer) {
        mIsServer = true;
    }
    else {
        mIsServer = false;
    }

    if (mIsServer) {
        MessageHandler::instance()->print(
            MessageHandler::Level::Info,
            "NetworkManager: This computer is the network server\n"
        );
    }
    else {
        MessageHandler::instance()->print(
            MessageHandler::Level::Info,
            "NetworkManager: This computer is the network client\n"
        );
    }
}

NetworkManager::~NetworkManager() {
    close();
}

bool NetworkManager::init() {
    ClusterManager& cm = *ClusterManager::instance();
    std::string thisAddress = cm.getThisNode()->getAddress();
    if (thisAddress.empty()) {
        MessageHandler::instance()->print(
            MessageHandler::Level::Error,
            "NetworkManager: No address information for this node available\n"
        );
        return false;
    }

    std::string remoteAddress;
    if (mMode == NetworkMode::Remote) {
        remoteAddress = cm.getMasterAddress();

        if (remoteAddress.empty()) {
            MessageHandler::instance()->print(
                MessageHandler::Level::Error,
                "NetworkManager: No address information for master/host availible\n"
            );
            return false;
        }
    }
    else {
        // local (not remote)
        remoteAddress = "127.0.0.1";
    }


    // if faking an address (running local) then add it to the search list
    if (mMode != NetworkMode::Remote) {
        mLocalAddresses.push_back(cm.getThisNode()->getAddress());
    }

    // Add Cluster Functionality
    if (ClusterManager::instance()->getNumberOfNodes() > 1) {
        // sanity check if port is used somewhere else
        for (size_t i = 0; i < mNetworkConnections.size(); i++) {
            const std::string& port = mNetworkConnections[i]->getPort();
            if (port == cm.getThisNode()->getSyncPort() ||
                port == cm.getThisNode()->getDataTransferPort() ||
                port == cm.getExternalControlPort())
            {
                MessageHandler::instance()->print(
                    MessageHandler::Level::Error,
                    "NetworkManager: Port %s is already used by connection %u\n",
                    cm.getThisNode()->getSyncPort().c_str(), i
                );
                return false;
            }
        }

        // if client
        if (!mIsServer) {
            bool addSyncPort = addConnection(
                cm.getThisNode()->getSyncPort(),
                remoteAddress
            );
            if (addSyncPort) {
                mNetworkConnections.back()->setDecodeFunction(
                    [](const char* data, int length, int index) {
                        SharedData::instance()->decode(data, length, index);
                    }
                );
            }
            else {
                MessageHandler::instance()->print(
                    MessageHandler::Level::Error,
                    "NetworkManager: Failed to add network connection to %s\n",
                    cm.getMasterAddress().c_str()
                );
                return false;
            }

            // add data transfer connection
            bool addTransferPort = addConnection(
                cm.getThisNode()->getDataTransferPort(),
                remoteAddress,
                Network::ConnectionType::DataTransfer
            );
            if (addTransferPort) {
                mNetworkConnections.back()->setPackageDecodeFunction(
                    [](void* data, int length, int packageId, int clientId) {
                    Engine::instance()->invokeDecodeCallbackForDataTransfer(
                            data,
                            length,
                            packageId,
                            clientId
                        );
                    }
                );

                // acknowledge callback
                mNetworkConnections.back()->setAcknowledgeFunction(
                    [](int packageId, int clientId) {
                        Engine::instance()->invokeAcknowledgeCallbackForDataTransfer(
                            packageId,
                            clientId
                        );
                    }
                );
            }
        }

        // add all connections from config file
        for (unsigned int i = 0; i < cm.getNumberOfNodes(); i++) {
            // don't add itself if server
            if (mIsServer && !matchAddress(cm.getNode(i)->getAddress())) {
                bool addSyncPort = addConnection(
                    cm.getNode(i)->getSyncPort(),
                    remoteAddress
                );
                if (!addSyncPort) {
                    MessageHandler::instance()->print(
                        MessageHandler::Level::Error,
                        "NetworkManager: Failed to add network connection to %s!\n",
                        cm.getNode(i)->getAddress().c_str()
                    );
                    return false;
                }
                else {
                    mNetworkConnections.back()->setDecodeFunction(
                        [](const char* data, int length, int index) {
                            MessageHandler::instance()->decode(
                                std::vector<char>(data, data + length),
                                index
                            );
                        }
                    );
                }

                // add data transfer connection
                bool addTransferPort = addConnection(
                    cm.getNode(i)->getDataTransferPort(),
                    remoteAddress,
                    Network::ConnectionType::DataTransfer
                );
                if (addTransferPort) {
                    mNetworkConnections.back()->setPackageDecodeFunction(
                        [](void* data, int length, int packageId, int clientId) {
                            Engine::instance()->invokeDecodeCallbackForDataTransfer(
                                data,
                                length,
                                packageId,
                                clientId
                            );
                        }
                    );

                    // acknowledge callback
                    mNetworkConnections.back()->setAcknowledgeFunction(
                        [](int packageId, int clientId) {
                            Engine::instance()->invokeAcknowledgeCallbackForDataTransfer(
                                packageId,
                                clientId
                            );
                        }
                    );
                }
            }
        }
    }

    // add connection for external communication
    if (mIsServer) {
        bool addExternalPort = addConnection(
            cm.getExternalControlPort(),
            "127.0.0.1",
            cm.getUseASCIIForExternalControl() ?
                Network::ConnectionType::ExternalASCIIConnection :
                Network::ConnectionType::ExternalRawConnection
        );
        if (addExternalPort) {
            mNetworkConnections.back()->setDecodeFunction(
                [](const char* data, int length, int client) {
                    Engine::instance()->invokeDecodeCallbackForExternalControl(
                        data,
                        length,
                        client
                    );
                }
            );
        }
    }

    MessageHandler::instance()->print(
        MessageHandler::Level::Debug,
        "NetworkManager: Cluster sync is set to %s\n",
        cm.getFirmFrameLockSyncStatus() ? "firm/strict" : "loose"
    );

    return true;
}

void NetworkManager::sync(SyncMode sm, Statistics& stats) {
    if (sm == SyncMode::SendDataToClients) {
        double maxTime = -std::numeric_limits<double>::max();
        double minTime = std::numeric_limits<double>::max();

        for (Network* connection : mSyncConnections) {
            if (!connection->isServer() || !connection->isConnected()) {
                continue;
            }

            double currentTime = connection->getLoopTime();
            maxTime = std::max(currentTime, maxTime);
            minTime = std::min(currentTime, minTime);

            int currentSize =
                static_cast<int>(SharedData::instance()->getDataSize()) -
                Network::HeaderSize;

            // iterate counter
            int currentFrame = connection->iterateFrameCounter();

            unsigned char* dataBlock = SharedData::instance()->getDataBlock();
            std::memcpy(dataBlock + 1, &currentFrame, sizeof(int));
            std::memcpy(dataBlock + 5, &currentSize, sizeof(int));

            connection->sendData(
                SharedData::instance()->getDataBlock(),
                static_cast<int>(SharedData::instance()->getDataSize())
            );
        }

        if (isComputerServer()) {
            stats.setLoopTime(static_cast<float>(minTime), static_cast<float>(maxTime));
        }
    }
    else if (sm == SyncMode::AcknowledgeData) {
        for (Network* connection : mSyncConnections) {
            // Client
            if (!connection->isServer() && connection->isConnected()) {
                // The servers's render function is locked until a message starting with
                // the ack-byte is received.
                // send message to server
                connection->pushClientMessage();
            }
        }
    }
}

bool NetworkManager::isSyncComplete() {
    unsigned int counter = static_cast<unsigned int>(
        std::count_if(
            mSyncConnections.begin(),
            mSyncConnections.end(),
            [](Network* n) { return n->isUpdated(); }
        )
    );
#ifdef __SGCT_NETWORK_DEBUG__
    MessageHandler::instance()->printDebug(
        MessageHandler::Level::Debug,
        "SGCTNetworkManager::isSyncComplete: counter %u of %u\n",
        counter, getSyncConnectionsCount()
    );
#endif

    return counter == getActiveSyncConnectionsCount();
}

Network* NetworkManager::getExternalControlPtr() {
    return mExternalControlConnection;
}

void NetworkManager::transferData(const void* data, int length, int packageId) {
    std::vector<char> buffer;
    const bool success = prepareTransferData(data, buffer, length, packageId);
    if (success) {
        for (Network* connection : mDataTransferConnections) {
            if (connection->isConnected()) {
                connection->sendData(buffer.data(), length);
            }
        }
    }
}

void NetworkManager::transferData(const void* data, int length, int packageId,
                                  size_t nodeIndex)
{
    if (nodeIndex >= mDataTransferConnections.size() ||
        !mDataTransferConnections[nodeIndex]->isConnected())
    {
        return;

    }
    std::vector<char> buffer;
    const bool success = prepareTransferData(data, buffer, length, packageId);
    if (success) {
        mDataTransferConnections[nodeIndex]->sendData(buffer.data(), length);
    }
}

void NetworkManager::transferData(const void* data, int length, int packageId,
                                  Network* connection)
{
    if (connection->isConnected()) {
        std::vector<char> buffer;
        const bool success = prepareTransferData(data, buffer, length, packageId);
        if (success) {
            connection->sendData(buffer.data(), length);
        }
    }
}

bool NetworkManager::prepareTransferData(const void* data, std::vector<char>& buffer,
                                         int& length, int packageId)
{
    int messageLength = length;

    if (mCompress) {
        length = static_cast<int>(compressBound(static_cast<uLong>(length)));
    }
    length += static_cast<int>(Network::HeaderSize);

    buffer.resize(length);

    buffer[0] = static_cast<char>(
        mCompress ? Network::CompressedDataId : Network::DataId
    );
    memcpy(buffer.data() + 1, &packageId, sizeof(int));

    if (mCompress) {
        char* compDataPtr = buffer.data() + Network::HeaderSize;
        uLong compressedSize = static_cast<uLongf>(length - Network::HeaderSize);
        int err = compress2(
            reinterpret_cast<Bytef*>(compDataPtr),
            &compressedSize,
            reinterpret_cast<const Bytef*>(data),
            static_cast<uLong>(length),
            mCompressionLevel
        );

        if (err != Z_OK) {
            std::string errStr;
            switch (err) {
                case Z_BUF_ERROR:
                    errStr = "Dest. buffer not large enough";
                    break;
                case Z_MEM_ERROR:
                    errStr = "Insufficient memory";
                    break;
                case Z_STREAM_ERROR:
                    errStr = "Incorrect compression level";
                    break;
                default:
                    errStr = "Unknown error";
                    break;
            }

            MessageHandler::instance()->print(
                MessageHandler::Level::Error,
                "NetworkManager: Failed to compress data! Error: %s\n",
                errStr.c_str()
            );
            return false;
        }

        // send original size
        std::memcpy(buffer.data() + 9, &length, sizeof(int));

        length = static_cast<int>(compressedSize);
        // re-calculate the true send size
        length = length + static_cast<int>(Network::HeaderSize);
    }
    else {
        // set uncompressed size to DefaultId since compression is not used
        memset(buffer.data() + 9, Network::DefaultId, sizeof(int));

        // add data to buffer
        memcpy(buffer.data() + Network::HeaderSize, data, length - Network::HeaderSize);
        //int offset = 0;
        //int stride = 4096;

        //while (offset < messageLength) {
        //    if ((messageLength - offset) < stride) {
        //        stride = messageLength - offset;
        //    }

        //    memcpy(
        //        (*bufferPtr) + Network::HeaderSize + offset,
        //        reinterpret_cast<const char*>(data)+offset,
        //        stride
        //    );
        //    offset += stride;
        //}
    }

    std::memcpy(buffer.data() + 5, &messageLength, sizeof(int));

    return true;
}

void NetworkManager::setDataTransferCompression(bool state, int level) {
    mCompress = state;
    mCompressionLevel = level;
}

unsigned int NetworkManager::getActiveConnectionsCount() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return mNumberOfActiveConnections;
}

unsigned int NetworkManager::getActiveSyncConnectionsCount() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return mNumberOfActiveSyncConnections;
}

unsigned int NetworkManager::getActiveDataTransferConnectionsCount() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return mNumberOfActiveDataTransferConnections;
}

int NetworkManager::getConnectionsCount() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return static_cast<int>(mNetworkConnections.size());
}

int NetworkManager::getSyncConnectionsCount() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return static_cast<int>(mSyncConnections.size());
}

int NetworkManager::getDataTransferConnectionsCount() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return static_cast<int>(mDataTransferConnections.size());
}

const Network& NetworkManager::getConnectionByIndex(unsigned int index) const {
    return *mNetworkConnections[index];
}

Network* NetworkManager::getSyncConnectionByIndex(unsigned int index) const {
    return mSyncConnections[index];
}

const std::vector<std::string>& NetworkManager::getLocalAddresses() const {
    return mLocalAddresses;
}

void NetworkManager::updateConnectionStatus(Network* connection) {
    MessageHandler::instance()->print(
        MessageHandler::Level::Debug,
        "NetworkManager: Updating status for connection %d\n", connection->getId()
    );

    unsigned int numberOfConnectionsCounter = 0;
    unsigned int numberOfConnectedSyncNodesCounter = 0;
    unsigned int numberOfConnectedDataTransferNodesCounter = 0;

    MutexManager::instance()->mDataSyncMutex.lock();
    unsigned int totalNumberOfConnections =
        static_cast<unsigned int>(mNetworkConnections.size());
    unsigned int totalNumberOfSyncConnections =
        static_cast<unsigned int>(mSyncConnections.size());
    unsigned int totalNumberOfTransferConnections =
        static_cast<unsigned int>(mDataTransferConnections.size());
    MutexManager::instance()->mDataSyncMutex.unlock();

    // count connections
    for (const std::unique_ptr<Network>& conn : mNetworkConnections) {
        if (!conn->isConnected()) {
            continue;
        }

        numberOfConnectionsCounter++;
        if (conn->getType() == Network::ConnectionType::SyncConnection) {
            numberOfConnectedSyncNodesCounter++;
        }
        else if (conn->getType() == Network::ConnectionType::DataTransfer) {
            numberOfConnectedDataTransferNodesCounter++;
        }
    }

    MessageHandler::instance()->print(
        MessageHandler::Level::Info,
        "NetworkManager: Number of active connections %u of %u\n",
        numberOfConnectionsCounter, totalNumberOfConnections
    );
    MessageHandler::instance()->print(
        MessageHandler::Level::Debug,
        "NetworkManager: Number of connected sync nodes %u of %u\n",
        numberOfConnectedSyncNodesCounter, totalNumberOfSyncConnections
    );
    MessageHandler::instance()->print(
        MessageHandler::Level::Debug,
        "NetworkManager: Number of connected data transfer nodes %u of %u\n",
        numberOfConnectedDataTransferNodesCounter, totalNumberOfTransferConnections
    );

    MutexManager::instance()->mDataSyncMutex.lock();
    mNumberOfActiveConnections = numberOfConnectionsCounter;
    mNumberOfActiveSyncConnections = numberOfConnectedSyncNodesCounter;
    mNumberOfActiveDataTransferConnections = numberOfConnectedDataTransferNodesCounter;


    // if client disconnects then it cannot run anymore
    if (mNumberOfActiveSyncConnections == 0 && !mIsServer) {
        mIsRunning = false;
    }
    MutexManager::instance()->mDataSyncMutex.unlock();

    if (mIsServer) {
        MutexManager::instance()->mDataSyncMutex.lock();
        // local copy (thread safe)
        bool allNodesConnectedCopy =
            (numberOfConnectedSyncNodesCounter == totalNumberOfSyncConnections) &&
            (numberOfConnectedDataTransferNodesCounter == totalNumberOfTransferConnections);

        mAllNodesConnected = allNodesConnectedCopy;
        MutexManager::instance()->mDataSyncMutex.unlock();

        // send cluster connected message to nodes/slaves
        if (allNodesConnectedCopy) {
            for (unsigned int i = 0; i < mSyncConnections.size(); i++) {
                if (!mSyncConnections[i]->isConnected()) {
                    continue;
                }
                char data[Network::HeaderSize];
                std::fill(
                    std::begin(data),
                    std::end(data),
                    static_cast<char>(Network::DefaultId)
                );
                data[0] = Network::ConnectedId;

                mSyncConnections[i]->sendData(&data, Network::HeaderSize);
            }
            for (unsigned int i = 0; i < mDataTransferConnections.size(); i++) {
                if (mDataTransferConnections[i]->isConnected()) {
                    char data[Network::HeaderSize];
                    std::fill(
                        std::begin(data),
                        std::end(data),
                        static_cast<char>(Network::DefaultId)
                    );
                    data[0] = Network::ConnectedId;
                    mDataTransferConnections[i]->sendData(&data, Network::HeaderSize);
                }
            }
        }

        // Check if any external connection
        if (connection->getType() == Network::ConnectionType::ExternalASCIIConnection) {
            bool externalControlConnectionStatus = connection->isConnected();
            std::string msg = "Connected to SGCT!\r\n";
            connection->sendData(msg.c_str(), static_cast<int>(msg.size()));
            Engine::instance()->invokeUpdateCallbackForExternalControl(
                externalControlConnectionStatus
            );
        }
        else if (connection->getType() == Network::ConnectionType::ExternalRawConnection)
        {
            bool externalControlConnectionStatus = connection->isConnected();
            Engine::instance()->invokeUpdateCallbackForExternalControl(
                externalControlConnectionStatus
            );
        }

        // wake up the connection handler thread on server
        // if node disconnects to enable reconnection
        connection->mStartConnectionCond.notify_all();
    }


    if (connection->getType() == Network::ConnectionType::DataTransfer) {
        const bool dataTransferConnectionStatus = connection->isConnected();
        Engine::instance()->invokeUpdateCallbackForDataTransfer(
            dataTransferConnectionStatus,
            connection->getId()
        );
    }

    // signal done to caller
    gCond.notify_all();
}

void NetworkManager::setAllNodesConnected() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);

    if (!mIsServer) {
        unsigned int totalNumberOfTransferConnections =
            static_cast<unsigned int>(mDataTransferConnections.size());
        mAllNodesConnected = (mNumberOfActiveSyncConnections == 1) &&
            (mNumberOfActiveDataTransferConnections == totalNumberOfTransferConnections);
    }
}

void NetworkManager::close() {
    mIsRunning = false;

    // release condition variables
    gCond.notify_all();

    // signal to terminate
    for (std::unique_ptr<Network>& connection : mNetworkConnections) {
        connection->initShutdown();
    }

    // wait for all nodes callbacks to run
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // wait for threads to die
    for (std::unique_ptr<Network>& connection : mNetworkConnections) {
        connection->closeNetwork(false);
    }

    mNetworkConnections.clear();
    mSyncConnections.clear();
    mDataTransferConnections.clear();

#ifdef _WIN_PLATFORM
    WSACleanup();
#endif
    MessageHandler::instance()->print(
        MessageHandler::Level::Info,
        "NetworkManager: Network API closed\n"
    );
}

bool NetworkManager::addConnection(const std::string& port, const std::string& address,
                                   Network::ConnectionType connectionType)
{
    if (port.empty()) {
        MessageHandler::instance()->print(
            MessageHandler::Level::Info,
            "NetworkManager: No port set for %s\n",
            Network::getTypeStr(connectionType).c_str()
        );
        return false;
    }

    if (address.empty()) {
        MessageHandler::instance()->print(
            MessageHandler::Level::Error,
            "NetworkManager: Error: No address set for %s\n",
            Network::getTypeStr(connectionType).c_str()
        );
        return false;
    }

    try {
        std::unique_ptr<Network> netPtr = std::make_unique<Network>();
        MessageHandler::instance()->print(
            MessageHandler::Level::Debug,
            "NetworkManager: Initiating network connection %d at port %s\n",
            mNetworkConnections.size(), port.c_str()
        );
        netPtr->setUpdateFunction([this](Network* c) { updateConnectionStatus(c); });
        netPtr->setConnectedFunction([this]() { setAllNodesConnected(); });

        // must be initialized after binding
        netPtr->init(port, address, mIsServer, connectionType);
        mNetworkConnections.push_back(std::move(netPtr));
    }
    catch (const std::runtime_error& e) {
        MessageHandler::instance()->print(
            MessageHandler::Level::Error,
            "NetworkManager: Network error: %s\n", e.what()
        );
        return false;
    }
    catch (const char* err) {
        // abock (2019-08-17):  I don't think catch block is necessary anymore as we now
        //                      throw the correct type from Network, but I don't dare
        //                      to remove it yet
        MessageHandler::instance()->print(
            MessageHandler::Level::Error,
            "NetworkManager: Network error: %s\n", err
        );
        return false;
    }

    // Update the previously existing shortcuts (maybe remove them altogether?)
    mSyncConnections.clear();
    mDataTransferConnections.clear();
    mExternalControlConnection = nullptr;

    for (std::unique_ptr<Network>& connection : mNetworkConnections) {
        switch (connection->getType()) {
            case Network::ConnectionType::SyncConnection:
                mSyncConnections.push_back(connection.get());
                break;
            case Network::ConnectionType::DataTransfer:
                mDataTransferConnections.push_back(connection.get());
                break;
            default:
                mExternalControlConnection = connection.get();
                break;
        }
    }

    return true;
}

void NetworkManager::initAPI() {
#ifdef WIN32
    WORD version = MAKEWORD(2, 2);

    WSADATA wsaData;
    int error = WSAStartup(version, &wsaData);

    if (error != 0 || LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        // incorrect WinSock version
        WSACleanup();
        throw std::runtime_error("Winsock 2.2 startup failed");
    }
#endif
}

void NetworkManager::getHostInfo() {
    // get name & local ips
    // retrieves the standard host name for the local computer
    char tmpStr[128];
    if (gethostname(tmpStr, sizeof(tmpStr)) == SOCKET_ERROR) {
#ifdef _WIN_PLATFORM
        WSACleanup();
#endif
        throw std::runtime_error("Failed to get host name");
    }

    mHostName = tmpStr;
    // add hostname and adress in lower case
    std::transform(
        mHostName.begin(),
        mHostName.end(),
        mHostName.begin(),
        [](char c) { return static_cast<char>(::tolower(c)); }
    );
    mLocalAddresses.push_back(mHostName);

    addrinfo hints;
    sockaddr_in* sockaddr_ipv4;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    // hints.ai_family = AF_UNSPEC; // either IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    addrinfo* info;
    int result = getaddrinfo(tmpStr, "http", &hints, &info);
    if (result != 0) {
        MessageHandler::instance()->print(
            MessageHandler::Level::Error,
            "NetworkManager: Failed to get address info (error %d)\n",
            Network::getLastError()
        );
    }
    else {
        char addr_str[INET_ADDRSTRLEN];
        for (addrinfo* p = info; p != nullptr; p = p->ai_next) {
            sockaddr_ipv4 = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), addr_str, INET_ADDRSTRLEN);
            if (p->ai_canonname) {
                mDNSNames.push_back(p->ai_canonname);
            }
            mLocalAddresses.push_back(addr_str);
        }
    }

    freeaddrinfo(info);

    for (std::string& dns : mDNSNames) {
        std::transform(
            dns.begin(),
            dns.end(),
            dns.begin(),
            [](char c) { return static_cast<char>(::tolower(c)); }
        );
        mLocalAddresses.push_back(dns);
    }

    // add the loop-back
    mLocalAddresses.push_back("127.0.0.1");
    mLocalAddresses.push_back("localhost");
}

bool NetworkManager::matchAddress(const std::string& address) {
    auto it = std::find(mLocalAddresses.begin(), mLocalAddresses.end(), address);
    return it != mLocalAddresses.end();
}

bool NetworkManager::isComputerServer() {
    return mIsServer;
}

bool NetworkManager::isRunning() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return mIsRunning;
}

bool NetworkManager::areAllNodesConnected() {
    std::unique_lock lock(MutexManager::instance()->mDataSyncMutex);
    return mAllNodesConnected;
}

void NetworkManager::retrieveNodeId() {
    for (size_t i = 0; i < ClusterManager::instance()->getNumberOfNodes(); i++) {
        // check ip
        if (matchAddress(ClusterManager::instance()->getNode(i)->getAddress())) {
            ClusterManager::instance()->setThisNodeId(static_cast<int>(i));
            MessageHandler::instance()->print(
                MessageHandler::Level::Debug,
                "NetworkManager: Running in cluster mode as node %d\n",
                ClusterManager::instance()->getThisNodeId()
            );
            break;
        }
    }
}

} // namespace sgct::core
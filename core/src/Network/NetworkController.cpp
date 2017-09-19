#include "Network/NetworkController.h"

#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QReadWriteLock>
#include <QThread>

#include <unordered_map>

Q_LOGGING_CATEGORY(LOG_NetworkController, "NetworkController")

struct NetworkController::NetworkControllerPrivate {
    explicit NetworkControllerPrivate(NetworkController *parent) : m_WorkingMutex{} {}

    void lockRead() { m_Lock.lockForRead(); }
    void lockWrite() { m_Lock.lockForWrite(); }
    void unlock() { m_Lock.unlock(); }

    QMutex m_WorkingMutex;

    QReadWriteLock m_Lock;
    std::unordered_map<QNetworkReply *, QUuid> m_NetworkReplyToVariableId;
    std::unique_ptr<QNetworkAccessManager> m_AccessManager{nullptr};
};

NetworkController::NetworkController(QObject *parent)
        : QObject(parent), impl{spimpl::make_unique_impl<NetworkControllerPrivate>(this)}
{
}

void NetworkController::onProcessRequested(std::shared_ptr<QNetworkRequest> request,
                                           QUuid identifier,
                                           std::function<void(QNetworkReply *, QUuid)> callback)
{
    qCDebug(LOG_NetworkController()) << tr("NetworkController onProcessRequested")
                                     << QThread::currentThread()->objectName() << &request;
    auto reply = impl->m_AccessManager->get(*request);

    // Store the couple reply id
    impl->lockWrite();
    impl->m_NetworkReplyToVariableId[reply] = identifier;
    impl->unlock();

    auto onReplyFinished = [request, reply, this, identifier, callback]() {

        qCDebug(LOG_NetworkController()) << tr("NetworkController onReplyFinished")
                                         << QThread::currentThread() << request.get() << reply;
        impl->lockRead();
        auto it = impl->m_NetworkReplyToVariableId.find(reply);
        impl->unlock();
        if (it != impl->m_NetworkReplyToVariableId.cend()) {
            impl->lockWrite();
            impl->m_NetworkReplyToVariableId.erase(reply);
            impl->unlock();
            // Deletes reply
            callback(reply, identifier);
            reply->deleteLater();
        }

        qCDebug(LOG_NetworkController()) << tr("NetworkController onReplyFinished END")
                                         << QThread::currentThread() << reply;
    };

    auto onReplyProgress = [reply, request, this](qint64 bytesRead, qint64 totalBytes) {

        double progress = (bytesRead * 100.0) / totalBytes;
        qCDebug(LOG_NetworkController()) << tr("NetworkController onReplyProgress") << progress
                                         << QThread::currentThread() << request.get() << reply;
        impl->lockRead();
        auto it = impl->m_NetworkReplyToVariableId.find(reply);
        impl->unlock();
        if (it != impl->m_NetworkReplyToVariableId.cend()) {
            emit this->replyDownloadProgress(it->second, request, progress);
        }
        qCDebug(LOG_NetworkController()) << tr("NetworkController onReplyProgress END")
                                         << QThread::currentThread() << reply;
    };


    connect(reply, &QNetworkReply::finished, this, onReplyFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, onReplyProgress);
    qCDebug(LOG_NetworkController()) << tr("NetworkController registered END")
                                     << QThread::currentThread()->objectName() << reply;
}

void NetworkController::initialize()
{
    qCDebug(LOG_NetworkController()) << tr("NetworkController init") << QThread::currentThread();
    impl->m_WorkingMutex.lock();
    impl->m_AccessManager = std::make_unique<QNetworkAccessManager>();


    auto onReplyErrors = [this](QNetworkReply *reply, const QList<QSslError> &errors) {

        qCCritical(LOG_NetworkController()) << tr("NetworkAcessManager errors: ") << errors;

    };


    connect(impl->m_AccessManager.get(), &QNetworkAccessManager::sslErrors, this, onReplyErrors);

    qCDebug(LOG_NetworkController()) << tr("NetworkController init END");
}

void NetworkController::finalize()
{
    impl->m_WorkingMutex.unlock();
}

void NetworkController::onReplyCanceled(QUuid identifier)
{
    auto findReply = [identifier](const auto &entry) { return identifier == entry.second; };
    qCDebug(LOG_NetworkController()) << tr("NetworkController onReplyCanceled")
                                     << QThread::currentThread();


    impl->lockRead();
    auto end = impl->m_NetworkReplyToVariableId.cend();
    auto it = std::find_if(impl->m_NetworkReplyToVariableId.cbegin(), end, findReply);
    impl->unlock();
    if (it != end) {
        it->first->abort();
    }
    qCDebug(LOG_NetworkController()) << tr("NetworkController onReplyCanceled END")
                                     << QThread::currentThread();
}

void NetworkController::waitForFinish()
{
    QMutexLocker locker{&impl->m_WorkingMutex};
}

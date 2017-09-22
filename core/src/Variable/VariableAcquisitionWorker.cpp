#include "Variable/VariableAcquisitionWorker.h"

#include "Variable/Variable.h"

#include <Data/AcquisitionRequest.h>
#include <Data/SqpRange.h>

#include <unordered_map>
#include <utility>

#include <QMutex>
#include <QReadWriteLock>
#include <QThread>

#include <cmath>

Q_LOGGING_CATEGORY(LOG_VariableAcquisitionWorker, "VariableAcquisitionWorker")

struct VariableAcquisitionWorker::VariableAcquisitionWorkerPrivate {

    explicit VariableAcquisitionWorkerPrivate(VariableAcquisitionWorker *parent)
            : m_Lock{QReadWriteLock::Recursive}, q{parent}
    {
    }

    void lockRead() { m_Lock.lockForRead(); }
    void lockWrite() { m_Lock.lockForWrite(); }
    void unlock() { m_Lock.unlock(); }

    void removeVariableRequest(QUuid vIdentifier);

    /// Remove the current request and execute the next one if exist
    void updateToNextRequest(QUuid vIdentifier);

    QMutex m_WorkingMutex;
    QReadWriteLock m_Lock;

    std::map<QUuid, QVector<AcquisitionDataPacket> > m_AcqIdentifierToAcqDataPacketVectorMap;
    std::map<QUuid, AcquisitionRequest> m_AcqIdentifierToAcqRequestMap;
    std::map<QUuid, std::pair<QUuid, QUuid> > m_VIdentifierToCurrrentAcqIdNextIdPairMap;
    VariableAcquisitionWorker *q;
};


VariableAcquisitionWorker::VariableAcquisitionWorker(QObject *parent)
        : QObject{parent}, impl{spimpl::make_unique_impl<VariableAcquisitionWorkerPrivate>(this)}
{
}

VariableAcquisitionWorker::~VariableAcquisitionWorker()
{
    qCInfo(LOG_VariableAcquisitionWorker()) << tr("VariableAcquisitionWorker destruction")
                                            << QThread::currentThread();
    this->waitForFinish();
}


QUuid VariableAcquisitionWorker::pushVariableRequest(QUuid varRequestId, QUuid vIdentifier,
                                                     SqpRange rangeRequested,
                                                     SqpRange cacheRangeRequested,
                                                     DataProviderParameters parameters,
                                                     std::shared_ptr<IDataProvider> provider)
{
    qCDebug(LOG_VariableAcquisitionWorker())
        << tr("TORM VariableAcquisitionWorker::pushVariableRequest ") << cacheRangeRequested;
    auto varRequestIdCanceled = QUuid();

    // Request creation
    auto acqRequest = AcquisitionRequest{};
    qCInfo(LOG_VariableAcquisitionWorker()) << tr("TpushVariableRequest ") << vIdentifier;
    acqRequest.m_VarRequestId = varRequestId;
    acqRequest.m_vIdentifier = vIdentifier;
    acqRequest.m_DataProviderParameters = parameters;
    acqRequest.m_RangeRequested = rangeRequested;
    acqRequest.m_CacheRangeRequested = cacheRangeRequested;
    acqRequest.m_Size = parameters.m_Times.size();
    acqRequest.m_Provider = provider;


    // Register request
    impl->lockWrite();
    impl->m_AcqIdentifierToAcqRequestMap.insert(
        std::make_pair(acqRequest.m_AcqIdentifier, acqRequest));

    auto it = impl->m_VIdentifierToCurrrentAcqIdNextIdPairMap.find(vIdentifier);
    if (it != impl->m_VIdentifierToCurrrentAcqIdNextIdPairMap.cend()) {
        // A current request already exists, we can replace the next one
        auto nextAcqId = it->second.second;
        auto acqIdentifierToAcqRequestMapIt = impl->m_AcqIdentifierToAcqRequestMap.find(nextAcqId);
        if (acqIdentifierToAcqRequestMapIt != impl->m_AcqIdentifierToAcqRequestMap.cend()) {
            auto request = acqIdentifierToAcqRequestMapIt->second;
            varRequestIdCanceled = request.m_VarRequestId;
        }

        it->second.second = acqRequest.m_AcqIdentifier;
        impl->unlock();
    }
    else {
        // First request for the variable, it must be stored and executed
        impl->m_VIdentifierToCurrrentAcqIdNextIdPairMap.insert(
            std::make_pair(vIdentifier, std::make_pair(acqRequest.m_AcqIdentifier, QUuid())));
        impl->unlock();

        QMetaObject::invokeMethod(this, "onExecuteRequest", Qt::QueuedConnection,
                                  Q_ARG(QUuid, acqRequest.m_AcqIdentifier));
    }

    return varRequestIdCanceled;
}

void VariableAcquisitionWorker::abortProgressRequested(QUuid vIdentifier)
{
    impl->lockRead();

    auto it = impl->m_VIdentifierToCurrrentAcqIdNextIdPairMap.find(vIdentifier);
    if (it != impl->m_VIdentifierToCurrrentAcqIdNextIdPairMap.cend()) {
        auto currentAcqId = it->second.first;

        auto it = impl->m_AcqIdentifierToAcqRequestMap.find(currentAcqId);
        if (it != impl->m_AcqIdentifierToAcqRequestMap.cend()) {
            auto request = it->second;
            impl->unlock();

            // Remove the current request from the worker

            impl->lockWrite();
            impl->updateToNextRequest(vIdentifier);
            impl->unlock();

            // notify the request aborting to the provider
            request.m_Provider->requestDataAborting(currentAcqId);
        }
        else {
            impl->unlock();
            qCWarning(LOG_VariableAcquisitionWorker())
                << tr("Impossible to abort an unknown acquisition request") << currentAcqId;
        }
    }
    else {
        impl->unlock();
    }
}

void VariableAcquisitionWorker::onVariableRetrieveDataInProgress(QUuid acqIdentifier,
                                                                 double progress)
{
    qCDebug(LOG_VariableAcquisitionWorker()) << tr("TORM: onVariableRetrieveDataInProgress ")
                                             << acqIdentifier << progress;
    impl->lockRead();
    auto aIdToARit = impl->m_AcqIdentifierToAcqRequestMap.find(acqIdentifier);
    if (aIdToARit != impl->m_AcqIdentifierToAcqRequestMap.cend()) {
        auto currentPartSize = (aIdToARit->second.m_Size != 0) ? 100 / aIdToARit->second.m_Size : 0;

        auto currentPartProgress
            = std::isnan(progress) ? 0.0 : (progress * currentPartSize) / 100.0;
        auto currentAlreadyProgress = aIdToARit->second.m_Progression * currentPartSize;

        auto finalProgression = currentAlreadyProgress + currentPartProgress;
        emit variableRequestInProgress(aIdToARit->second.m_vIdentifier, finalProgression);
        qCDebug(LOG_VariableAcquisitionWorker())
            << tr("TORM: onVariableRetrieveDataInProgress ")
            << QThread::currentThread()->objectName() << aIdToARit->second.m_vIdentifier
            << currentPartSize << currentAlreadyProgress << currentPartProgress << finalProgression;
        if (finalProgression == 100.0) {
            emit variableRequestInProgress(aIdToARit->second.m_vIdentifier, 0.0);
        }
    }
    impl->unlock();
}

void VariableAcquisitionWorker::onVariableAcquisitionFailed(QUuid acqIdentifier)
{
    qCDebug(LOG_VariableAcquisitionWorker()) << tr("onVariableAcquisitionFailed")
                                             << QThread::currentThread();
    impl->lockRead();
    auto it = impl->m_AcqIdentifierToAcqRequestMap.find(acqIdentifier);
    if (it != impl->m_AcqIdentifierToAcqRequestMap.cend()) {
        auto request = it->second;
        impl->unlock();
        qCInfo(LOG_VariableAcquisitionWorker()) << tr("onVariableAcquisitionFailed")
                                                << acqIdentifier << request.m_vIdentifier
                                                << QThread::currentThread();
        emit variableCanceledRequested(request.m_vIdentifier);
    }
    else {
        impl->unlock();
        // TODO log no acqIdentifier recognized
    }
}

void VariableAcquisitionWorker::onVariableDataAcquired(QUuid acqIdentifier,
                                                       std::shared_ptr<IDataSeries> dataSeries,
                                                       SqpRange dataRangeAcquired)
{
    qCDebug(LOG_VariableAcquisitionWorker()) << tr("TORM: onVariableDataAcquired on range ")
                                             << acqIdentifier << dataRangeAcquired;
    impl->lockWrite();
    auto aIdToARit = impl->m_AcqIdentifierToAcqRequestMap.find(acqIdentifier);
    if (aIdToARit != impl->m_AcqIdentifierToAcqRequestMap.cend()) {
        // Store the result
        auto dataPacket = AcquisitionDataPacket{};
        dataPacket.m_Range = dataRangeAcquired;
        dataPacket.m_DateSeries = dataSeries;

        auto aIdToADPVit = impl->m_AcqIdentifierToAcqDataPacketVectorMap.find(acqIdentifier);
        if (aIdToADPVit != impl->m_AcqIdentifierToAcqDataPacketVectorMap.cend()) {
            // A current request result already exists, we can update it
            aIdToADPVit->second.push_back(dataPacket);
        }
        else {
            // First request result for the variable, it must be stored
            impl->m_AcqIdentifierToAcqDataPacketVectorMap.insert(
                std::make_pair(acqIdentifier, QVector<AcquisitionDataPacket>() << dataPacket));
        }


        // Decrement the counter of the request
        auto &acqRequest = aIdToARit->second;
        acqRequest.m_Progression = acqRequest.m_Progression + 1;

        // if the counter is 0, we can return data then run the next request if it exists and
        // removed the finished request
        if (acqRequest.m_Size == acqRequest.m_Progression) {
            // Return the data
            aIdToADPVit = impl->m_AcqIdentifierToAcqDataPacketVectorMap.find(acqIdentifier);
            if (aIdToADPVit != impl->m_AcqIdentifierToAcqDataPacketVectorMap.cend()) {
                emit dataProvided(acqRequest.m_vIdentifier, acqRequest.m_RangeRequested,
                                  acqRequest.m_CacheRangeRequested, aIdToADPVit->second);
            }

            // Update to the next request
            impl->updateToNextRequest(acqRequest.m_vIdentifier);
        }
    }
    else {
        qCWarning(LOG_VariableAcquisitionWorker())
            << tr("Impossible to retrieve AcquisitionRequest for the incoming data.");
    }
    impl->unlock();
}

void VariableAcquisitionWorker::onExecuteRequest(QUuid acqIdentifier)
{
    qCDebug(LOG_VariableAcquisitionWorker()) << tr("onExecuteRequest") << QThread::currentThread();
    impl->lockRead();
    auto it = impl->m_AcqIdentifierToAcqRequestMap.find(acqIdentifier);
    if (it != impl->m_AcqIdentifierToAcqRequestMap.cend()) {
        auto request = it->second;
        impl->unlock();
        emit variableRequestInProgress(request.m_vIdentifier, 0.1);
        request.m_Provider->requestDataLoading(acqIdentifier, request.m_DataProviderParameters);
    }
    else {
        impl->unlock();
        // TODO log no acqIdentifier recognized
    }
}

void VariableAcquisitionWorker::initialize()
{
    qCDebug(LOG_VariableAcquisitionWorker()) << tr("VariableAcquisitionWorker init")
                                             << QThread::currentThread();
    impl->m_WorkingMutex.lock();
    qCDebug(LOG_VariableAcquisitionWorker()) << tr("VariableAcquisitionWorker init END");
}

void VariableAcquisitionWorker::finalize()
{
    impl->m_WorkingMutex.unlock();
}

void VariableAcquisitionWorker::waitForFinish()
{
    QMutexLocker locker{&impl->m_WorkingMutex};
}

void VariableAcquisitionWorker::VariableAcquisitionWorkerPrivate::removeVariableRequest(
    QUuid vIdentifier)
{
    lockWrite();
    auto it = m_VIdentifierToCurrrentAcqIdNextIdPairMap.find(vIdentifier);

    if (it != m_VIdentifierToCurrrentAcqIdNextIdPairMap.cend()) {
        // A current request already exists, we can replace the next one

        m_AcqIdentifierToAcqRequestMap.erase(it->second.first);
        m_AcqIdentifierToAcqDataPacketVectorMap.erase(it->second.first);

        m_AcqIdentifierToAcqRequestMap.erase(it->second.second);
        m_AcqIdentifierToAcqDataPacketVectorMap.erase(it->second.second);
    }
    m_VIdentifierToCurrrentAcqIdNextIdPairMap.erase(vIdentifier);
    unlock();
}

void VariableAcquisitionWorker::VariableAcquisitionWorkerPrivate::updateToNextRequest(
    QUuid vIdentifier)
{
    auto it = m_VIdentifierToCurrrentAcqIdNextIdPairMap.find(vIdentifier);
    if (it != m_VIdentifierToCurrrentAcqIdNextIdPairMap.cend()) {
        if (it->second.second.isNull()) {
            // There is no next request, we can remove the variable request
            removeVariableRequest(vIdentifier);
        }
        else {
            auto acqIdentifierToRemove = it->second.first;
            // Move the next request to the current request
            it->second.first = it->second.second;
            it->second.second = QUuid();
            // Remove AcquisitionRequest and results;
            m_AcqIdentifierToAcqRequestMap.erase(acqIdentifierToRemove);
            m_AcqIdentifierToAcqDataPacketVectorMap.erase(acqIdentifierToRemove);
            // Execute the current request
            QMetaObject::invokeMethod(q, "onExecuteRequest", Qt::QueuedConnection,
                                      Q_ARG(QUuid, it->second.first));
        }
    }
    else {
        qCCritical(LOG_VariableAcquisitionWorker())
            << tr("Impossible to execute the acquisition on an unfound variable ");
    }
}

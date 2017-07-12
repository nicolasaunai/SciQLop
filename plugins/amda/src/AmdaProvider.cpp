#include "AmdaProvider.h"
#include "AmdaResultParser.h"

#include <Data/DataProviderParameters.h>
#include <Network/NetworkController.h>
#include <SqpApplication.h>
#include <Variable/Variable.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QThread>

Q_LOGGING_CATEGORY(LOG_AmdaProvider, "AmdaProvider")

namespace {

/// URL format for a request on AMDA server. The parameters are as follows:
/// - %1: start date
/// - %2: end date
/// - %3: parameter id
const auto AMDA_URL_FORMAT = QStringLiteral(
    "http://amda.irap.omp.eu/php/rest/"
    "getParameter.php?startTime=%1&stopTime=%2&parameterID=%3&sampling=60&outputFormat=ASCII&"
    "timeFormat=ISO8601&gzip=0");

/// Dates format passed in the URL (e.g 2013-09-23T09:00)
const auto AMDA_TIME_FORMAT = QStringLiteral("yyyy-MM-ddThh:ss");

/// Formats a time to a date that can be passed in URL
QString dateFormat(double sqpDateTime) noexcept
{
    auto dateTime = QDateTime::fromMSecsSinceEpoch(sqpDateTime * 1000.);
    return dateTime.toString(AMDA_TIME_FORMAT);
}

} // namespace

AmdaProvider::AmdaProvider()
{
    qCDebug(LOG_NetworkController()) << tr("AmdaProvider::AmdaProvider")
                                     << QThread::currentThread();
    if (auto app = sqpApp) {
        auto &networkController = app->networkController();
        connect(this, &AmdaProvider::requestConstructed, &networkController,
                &NetworkController::onProcessRequested);
    }
}

void AmdaProvider::requestDataLoading(QUuid token, const DataProviderParameters &parameters)
{
    // NOTE: Try to use multithread if possible
    const auto times = parameters.m_Times;
    for (const auto &dateTime : qAsConst(times)) {
        retrieveData(token, dateTime);
    }
}

void AmdaProvider::retrieveData(QUuid token, const SqpDateTime &dateTime)
{
    // /////////// //
    // Creates URL //
    // /////////// //

    auto startDate = dateFormat(dateTime.m_TStart);
    auto endDate = dateFormat(dateTime.m_TEnd);
    auto productId = QStringLiteral("imf(0)");

    auto url = QUrl{QString{AMDA_URL_FORMAT}.arg(startDate, endDate, productId)};

    auto tempFile = std::make_shared<QTemporaryFile>();

    // LAMBDA
    auto httpDownloadFinished
        = [this, dateTime, tempFile, token](QNetworkReply *reply, QUuid dataId) noexcept {
              Q_UNUSED(dataId);

              if (tempFile) {
                  auto replyReadAll = reply->readAll();
                  if (!replyReadAll.isEmpty()) {
                      tempFile->write(replyReadAll);
                  }
                  tempFile->close();

                  // Parse results file
                  if (auto dataSeries = AmdaResultParser::readTxt(tempFile->fileName())) {
                      emit dataProvided(token, dataSeries, dateTime);
                  }
                  else {
                      /// @todo ALX : debug
                  }
              }

              // Deletes reply
              reply->deleteLater();
              reply = nullptr;
          };
    auto httpFinishedLambda = [this, httpDownloadFinished, tempFile](QNetworkReply *reply,
                                                                     QUuid dataId) noexcept {

        auto downloadFileUrl = QUrl{QString{reply->readAll()}};
        // Deletes old reply
        reply->deleteLater();

        // Executes request for downloading file //

        // Creates destination file
        if (tempFile->open()) {
            // Executes request
            emit requestConstructed(QNetworkRequest{downloadFileUrl}, dataId, httpDownloadFinished);
        }
    };

    // //////////////// //
    // Executes request //
    // //////////////// //
    emit requestConstructed(QNetworkRequest{url}, token, httpFinishedLambda);
}

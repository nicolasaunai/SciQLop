#ifndef SCIQLOP_IDATAPROVIDER_H
#define SCIQLOP_IDATAPROVIDER_H

#include <memory>

#include <QObject>

class DataProviderParameters;
class IDataSeries;

/**
 * @brief The IDataProvider interface aims to declare a data provider.
 *
 * A data provider is an entity that generates data and returns it according to various parameters
 * (time interval, product to retrieve the data, etc.)
 *
 * @sa IDataSeries
 */
class IDataProvider {
public:
    virtual ~IDataProvider() noexcept = default;

    virtual std::unique_ptr<IDataSeries>
    retrieveData(const DataProviderParameters &parameters) const = 0;
};

// Required for using shared_ptr in signals/slots
Q_DECLARE_METATYPE(std::shared_ptr<IDataProvider>)

#endif // SCIQLOP_IDATAPROVIDER_H

#include <MockPlugin.h>

#include <DataSource/DataSourceController.h>
#include <DataSource/DataSourceItem.h>

#include <SqpApplication.h>

Q_LOGGING_CATEGORY(LOG_MockPlugin, "MockPlugin")

namespace {

/// Name of the data source
const auto DATA_SOURCE_NAME = QStringLiteral("MMS");

} // namespace

void MockPlugin::initialize()
{
    if (auto app = sqpApp) {
        // Registers to the data source controller
        auto &dataSourceController = app->dataSourceController();
        auto dataSourceUid = dataSourceController.registerDataSource(DATA_SOURCE_NAME);

        dataSourceController.setDataSourceItem(dataSourceUid, createDataSourceItem());
    }
    else {
        qCWarning(LOG_MockPlugin()) << tr("Can't access to SciQlop application");
    }
}

std::unique_ptr<DataSourceItem> MockPlugin::createDataSourceItem() const noexcept
{
    // Magnetic field products
    auto fgmProduct = std::make_unique<DataSourceItem>(DataSourceItemType::PRODUCT,
                                                       QVector<QVariant>{QStringLiteral("FGM")});
    auto scProduct = std::make_unique<DataSourceItem>(DataSourceItemType::PRODUCT,
                                                      QVector<QVariant>{QStringLiteral("SC")});

    auto magneticFieldFolder = std::make_unique<DataSourceItem>(
        DataSourceItemType::NODE, QVector<QVariant>{QStringLiteral("Magnetic field")});
    magneticFieldFolder->appendChild(std::move(fgmProduct));
    magneticFieldFolder->appendChild(std::move(scProduct));

    // Electric field products
    auto electricFieldFolder = std::make_unique<DataSourceItem>(
        DataSourceItemType::NODE, QVector<QVariant>{QStringLiteral("Electric field")});

    // Root
    auto root = std::make_unique<DataSourceItem>(DataSourceItemType::NODE,
                                                 QVector<QVariant>{DATA_SOURCE_NAME});
    root->appendChild(std::move(magneticFieldFolder));
    root->appendChild(std::move(electricFieldFolder));

    return std::move(root);
}

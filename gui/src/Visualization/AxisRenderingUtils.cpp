#include "Visualization/AxisRenderingUtils.h"

#include <Data/ScalarSeries.h>
#include <Data/SpectrogramSeries.h>
#include <Data/VectorSeries.h>

#include <Visualization/qcustomplot.h>

Q_LOGGING_CATEGORY(LOG_AxisRenderingUtils, "AxisRenderingUtils")

namespace {

const auto DATETIME_FORMAT = QStringLiteral("yyyy/MM/dd hh:mm:ss:zzz");

/// Format for datetimes on a axis
const auto DATETIME_TICKER_FORMAT = QStringLiteral("yyyy/MM/dd \nhh:mm:ss");

/// Generates the appropriate ticker for an axis, depending on whether the axis displays time or
/// non-time data
QSharedPointer<QCPAxisTicker> axisTicker(bool isTimeAxis, QCPAxis::ScaleType scaleType)
{
    if (isTimeAxis) {
        auto dateTicker = QSharedPointer<QCPAxisTickerDateTime>::create();
        dateTicker->setDateTimeFormat(DATETIME_TICKER_FORMAT);
        dateTicker->setDateTimeSpec(Qt::UTC);

        return dateTicker;
    }
    else if (scaleType == QCPAxis::stLogarithmic) {
        return QSharedPointer<QCPAxisTickerLog>::create();
    }
    else {
        // default ticker
        return QSharedPointer<QCPAxisTicker>::create();
    }
}

/**
 * Sets properties of the axis passed as parameter
 * @param axis the axis to set
 * @param unit the unit to set for the axis
 * @param scaleType the scale type to set for the axis
 */
void setAxisProperties(QCPAxis &axis, const Unit &unit,
                       QCPAxis::ScaleType scaleType = QCPAxis::stLinear)
{
    // label (unit name)
    axis.setLabel(unit.m_Name);

    // scale type
    axis.setScaleType(scaleType);
    if (scaleType == QCPAxis::stLogarithmic) {
        // Scientific notation
        axis.setNumberPrecision(0);
        axis.setNumberFormat("eb");
    }

    // ticker (depending on the type of unit)
    axis.setTicker(axisTicker(unit.m_TimeUnit, scaleType));
}

/**
 * Delegate used to set axes properties
 */
template <typename T, typename Enabled = void>
struct AxisSetter {
    static void setProperties(T &, QCustomPlot &, QCPColorScale &)
    {
        // Default implementation does nothing
        qCCritical(LOG_AxisRenderingUtils()) << "Can't set axis properties: unmanaged type of data";
    }
};

/**
 * Specialization of AxisSetter for scalars and vectors
 * @sa ScalarSeries
 * @sa VectorSeries
 */
template <typename T>
struct AxisSetter<T, typename std::enable_if_t<std::is_base_of<ScalarSeries, T>::value
                                               or std::is_base_of<VectorSeries, T>::value> > {
    static void setProperties(T &dataSeries, QCustomPlot &plot, QCPColorScale &)
    {
        dataSeries.lockRead();
        auto xAxisUnit = dataSeries.xAxisUnit();
        auto valuesUnit = dataSeries.valuesUnit();
        dataSeries.unlock();

        setAxisProperties(*plot.xAxis, xAxisUnit);
        setAxisProperties(*plot.yAxis, valuesUnit);
    }
};

/**
 * Specialization of AxisSetter for spectrograms
 * @sa SpectrogramSeries
 */
template <typename T>
struct AxisSetter<T, typename std::enable_if_t<std::is_base_of<SpectrogramSeries, T>::value> > {
    static void setProperties(T &dataSeries, QCustomPlot &plot, QCPColorScale &colorScale)
    {
        dataSeries.lockRead();
        auto xAxisUnit = dataSeries.xAxisUnit();
        /// @todo ALX: use iterators here
        auto yAxisUnit = dataSeries.yAxis().unit();
        auto valuesUnit = dataSeries.valuesUnit();
        dataSeries.unlock();

        setAxisProperties(*plot.xAxis, xAxisUnit);
        setAxisProperties(*plot.yAxis, yAxisUnit, QCPAxis::stLogarithmic);

        // Displays color scale in plot
        plot.plotLayout()->insertRow(0);
        plot.plotLayout()->addElement(0, 0, &colorScale);
        colorScale.setType(QCPAxis::atTop);
        colorScale.setMinimumMargins(QMargins{0, 0, 0, 0});

        // Aligns color scale with axes
        auto marginGroups = plot.axisRect()->marginGroups();
        for (auto it = marginGroups.begin(), end = marginGroups.end(); it != end; ++it) {
            colorScale.setMarginGroup(it.key(), it.value());
        }

        // Set color scale properties
        setAxisProperties(*colorScale.axis(), valuesUnit, QCPAxis::stLogarithmic);
    }
};

/**
 * Default implementation of IAxisHelper, which takes data series to set axes properties
 * @tparam T the data series' type
 */
template <typename T>
struct AxisHelper : public IAxisHelper {
    explicit AxisHelper(T &dataSeries) : m_DataSeries{dataSeries} {}

    void setProperties(QCustomPlot &plot, QCPColorScale &colorScale) override
    {
        AxisSetter<T>::setProperties(m_DataSeries, plot, colorScale);
    }

    T &m_DataSeries;
};

} // namespace

QString formatValue(double value, const QCPAxis &axis)
{
    // If the axis is a time axis, formats the value as a date
    if (auto axisTicker = qSharedPointerDynamicCast<QCPAxisTickerDateTime>(axis.ticker())) {
        return DateUtils::dateTime(value, axisTicker->dateTimeSpec()).toString(DATETIME_FORMAT);
    }
    else {
        return QString::number(value);
    }
}

std::unique_ptr<IAxisHelper>
IAxisHelperFactory::create(std::shared_ptr<IDataSeries> dataSeries) noexcept
{
    if (auto scalarSeries = std::dynamic_pointer_cast<ScalarSeries>(dataSeries)) {
        return std::make_unique<AxisHelper<ScalarSeries> >(*scalarSeries);
    }
    else if (auto spectrogramSeries = std::dynamic_pointer_cast<SpectrogramSeries>(dataSeries)) {
        return std::make_unique<AxisHelper<SpectrogramSeries> >(*spectrogramSeries);
    }
    else if (auto vectorSeries = std::dynamic_pointer_cast<VectorSeries>(dataSeries)) {
        return std::make_unique<AxisHelper<VectorSeries> >(*vectorSeries);
    }
    else {
        return std::make_unique<AxisHelper<IDataSeries> >(*dataSeries);
    }
}